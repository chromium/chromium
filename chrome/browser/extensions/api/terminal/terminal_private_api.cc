// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/terminal/terminal_private_api.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/ash_pref_names.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_terminal.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/extensions/api/terminal/crostini_startup_status.h"
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
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extensions_browser_client.h"

namespace terminal_private = extensions::api::terminal_private;
namespace OnTerminalResize =
    extensions::api::terminal_private::OnTerminalResize;
namespace OpenTerminalProcess =
    extensions::api::terminal_private::OpenTerminalProcess;
namespace OpenVmshellProcess =
    extensions::api::terminal_private::OpenVmshellProcess;
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
const char kSwitchCurrentWorkingDir[] = "cwd";

const char kCwdTerminalIdPrefix[] = "terminal_id:";

class TerminalTabHelper
    : public content::WebContentsUserData<TerminalTabHelper> {
 public:
  void AddTerminalId(const std::string& terminal_id) {
    if (!terminal_ids_.insert(terminal_id).second) {
      LOG(ERROR) << "terminal id already exists" << terminal_id;
    }
  }

  void RemoveTerminalId(const std::string& terminal_id) {
    if (terminal_ids_.erase(terminal_id) == 0) {
      LOG(ERROR) << "terminal id does not exists" << terminal_id;
    }
  }

  static bool ValidateTerminalId(const content::WebContents* contents,
                                 const std::string& terminal_id) {
    if (contents != nullptr) {
      auto* helper = TerminalTabHelper::FromWebContents(contents);
      if (helper != nullptr) {
        return helper->terminal_ids_.contains(terminal_id);
      }
    }
    return false;
  }

 private:
  explicit TerminalTabHelper(content::WebContents* contents) {}

  friend class content::WebContentsUserData<TerminalTabHelper>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  base::flat_set<std::string> terminal_ids_;
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(TerminalTabHelper)

// Copies the value of |switch_name| if present from |src| to |dst|.  If not
// present, uses |default_value| if nonempty.  Returns the value set into |dst|.
std::string GetSwitch(const base::CommandLine& src,
                      base::CommandLine* dst,
                      const std::string& switch_name,
                      const std::string& default_value) {
  std::string result = src.HasSwitch(switch_name)
                           ? src.GetSwitchValueASCII(switch_name)
                           : default_value;
  if (!result.empty()) {
    dst->AppendSwitchASCII(switch_name, result);
  }
  return result;
}

void NotifyProcessOutput(content::BrowserContext* browser_context,
                         const std::string& terminal_id,
                         const std::string& output_type,
                         const std::string& output) {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&NotifyProcessOutput, browser_context,
                                  terminal_id, output_type, output));
    return;
  }

  std::unique_ptr<base::ListValue> args(new base::ListValue());
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

