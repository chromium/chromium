// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/terminal/terminal_private_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_refptr.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/extensions/api/terminal/crostini_startup_status.h"
#include "chrome/browser/extensions/api/terminal/terminal_extension_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/terminal_private.h"
#include "chromeos/process_proxy/process_proxy_registry.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/storage/settings_namespace.h"
#include "extensions/browser/api/storage/storage_frontend.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension.h"

namespace terminal_private = extensions::api::terminal_private;
namespace OnTerminalResize =
    extensions::api::terminal_private::OnTerminalResize;
namespace OpenTerminalProcess =
    extensions::api::terminal_private::OpenTerminalProcess;
namespace CloseTerminalProcess =
    extensions::api::terminal_private::CloseTerminalProcess;
namespace SendInput = extensions::api::terminal_private::SendInput;
namespace AckOutput = extensions::api::terminal_private::AckOutput;
namespace SetSettings = extensions::api::terminal_private::SetSettings;

using crostini::mojom::InstallerState;

namespace {

const char kCroshName[] = "crosh";
const char kCroshCommand[] = "/usr/bin/crosh";
// We make stubbed crosh just echo back input.
const char kStubbedCroshCommand[] = "cat";

const char kVmShellName[] = "vmshell";
const char kVmShellCommand[] = "/usr/bin/vsh";

const char kSwitchOwnerId[] = "owner_id";
const char kSwitchVmName[] = "vm_name";
const char kSwitchTargetContainer[] = "target_container";
const char kSwitchStartupId[] = "startup_id";

// Copies the value of |switch_name| if present from |src| to |dst|.  If not
// present, uses |default_value|.  Returns the value set into |dst|.
std::string GetSwitch(const base::CommandLine* src,
                      base::CommandLine* dst,
                      const std::string& switch_name,
                      const std::string& default_value) {
  std::string result = src->HasSwitch(switch_name)
                           ? src->GetSwitchValueASCII(switch_name)
                           : default_value;
  dst->AppendSwitchASCII(switch_name, result);
  return result;
}

void NotifyProcessOutput(content::BrowserContext* browser_context,
                         int tab_id,
                         const std::string& terminal_id,
                         const std::string& output_type,
                         const std::string& output) {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                   base::BindOnce(&NotifyProcessOutput, browser_context, tab_id,
                                  terminal_id, output_type, output));
    return;
  }

  std::unique_ptr<base::ListValue> args(new base::ListValue());
  args->AppendInteger(tab_id);
  args->AppendString(terminal_id);
  args->AppendString(output_type);
  args->AppendString(output);

  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(browser_context);
  if (event_router) {
    std::unique_ptr<extensions::Event> event(new extensions::Event(
        extensions::events::TERMINAL_PRIVATE_ON_PROCESS_OUTPUT,
        terminal_private::OnProcessOutput::kEventName, std::move(args)));
    event_router->BroadcastEvent(std::move(event));
  }
}

// Returns tab ID, or window session ID (for platform apps) for |web_contents|.
int GetTabOrWindowSessionId(content::BrowserContext* browser_context,
                            content::WebContents* web_contents) {
  int tab_id = extensions::ExtensionTabUtil::GetTabId(web_contents);
  if (tab_id >= 0)
    return tab_id;
  extensions::AppWindow* window =
      extensions::AppWindowRegistry::Get(browser_context)
          ->GetAppWindowForWebContents(web_contents);
  return window ? window->session_id().id() : -1;
}

void SettingsChanged(Profile* profile) {
  const base::DictionaryValue* value = profile->GetPrefs()->GetDictionary(
      crostini::prefs::kCrostiniTerminalSettings);

  auto args = std::make_unique<base::ListValue>();
  args->Append(value->CreateDeepCopy());

  extensions::EventRouter* event_router = extensions::EventRouter::Get(profile);
  if (event_router) {
    auto event = std::make_unique<extensions::Event>(
        extensions::events::TERMINAL_PRIVATE_ON_SETTINGS_CHANGED,
        terminal_private::OnSettingsChanged::kEventName, std::move(args));
    event_router->BroadcastEvent(std::move(event));
  }
}

}  // namespace

namespace extensions {

TerminalPrivateAPI::TerminalPrivateAPI(content::BrowserContext* context)
    : context_(context),
      pref_change_registrar_(std::make_unique<PrefChangeRegistrar>()) {
  Profile* profile = Profile::FromBrowserContext(context);
  pref_change_registrar_->Init(profile->GetPrefs());
  pref_change_registrar_->Add(crostini::prefs::kCrostiniTerminalSettings,
                              base::BindRepeating(&SettingsChanged, profile));
}

TerminalPrivateAPI::~TerminalPrivateAPI() = default;

static base::LazyInstance<BrowserContextKeyedAPIFactory<TerminalPrivateAPI>>::
    DestructorAtExit g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<TerminalPrivateAPI>*
TerminalPrivateAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

TerminalPrivateOpenTerminalProcessFunction::
    ~TerminalPrivateOpenTerminalProcessFunction() = default;

ExtensionFunction::ResponseAction
TerminalPrivateOpenTerminalProcessFunction::Run() {
  std::unique_ptr<OpenTerminalProcess::Params> params(
      OpenTerminalProcess::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const std::string& user_id_hash =
      extensions::ExtensionsBrowserClient::Get()->GetUserIdHashFromContext(
          browser_context());
  content::WebContents* caller_contents = GetSenderWebContents();
  if (!caller_contents)
    return RespondNow(Error("No web contents."));

  // Passed to terminalPrivate.ackOutput, which is called from the API's custom
  // bindings after terminalPrivate.onProcessOutput is dispatched. It is used to
  // determine whether ackOutput call should be handled or not. ackOutput will
  // be called from every web contents in which a onProcessOutput listener
  // exists (because the API custom bindings hooks are run in every web contents
  // with a listener). Only ackOutput called from the web contents that has the
  // target terminal instance should be handled.
  // TODO(tbarzic): Instead of passing tab/app window session id around, keep
  //     mapping from web_contents to terminal ID running in it. This will be
  //     needed to fix crbug.com/210295.
  int tab_id = GetTabOrWindowSessionId(browser_context(), caller_contents);
  if (tab_id < 0)
    return RespondNow(Error("Not called from a tab or app window"));

  // Passing --crosh-command overrides any JS process name.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kCroshCommand)) {
    OpenProcess(user_id_hash, tab_id,
                {command_line->GetSwitchValueASCII(switches::kCroshCommand)});

  } else if (params->process_name == kCroshName) {
    // command=crosh: use '/usr/bin/crosh' on a device, 'cat' otherwise.
    if (base::SysInfo::IsRunningOnChromeOS()) {
      OpenProcess(user_id_hash, tab_id, {kCroshCommand});
    } else {
      OpenProcess(user_id_hash, tab_id, {kStubbedCroshCommand});
    }

  } else if (params->process_name == kVmShellName) {
    // command=vmshell: ensure --owner_id, --vm_name, and --target_container are
    // set and the specified vm/container is running.
    base::CommandLine vmshell_cmd({kVmShellCommand});
    std::vector<std::string> args = {kVmShellCommand};
    if (params->args)
      args.insert(args.end(), params->args->begin(), params->args->end());
    base::CommandLine params_args(args);
    std::string owner_id =
        GetSwitch(&params_args, &vmshell_cmd, kSwitchOwnerId, user_id_hash);
    std::string vm_name = GetSwitch(&params_args, &vmshell_cmd, kSwitchVmName,
                                    crostini::kCrostiniDefaultVmName);
    std::string container_name =
        GetSwitch(&params_args, &vmshell_cmd, kSwitchTargetContainer,
                  crostini::kCrostiniDefaultContainerName);
    std::string startup_id = params_args.GetSwitchValueASCII(kSwitchStartupId);

    auto open_process =
        base::BindOnce(&TerminalPrivateOpenTerminalProcessFunction::OpenProcess,
                       this, user_id_hash, tab_id, vmshell_cmd.argv());
    auto* mgr = crostini::CrostiniManager::GetForProfile(
        Profile::FromBrowserContext(browser_context()));
    bool verbose =
        !mgr->GetContainerInfo(crostini::kCrostiniDefaultVmName,
                               crostini::kCrostiniDefaultContainerName)
             .has_value();
    auto* observer = new CrostiniStartupStatus(
        base::BindRepeating(&NotifyProcessOutput, browser_context(), tab_id,
                            startup_id,
                            api::terminal_private::ToString(
                                api::terminal_private::OUTPUT_TYPE_STDOUT)),
        verbose, std::move(open_process));
    observer->ShowStatusLineAtInterval();
    mgr->RestartCrostini(
        vm_name, container_name,
        base::BindOnce(&CrostiniStartupStatus::OnCrostiniRestarted,
                       base::Unretained(observer)),
        observer);
  } else {
    // command=[unrecognized].
    return RespondNow(Error("Invalid process name: " + params->process_name));
  }
  return RespondLater();
}

void TerminalPrivateOpenTerminalProcessFunction::OpenProcess(
    const std::string& user_id_hash,
    int tab_id,
    const std::vector<std::string>& arguments) {
  DCHECK(!arguments.empty());
  // Registry lives on its own task runner.
  chromeos::ProcessProxyRegistry::GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &TerminalPrivateOpenTerminalProcessFunction::OpenOnRegistryTaskRunner,
          this, base::Bind(&NotifyProcessOutput, browser_context(), tab_id),
          base::Bind(
              &TerminalPrivateOpenTerminalProcessFunction::RespondOnUIThread,
              this),
          arguments, user_id_hash));
}