void PreferenceChanged(Profile* profile,
                       const std::string& pref_name,
                       extensions::events::HistogramValue histogram,
                       const char* eventName) {
  auto args = std::make_unique<base::ListValue>();
  args->Append(profile->GetPrefs()->Get(pref_name)->CreateDeepCopy());
  extensions::EventRouter* event_router = extensions::EventRouter::Get(profile);
  if (event_router) {
    auto event = std::make_unique<extensions::Event>(histogram, eventName,
                                                     std::move(args));
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
  pref_change_registrar_->Add(
      crostini::prefs::kCrostiniTerminalSettings,
      base::BindRepeating(
          &PreferenceChanged, profile,
          crostini::prefs::kCrostiniTerminalSettings,
          extensions::events::TERMINAL_PRIVATE_ON_SETTINGS_CHANGED,
          terminal_private::OnSettingsChanged::kEventName));
  pref_change_registrar_->Add(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled,
      base::BindRepeating(
          &PreferenceChanged, profile,
          ash::prefs::kAccessibilitySpokenFeedbackEnabled,
          extensions::events::TERMINAL_PRIVATE_ON_A11Y_STATUS_CHANGED,
          terminal_private::OnA11yStatusChanged::kEventName));
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

  return OpenProcess(params->process_name, std::move(params->args));
}

ExtensionFunction::ResponseAction
TerminalPrivateOpenTerminalProcessFunction::OpenProcess(
    const std::string& process_name,
    std::unique_ptr<std::vector<std::string>> args) {
  const std::string& user_id_hash =
      extensions::ExtensionsBrowserClient::Get()->GetUserIdHashFromContext(
          browser_context());
  content::WebContents* caller_contents = GetSenderWebContents();
  if (!caller_contents)
    return RespondNow(Error("No web contents."));

  // Passing --crosh-command overrides any JS process name.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kCroshCommand)) {
    OpenProcess(
        user_id_hash,
        base::CommandLine(base::FilePath(
            command_line->GetSwitchValueASCII(switches::kCroshCommand))));

  } else if (process_name == kCroshName) {
    // command=crosh: use '/usr/bin/crosh' on a device, 'cat' otherwise.
    if (base::SysInfo::IsRunningOnChromeOS()) {
      OpenProcess(user_id_hash,
                  base::CommandLine(base::FilePath(kCroshCommand)));
    } else {
      OpenProcess(user_id_hash,
                  base::CommandLine(base::FilePath(kStubbedCroshCommand)));
    }

  } else if (process_name == kVmShellName) {
    // Ensure crostini is allowed before starting terminal.
    Profile* profile = Profile::FromBrowserContext(browser_context());
    if (!crostini::CrostiniFeatures::Get()->IsAllowedNow(profile))
      return RespondNow(Error("vmshell not allowed"));

    // command=vmshell: ensure --owner_id, --vm_name, --target_container, --cwd
    // are set, and the specified vm/container is running.
    base::CommandLine cmdline((base::FilePath(kVmShellCommand)));
    if (!args)
      args = std::make_unique<std::vector<std::string>>();
    args->insert(args->begin(), kVmShellCommand);
    base::CommandLine params_args(*args);
    std::string owner_id =
        GetSwitch(params_args, &cmdline, kSwitchOwnerId, user_id_hash);
    std::string vm_name = GetSwitch(params_args, &cmdline, kSwitchVmName,
                                    crostini::kCrostiniDefaultVmName);
    std::string container_name =
        GetSwitch(params_args, &cmdline, kSwitchTargetContainer,
                  crostini::kCrostiniDefaultContainerName);
    GetSwitch(params_args, &cmdline, kSwitchCurrentWorkingDir, "");
    std::string startup_id = params_args.GetSwitchValueASCII(kSwitchStartupId);
    crostini::ContainerId container_id(vm_name, container_name);

    auto* mgr = crostini::CrostiniManager::GetForProfile(profile);
    bool verbose = !mgr->GetContainerInfo(container_id).has_value();
    auto observer = std::make_unique<CrostiniStartupStatus>(
        base::BindRepeating(&NotifyProcessOutput, browser_context(), startup_id,
                            api::terminal_private::ToString(
                                api::terminal_private::OUTPUT_TYPE_STDOUT)),
        verbose);
    // Save copy of pointer for RestartObserver before moving object.
    CrostiniStartupStatus* observer_ptr = observer.get();
    observer->ShowProgressAtInterval();
    mgr->RestartCrostini(
        container_id,
        base::BindOnce(
            &TerminalPrivateOpenTerminalProcessFunction::OnCrostiniRestarted,
            this, std::move(observer), user_id_hash, std::move(cmdline)),
        observer_ptr);
  } else {
    // command=[unrecognized].
    return RespondNow(Error("Invalid process name: " + process_name));
  }
  return RespondLater();
}

void TerminalPrivateOpenTerminalProcessFunction::OnCrostiniRestarted(
    std::unique_ptr<CrostiniStartupStatus> startup_status,
    const std::string& user_id_hash,
    base::CommandLine cmdline,
    crostini::CrostiniResult result) {
  if (crostini::MaybeShowCrostiniDialogBeforeLaunch(
          Profile::FromBrowserContext(browser_context()), result)) {
    const std::string msg = "Waiting for component update dialog response";
    LOG(ERROR) << msg;
    Respond(Error(msg));
    return;
  }
  startup_status->OnCrostiniRestarted(result);
  if (result == crostini::CrostiniResult::SUCCESS) {
    OpenVmshellProcess(user_id_hash, std::move(cmdline));
  } else {
    const std::string msg =
        base::StringPrintf("Error starting crostini for terminal: %d", result);
    LOG(ERROR) << msg;
    Respond(Error(msg));
  }
}

void TerminalPrivateOpenTerminalProcessFunction::OpenVmshellProcess(
    const std::string& user_id_hash,
    base::CommandLine cmdline) {
  const std::string cwd = cmdline.GetSwitchValueASCII(kSwitchCurrentWorkingDir);

  if (!base::StartsWith(cwd, kCwdTerminalIdPrefix)) {
    return OpenProcess(user_id_hash, std::move(cmdline));
  }

  // The cwd has this format `terminal_id:<terminal_id>`. We need to convert the
  // terminal id to the pid of the shell process inside the container.
  int host_pid = chromeos::ProcessProxyRegistry::ConvertToSystemPID(
      cwd.substr(sizeof(kCwdTerminalIdPrefix) - 1));

  // Lookup container shell pid from cicierone to use for cwd.
  crostini::CrostiniManager::GetForProfile(
      Profile::FromBrowserContext(browser_context()))
      ->GetVshSession(
          crostini::ContainerId::GetDefault(), host_pid,
          base::BindOnce(
              &TerminalPrivateOpenTerminalProcessFunction::OnGetVshSession,
              this, user_id_hash, std::move(cmdline), /*terminal_id=*/cwd));
}

void TerminalPrivateOpenTerminalProcessFunction::OnGetVshSession(
    const std::string& user_id_hash,
    base::CommandLine cmdline,
    const std::string& terminal_id,
    bool success,
    const std::string& failure_reason,
    int32_t container_shell_pid) {
  if (!success) {
    LOG(WARNING) << "Failed to get vsh session for " << terminal_id << ". "
                 << failure_reason;
  } else {
    cmdline.AppendSwitchASCII(kSwitchCurrentWorkingDir,
                              base::NumberToString(container_shell_pid));
  }
  OpenProcess(user_id_hash, std::move(cmdline));
}

void TerminalPrivateOpenTerminalProcessFunction::OpenProcess(
    const std::string& user_id_hash,
    base::CommandLine cmdline) {
  DCHECK(!cmdline.argv().empty());
  // Registry lives on its own task runner.
  chromeos::ProcessProxyRegistry::GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &TerminalPrivateOpenTerminalProcessFunction::OpenOnRegistryTaskRunner,
          this, base::BindRepeating(&NotifyProcessOutput, browser_context()),
          base::BindOnce(
              &TerminalPrivateOpenTerminalProcessFunction::RespondOnUIThread,
              this),
          std::move(cmdline), user_id_hash));
}

void TerminalPrivateOpenTerminalProcessFunction::OpenOnRegistryTaskRunner(
    ProcessOutputCallback output_callback,
    OpenProcessCallback callback,
    base::CommandLine cmdline,
    const std::string& user_id_hash) {
  chromeos::ProcessProxyRegistry* registry =
      chromeos::ProcessProxyRegistry::Get();
  std::string terminal_id;
  bool success =
      registry->OpenProcess(std::move(cmdline), user_id_hash,
                            std::move(output_callback), &terminal_id);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), success, terminal_id));
}

void TerminalPrivateOpenTerminalProcessFunction::RespondOnUIThread(
    bool success,
    const std::string& terminal_id) {
  auto* contents = GetSenderWebContents();
  if (!contents) {
    LOG(WARNING) << "content is closed before returning opened process";
    chromeos::ProcessProxyRegistry::GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](const std::string& terminal_id) {
              if (!chromeos::ProcessProxyRegistry::Get()->CloseProcess(
                      terminal_id)) {
                LOG(ERROR) << "Unable to close terminal " << terminal_id;
              }
            },
            terminal_id));
    return;
  }

  if (!success) {
    Respond(Error("Failed to open process."));
    return;
  }
  Respond(OneArgument(base::Value(terminal_id)));

  TerminalTabHelper::CreateForWebContents(contents);
  TerminalTabHelper::FromWebContents(contents)->AddTerminalId(terminal_id);
}

TerminalPrivateOpenVmshellProcessFunction::
    ~TerminalPrivateOpenVmshellProcessFunction() = default;

ExtensionFunction::ResponseAction
TerminalPrivateOpenVmshellProcessFunction::Run() {
  std::unique_ptr<OpenVmshellProcess::Params> params(
      OpenVmshellProcess::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Only opens 'vmshell'.
  return OpenProcess(kVmShellName, std::move(params->args));
}

TerminalPrivateSendInputFunction::~TerminalPrivateSendInputFunction() = default;

ExtensionFunction::ResponseAction TerminalPrivateSendInputFunction::Run() {
  std::unique_ptr<SendInput::Params> params(SendInput::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (!TerminalTabHelper::ValidateTerminalId(GetSenderWebContents(),
                                             params->id)) {
    LOG(ERROR) << "invalid terminal id " << params->id;
    return RespondNow(Error("invalid terminal id"));
  }


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

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&TerminalPrivateSendInputFunction::RespondOnUIThread, this,
                     success));
}

void TerminalPrivateSendInputFunction::RespondOnUIThread(bool success) {
  Respond(OneArgument(base::Value(success)));
}