void TerminalPrivateOpenTerminalProcessFunction::OpenOnRegistryTaskRunner(
    const ProcessOutputCallback& output_callback,
    const OpenProcessCallback& callback,
    const std::vector<std::string>& arguments,
    const std::string& user_id_hash) {
  chromeos::ProcessProxyRegistry* registry =
      chromeos::ProcessProxyRegistry::Get();
  const base::CommandLine cmdline{arguments};

  std::string terminal_id;
  bool success = registry->OpenProcess(cmdline, user_id_hash, output_callback,
                                       &terminal_id);

  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(callback, success, terminal_id));
}

TerminalPrivateSendInputFunction::~TerminalPrivateSendInputFunction() = default;

void TerminalPrivateOpenTerminalProcessFunction::RespondOnUIThread(
    bool success,
    const std::string& terminal_id) {
  if (!success) {
    Respond(Error("Failed to open process."));
    return;
  }
  Respond(OneArgument(std::make_unique<base::Value>(terminal_id)));
}

ExtensionFunction::ResponseAction TerminalPrivateSendInputFunction::Run() {
  std::unique_ptr<SendInput::Params> params(SendInput::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Registry lives on its own task runner.
  chromeos::ProcessProxyRegistry::GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &TerminalPrivateSendInputFunction::SendInputOnRegistryTaskRunner,
          this, params->id, params->input));
  return RespondLater();
}

void TerminalPrivateSendInputFunction::SendInputOnRegistryTaskRunner(
    const std::string& terminal_id,
    const std::string& text) {
  bool success =
      chromeos::ProcessProxyRegistry::Get()->SendInput(terminal_id, text);

  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&TerminalPrivateSendInputFunction::RespondOnUIThread, this,
                     success));
}

void TerminalPrivateSendInputFunction::RespondOnUIThread(bool success) {
  Respond(OneArgument(std::make_unique<base::Value>(success)));
}

TerminalPrivateCloseTerminalProcessFunction::
    ~TerminalPrivateCloseTerminalProcessFunction() = default;

ExtensionFunction::ResponseAction
TerminalPrivateCloseTerminalProcessFunction::Run() {
  std::unique_ptr<CloseTerminalProcess::Params> params(
      CloseTerminalProcess::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Registry lives on its own task runner.
  chromeos::ProcessProxyRegistry::GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&TerminalPrivateCloseTerminalProcessFunction::
                                    CloseOnRegistryTaskRunner,
                                this, params->id));

  return RespondLater();
}

void TerminalPrivateCloseTerminalProcessFunction::CloseOnRegistryTaskRunner(
    const std::string& terminal_id) {
  bool success =
      chromeos::ProcessProxyRegistry::Get()->CloseProcess(terminal_id);

  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          &TerminalPrivateCloseTerminalProcessFunction::RespondOnUIThread, this,
          success));
}

void TerminalPrivateCloseTerminalProcessFunction::RespondOnUIThread(
    bool success) {
  Respond(OneArgument(std::make_unique<base::Value>(success)));
}

TerminalPrivateOnTerminalResizeFunction::
    ~TerminalPrivateOnTerminalResizeFunction() = default;

ExtensionFunction::ResponseAction
TerminalPrivateOnTerminalResizeFunction::Run() {
  std::unique_ptr<OnTerminalResize::Params> params(
      OnTerminalResize::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Registry lives on its own task runner.
  chromeos::ProcessProxyRegistry::GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&TerminalPrivateOnTerminalResizeFunction::
                         OnResizeOnRegistryTaskRunner,
                     this, params->id, params->width, params->height));

  return RespondLater();
}

void TerminalPrivateOnTerminalResizeFunction::OnResizeOnRegistryTaskRunner(
    const std::string& terminal_id,
    int width,
    int height) {
  bool success = chromeos::ProcessProxyRegistry::Get()->OnTerminalResize(
      terminal_id, width, height);

  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          &TerminalPrivateOnTerminalResizeFunction::RespondOnUIThread, this,
          success));
}

void TerminalPrivateOnTerminalResizeFunction::RespondOnUIThread(bool success) {
  Respond(OneArgument(std::make_unique<base::Value>(success)));
}

TerminalPrivateAckOutputFunction::~TerminalPrivateAckOutputFunction() = default;

ExtensionFunction::ResponseAction TerminalPrivateAckOutputFunction::Run() {
  std::unique_ptr<AckOutput::Params> params(AckOutput::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  content::WebContents* caller_contents = GetSenderWebContents();
  if (!caller_contents)
    return RespondNow(Error("No web contents."));

  int tab_id = GetTabOrWindowSessionId(browser_context(), caller_contents);
  if (tab_id < 0)
    return RespondNow(Error("Not called from a tab or app window"));

  if (tab_id != params->tab_id)
    return RespondNow(NoArguments());

  // Registry lives on its own task runner.
  chromeos::ProcessProxyRegistry::GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &TerminalPrivateAckOutputFunction::AckOutputOnRegistryTaskRunner,
          this, params->id));

  return RespondNow(NoArguments());
}

void TerminalPrivateAckOutputFunction::AckOutputOnRegistryTaskRunner(
    const std::string& terminal_id) {
  chromeos::ProcessProxyRegistry::Get()->AckOutput(terminal_id);
}

TerminalPrivateGetCroshSettingsFunction::
    ~TerminalPrivateGetCroshSettingsFunction() = default;

ExtensionFunction::ResponseAction
TerminalPrivateGetCroshSettingsFunction::Run() {
  const Extension* crosh_extension =
      TerminalExtensionHelper::GetTerminalExtension(
          Profile::FromBrowserContext(browser_context()));
  StorageFrontend* frontend = StorageFrontend::Get(browser_context());
  frontend->RunWithStorage(
      crosh_extension, settings_namespace::SYNC,
      base::Bind(&TerminalPrivateGetCroshSettingsFunction::AsyncRunWithStorage,
                 this));
  return RespondLater();
}

void TerminalPrivateGetCroshSettingsFunction::AsyncRunWithStorage(
    ValueStore* storage) {
  ValueStore::ReadResult result = storage->Get();
  ExtensionFunction::ResponseValue response =
      result.status().ok() ? OneArgument(result.PassSettings())
                           : Error(result.status().message);
  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&TerminalPrivateGetCroshSettingsFunction::Respond, this,
                     std::move(response)));
}

TerminalPrivateGetSettingsFunction::~TerminalPrivateGetSettingsFunction() =
    default;

ExtensionFunction::ResponseAction TerminalPrivateGetSettingsFunction::Run() {
  PrefService* service =
      Profile::FromBrowserContext(browser_context())->GetPrefs();
  const base::DictionaryValue* value =
      service->GetDictionary(crostini::prefs::kCrostiniTerminalSettings);
  return RespondNow(OneArgument(value->CreateDeepCopy()));
}

TerminalPrivateSetSettingsFunction::~TerminalPrivateSetSettingsFunction() =
    default;

ExtensionFunction::ResponseAction TerminalPrivateSetSettingsFunction::Run() {
  std::unique_ptr<SetSettings::Params> params(
      SetSettings::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  PrefService* service =
      Profile::FromBrowserContext(browser_context())->GetPrefs();
  service->Set(crostini::prefs::kCrostiniTerminalSettings,
               params->settings.additional_properties);
  return RespondNow(NoArguments());
}

}  // namespace extensions