TerminalPrivateCloseTerminalProcessFunction::
    ~TerminalPrivateCloseTerminalProcessFunction() = default;

ExtensionFunction::ResponseAction
TerminalPrivateCloseTerminalProcessFunction::Run() {
  std::unique_ptr<CloseTerminalProcess::Params> params(
      CloseTerminalProcess::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (!TerminalTabHelper::ValidateTerminalId(GetSenderWebContents(),
                                             params->id)) {
    LOG(ERROR) << "invalid terminal id " << params->id;
    return RespondNow(Error("invalid terminal id"));
  }
  TerminalTabHelper::FromWebContents(GetSenderWebContents())
      ->RemoveTerminalId(params->id);

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

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &TerminalPrivateCloseTerminalProcessFunction::RespondOnUIThread, this,
          success));
}

void TerminalPrivateCloseTerminalProcessFunction::RespondOnUIThread(
    bool success) {
  Respond(OneArgument(base::Value(success)));
}

TerminalPrivateOnTerminalResizeFunction::
    ~TerminalPrivateOnTerminalResizeFunction() = default;

ExtensionFunction::ResponseAction
TerminalPrivateOnTerminalResizeFunction::Run() {
  std::unique_ptr<OnTerminalResize::Params> params(
      OnTerminalResize::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (!TerminalTabHelper::ValidateTerminalId(GetSenderWebContents(),
                                             params->id)) {
    LOG(ERROR) << "invalid terminal id " << params->id;
    return RespondNow(Error("invalid terminal id"));
  }

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

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &TerminalPrivateOnTerminalResizeFunction::RespondOnUIThread, this,
          success));
}

void TerminalPrivateOnTerminalResizeFunction::RespondOnUIThread(bool success) {
  Respond(OneArgument(base::Value(success)));
}

TerminalPrivateAckOutputFunction::~TerminalPrivateAckOutputFunction() = default;

ExtensionFunction::ResponseAction TerminalPrivateAckOutputFunction::Run() {
  std::unique_ptr<AckOutput::Params> params(AckOutput::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Every running terminal page will call ackOutput(), but we should only react
  // for the one who actually owns the output.
  if (TerminalTabHelper::ValidateTerminalId(GetSenderWebContents(),
                                            params->id)) {
    // Registry lives on its own task runner.
    chromeos::ProcessProxyRegistry::GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &TerminalPrivateAckOutputFunction::AckOutputOnRegistryTaskRunner,
            this, params->id));
  }

  return RespondNow(NoArguments());
}

void TerminalPrivateAckOutputFunction::AckOutputOnRegistryTaskRunner(
    const std::string& terminal_id) {
  chromeos::ProcessProxyRegistry::Get()->AckOutput(terminal_id);
}

TerminalPrivateOpenWindowFunction::~TerminalPrivateOpenWindowFunction() =
    default;

ExtensionFunction::ResponseAction TerminalPrivateOpenWindowFunction::Run() {
  crostini::LaunchTerminal(Profile::FromBrowserContext(browser_context()));
  return RespondNow(NoArguments());
}

TerminalPrivateOpenOptionsPageFunction::
    ~TerminalPrivateOpenOptionsPageFunction() = default;

ExtensionFunction::ResponseAction
TerminalPrivateOpenOptionsPageFunction::Run() {
  crostini::LaunchTerminalSettings(
      Profile::FromBrowserContext(browser_context()));
  return RespondNow(NoArguments());
}

TerminalPrivateGetSettingsFunction::~TerminalPrivateGetSettingsFunction() =
    default;

ExtensionFunction::ResponseAction TerminalPrivateGetSettingsFunction::Run() {
  crostini::RecordTerminalSettingsChangesUMAs(
      Profile::FromBrowserContext(browser_context()));
  PrefService* service =
      Profile::FromBrowserContext(browser_context())->GetPrefs();
  const base::DictionaryValue* value =
      service->GetDictionary(crostini::prefs::kCrostiniTerminalSettings);
  return RespondNow(OneArgument(value->Clone()));
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

TerminalPrivateGetA11yStatusFunction::~TerminalPrivateGetA11yStatusFunction() =
    default;

ExtensionFunction::ResponseAction TerminalPrivateGetA11yStatusFunction::Run() {
  return RespondNow(
      OneArgument(Profile::FromBrowserContext(browser_context())
                      ->GetPrefs()
                      ->Get(ash::prefs::kAccessibilitySpokenFeedbackEnabled)
                      ->Clone()));
}

}  // namespace extensions
