// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/extensions/api/terminal/terminal_private_api.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service_factory.h"
#include "chrome/browser/ash/guest_os/public/guest_os_terminal_provider.h"
#include "chrome/browser/ash/guest_os/public/guest_os_terminal_provider_registry.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/browser/ash/guest_os/virtual_machines/virtual_machines_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/terminal/startup_status.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/policy/system_features_disable_list_policy_handler.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/terminal_private.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
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
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/constants.h"
#include "ui/display/types/display_constants.h"

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
namespace OpenWindow = extensions::api::terminal_private::OpenWindow;
namespace GetPrefs = extensions::api::terminal_private::GetPrefs;
namespace SetPrefs = extensions::api::terminal_private::SetPrefs;

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
const char kSwitchContainerFeatures[] = "container_features";

const char kCwdTerminalIdPrefix[] = "terminal_id:";

// Prefs that we read and observe.
static const base::NoDestructor<std::vector<std::string>> kPrefsReadAllowList{{
    ash::prefs::kAccessibilitySpokenFeedbackEnabled,
    crostini::prefs::kCrostiniEnabled,
    guest_os::prefs::kGuestOsTerminalSettings,
    crostini::prefs::kTerminalSshAllowedByPolicy,
    guest_os::prefs::kGuestOsContainers,
}};

void CloseTerminal(const std::string& terminal_id,
                   base::OnceCallback<void(bool)> callback) {
  chromeos::ProcessProxyRegistry::GetTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](const std::string& terminal_id) {
            return chromeos::ProcessProxyRegistry::Get()->CloseProcess(
                terminal_id);
          },
          terminal_id),
      std::move(callback));
}

class TerminalTabHelper
    : public content::WebContentsUserData<TerminalTabHelper> {
 public:
  ~TerminalTabHelper() override {
    // The web contents object is being destructed. We should close all
    // terminals that haven't been closed already. This can happen when the JS
    // code didn't have a chance to do that (e.g. memory stress causes the web
    // contents to be killed directly).
    for (const std::string& terminal_id : terminal_ids_) {
      CloseTerminal(terminal_id, base::DoNothing());
    }
  }

  void AddTerminalId(const std::string& terminal_id) {
    if (!terminal_ids_.insert(terminal_id).second) {
      LOG(ERROR) << "Terminal id already exists: " << terminal_id;
    }
  }

  void RemoveTerminalId(const std::string& terminal_id) {
    if (terminal_ids_.erase(terminal_id) == 0) {
      LOG(ERROR) << "Terminal id does not exist: " << terminal_id;
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
  explicit TerminalTabHelper(content::WebContents* contents)
      : content::WebContentsUserData<TerminalTabHelper>(*contents) {}

  friend class content::WebContentsUserData<TerminalTabHelper>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  base::flat_set<std::string> terminal_ids_;
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(TerminalTabHelper);

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

std::string GetContainerFeaturesArg() {
  std::string result;
  // There are only a few available features so concatenating strings is
  // sufficient.
  for (vm_tools::cicerone::ContainerFeature feature :
       crostini::GetContainerFeatures()) {
    if (!result.empty())
      result += ",";
    result += base::NumberToString(static_cast<int>(feature));
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

  base::Value::List args;
  args.Append(terminal_id);
  args.Append(output_type);
  args.Append(base::Value(base::make_span(
      reinterpret_cast<const uint8_t*>(&output[0]), output.size())));

  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(browser_context);
  if (event_router) {
    std::unique_ptr<extensions::Event> event(new extensions::Event(
        extensions::events::TERMINAL_PRIVATE_ON_PROCESS_OUTPUT,
        terminal_private::OnProcessOutput::kEventName, std::move(args)));
    event_router->BroadcastEvent(std::move(event));
  }
}

void PrefChanged(Profile* profile, const std::string& pref_name) {
  extensions::EventRouter* event_router = extensions::EventRouter::Get(profile);
  if (!event_router) {
    return;
  }
  base::Value::List args;
  base::Value::Dict prefs;
  prefs.Set(pref_name, profile->GetPrefs()->GetValue(pref_name).Clone());
  args.Append(std::move(prefs));
  auto event = std::make_unique<extensions::Event>(
      extensions::events::TERMINAL_PRIVATE_ON_PREF_CHANGED,
      terminal_private::OnPrefChanged::kEventName, std::move(args));
  event_router->BroadcastEvent(std::move(event));
}

}  // namespace

namespace extensions {

TerminalPrivateAPI::TerminalPrivateAPI(content::BrowserContext* context)
    : context_(context),
      pref_change_registrar_(std::make_unique<PrefChangeRegistrar>()) {
  Profile* profile = Profile::FromBrowserContext(context);
  pref_change_registrar_->Init(profile->GetPrefs());
  for (const auto& pref : *kPrefsReadAllowList) {
    pref_change_registrar_->Add(pref,
                                base::BindRepeating(&PrefChanged, profile));
  }
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
    TerminalPrivateOpenTerminalProcessFunction() = default;

TerminalPrivateOpenTerminalProcessFunction::
    ~TerminalPrivateOpenTerminalProcessFunction() = default;

ExtensionFunction::ResponseAction
TerminalPrivateOpenTerminalProcessFunction::Run() {
  std::optional<OpenTerminalProcess::Params> params =
      OpenTerminalProcess::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  return OpenProcess(params->process_name, std::move(params->args));
}

ExtensionFunction::ResponseAction
TerminalPrivateOpenTerminalProcessFunction::OpenProcess(
    const std::string& process_name,
    std::optional<std::vector<std::string>> args) {
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
    // Ensure crosh is allowed before starting terminal.
    if (policy::SystemFeaturesDisableListPolicyHandler::IsSystemFeatureDisabled(
            policy::SystemFeature::kCrosh, g_browser_process->local_state())) {
      return RespondNow(Error("crosh not allowed"));
    }

    // command=crosh: use '/usr/bin/crosh' on a device, 'cat' otherwise.
    if (base::SysInfo::IsRunningOnChromeOS()) {
      OpenProcess(user_id_hash,
                  base::CommandLine(base::FilePath(kCroshCommand)));
    } else {
      OpenProcess(user_id_hash,
                  base::CommandLine(base::FilePath(kStubbedCroshCommand)));
    }
  } else if (process_name == kVmShellName) {
    // Ensure vms are allowed before starting terminal.
    if (!virtual_machines::AreVirtualMachinesAllowedByPolicy()) {
      return RespondNow(Error("vmshell not allowed"));
    }
    // command=vmshell: ensure --owner_id, --vm_name, --target_container, --cwd
    // are set, and the specified vm/container is running.
    base::CommandLine cmdline((base::FilePath(kVmShellCommand)));
    if (!args)
      args.emplace();
    args->insert(args->begin(), kVmShellCommand);
    base::CommandLine params_args(*args);
    VLOG(1) << "Original cmdline= " << params_args.GetCommandLineString();
    std::string owner_id =
        GetSwitch(params_args, &cmdline, kSwitchOwnerId, user_id_hash);
    std::string vm_name = GetSwitch(params_args, &cmdline, kSwitchVmName,
                                    crostini::kCrostiniDefaultVmName);
    std::string container_name =
        GetSwitch(params_args, &cmdline, kSwitchTargetContainer, "");
    GetSwitch(params_args, &cmdline, kSwitchCurrentWorkingDir, "");
    std::string startup_id = params_args.GetSwitchValueASCII(kSwitchStartupId);
    guest_id_ = std::make_unique<guest_os::GuestId>(guest_os::VmType::UNKNOWN,
                                                    vm_name, container_name);

    // Unlike the other switches, this is computed here directly rather than
    // taken from |args|.
    std::string container_features = GetContainerFeaturesArg();
    if (!container_features.empty())
      cmdline.AppendSwitchASCII(kSwitchContainerFeatures, container_features);

    // Append trailing passthrough args if any.  E.g. `-- vim file.txt`
    auto passthrough_args = params_args.GetArgs();
    if (!passthrough_args.empty()) {
      cmdline.AppendArg("--");
      for (const auto& arg : passthrough_args) {
        cmdline.AppendArg(arg);
      }
    }
    VLOG(1) << "Starting " << *guest_id_
            << ", cmdline=" << cmdline.GetCommandLineString();

    Profile* profile = Profile::FromBrowserContext(browser_context());
    auto* service = guest_os::GuestOsServiceFactory::GetForProfile(profile);
    guest_os::GuestOsTerminalProvider* provider = nullptr;
    if (service) {
      provider = service->TerminalProviderRegistry()->Get(*guest_id_);
    }
    auto* tracker =
        guest_os::GuestOsSessionTrackerFactory::GetForProfile(profile);
    bool verbose = !(tracker && tracker->GetInfo(*guest_id_).has_value());
    auto status_printer = std::make_unique<StartupStatusPrinter>(
        base::BindRepeating(&NotifyProcessOutput, browser_context(),
                            std::move(startup_id),
                            api::terminal_private::ToString(
                                api::terminal_private::OutputType::kStdout)),
        verbose);
    if (provider) {
      startup_status_ =
          provider->CreateStartupStatus(std::move(status_printer));
      startup_status_->StartShowingSpinner();
      provider->EnsureRunning(
          startup_status_.get(),
          base::BindOnce(
              &TerminalPrivateOpenTerminalProcessFunction::OnGuestRunning, this,
              user_id_hash, std::move(cmdline)));
    } else {
      // Don't recognise the guest. Options include:
      // * It doesn't exist but the terminal app thinks it does e.g. out-of-sync
      //   prefs, race between uninstalling and launching terminal.
      // * We're installing Bruschetta, and it's using the terminal to finish
      //   installing. We don't show Bruschetta until it's installed, but it's
      //   running and users need to connect to it.
      // Given this, try and connect directly to it, if it succeeds, great, if
      // it fails, then report failure to the user.
      startup_status_ =
          std::make_unique<StartupStatus>(std::move(status_printer), 2);
      startup_status_->StartShowingSpinner();
      OpenVmshellProcess(user_id_hash, std::move(cmdline));
    }
  } else {
    // command=[unrecognized].
    return RespondNow(Error("Invalid process name: " + process_name));
  }
  return RespondLater();
}

void TerminalPrivateOpenTerminalProcessFunction::OnGuestRunning(
    const std::string& user_id_hash,
    base::CommandLine cmdline,
    bool success,
    std::string failure_reason) {
  if (success) {
    OpenVmshellProcess(user_id_hash, std::move(cmdline));
  } else {
    startup_status_->OnFinished(success, failure_reason);
    LOG(ERROR) << failure_reason;
    Respond(Error(failure_reason));
  }
}

void TerminalPrivateOpenTerminalProcessFunction::OpenVmshellProcess(
    const std::string& user_id_hash,
    base::CommandLine cmdline) {
  startup_status_->OnConnectingToVsh();
  const std::string cwd = cmdline.GetSwitchValueASCII(kSwitchCurrentWorkingDir);

  if (!base::StartsWith(cwd, kCwdTerminalIdPrefix)) {
    return OpenProcess(user_id_hash, std::move(cmdline));
  }
  cmdline.RemoveSwitch(kSwitchCurrentWorkingDir);

  // The cwd has this format `terminal_id:<terminal_id>`. We need to convert the
  // terminal id to the pid of the shell process inside the container.
  int host_pid = chromeos::ProcessProxyRegistry::ConvertToSystemPID(
      cwd.substr(sizeof(kCwdTerminalIdPrefix) - 1));

  // Lookup container shell pid from cicierone to use for cwd.
  vm_tools::cicerone::GetVshSessionRequest request;
  request.set_vm_name(guest_id_->vm_name);
  request.set_container_name(guest_id_->container_name);
  request.set_owner_id(crostini::CryptohomeIdForProfile(
      Profile::FromBrowserContext(browser_context())));
  request.set_host_vsh_pid(host_pid);
  ash::CiceroneClient::Get()->GetVshSession(
      request,
      base::BindOnce(
          &TerminalPrivateOpenTerminalProcessFunction::OnGetVshSession, this,
          user_id_hash, std::move(cmdline), /*terminal_id=*/cwd));
}

void TerminalPrivateOpenTerminalProcessFunction::OnGetVshSession(
    const std::string& user_id_hash,
    base::CommandLine cmdline,
    const std::string& terminal_id,
    std::optional<vm_tools::cicerone::GetVshSessionResponse> response) {
  if (!response || !response->success()) {
    LOG(WARNING) << "Failed to get vsh session for " << terminal_id << ": "
                 << (response ? response->failure_reason() : "empty response");
  } else {
    cmdline.AppendSwitchASCII(
        kSwitchCurrentWorkingDir,
        base::NumberToString(response->container_shell_pid()));
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
  if (startup_status_) {
    startup_status_->OnFinished(
        success, success ? "" : "Error connecting shell to guest");
  }
  auto* contents = GetSenderWebContents();
  if (!contents) {
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
    const std::string msg = "Web contents closed during OpenProcess";
    LOG(WARNING) << msg;
    Respond(Error(msg));
    return;
  }

  if (!success) {
    Respond(Error("Failed to open process."));
    return;
  }
  Respond(WithArguments(terminal_id));

  TerminalTabHelper::CreateForWebContents(contents);
  TerminalTabHelper::FromWebContents(contents)->AddTerminalId(terminal_id);
}

TerminalPrivateOpenVmshellProcessFunction::
    ~TerminalPrivateOpenVmshellProcessFunction() = default;

ExtensionFunction::ResponseAction
TerminalPrivateOpenVmshellProcessFunction::Run() {
  std::optional<OpenVmshellProcess::Params> params =
      OpenVmshellProcess::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // Only opens 'vmshell'.
  return OpenProcess(kVmShellName, std::move(params->args));
}

TerminalPrivateSendInputFunction::~TerminalPrivateSendInputFunction() = default;

ExtensionFunction::ResponseAction TerminalPrivateSendInputFunction::Run() {
  std::optional<SendInput::Params> params = SendInput::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

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
  chromeos::ProcessProxyRegistry::Get()->SendInput(
      terminal_id, text,
      base::BindOnce(&TerminalPrivateSendInputFunction::OnSendInput, this));
}

void TerminalPrivateSendInputFunction::OnSendInput(bool success) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&TerminalPrivateSendInputFunction::RespondOnUIThread, this,
                     success));
}

void TerminalPrivateSendInputFunction::RespondOnUIThread(bool success) {
  Respond(WithArguments(success));
}

TerminalPrivateCloseTerminalProcessFunction::
    ~TerminalPrivateCloseTerminalProcessFunction() = default;

ExtensionFunction::ResponseAction
TerminalPrivateCloseTerminalProcessFunction::Run() {
  std::optional<CloseTerminalProcess::Params> params =
      CloseTerminalProcess::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!TerminalTabHelper::ValidateTerminalId(GetSenderWebContents(),
                                             params->id)) {
    LOG(ERROR) << "invalid terminal id " << params->id;
    return RespondNow(Error("invalid terminal id"));
  }
  TerminalTabHelper::FromWebContents(GetSenderWebContents())
      ->RemoveTerminalId(params->id);

  CloseTerminal(
      params->id,
      base::BindOnce(
          &TerminalPrivateCloseTerminalProcessFunction::RespondOnUIThread,
          this));

  return RespondLater();
}

void TerminalPrivateCloseTerminalProcessFunction::RespondOnUIThread(
    bool success) {
  Respond(WithArguments(success));
}

TerminalPrivateOnTerminalResizeFunction::
    ~TerminalPrivateOnTerminalResizeFunction() = default;

ExtensionFunction::ResponseAction
TerminalPrivateOnTerminalResizeFunction::Run() {
  std::optional<OnTerminalResize::Params> params =
      OnTerminalResize::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

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
  Respond(WithArguments(success));
}

TerminalPrivateAckOutputFunction::~TerminalPrivateAckOutputFunction() = default;

ExtensionFunction::ResponseAction TerminalPrivateAckOutputFunction::Run() {
  std::optional<AckOutput::Params> params = AckOutput::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

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
  std::optional<OpenWindow::Params> params = OpenWindow::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const std::string* url = &guest_os::GetTerminalHomeUrl();
  bool as_tab = false;

  auto& data = params->data;
  if (data) {
    if (data->url) {
      url = &*data->url;
    }
    if (data->as_tab) {
      as_tab = *data->as_tab;
    }
  }

  if (as_tab) {
    auto* browser = chrome::FindBrowserWithTab(GetSenderWebContents());
    if (browser) {
      chrome::AddTabAt(browser, GURL(*url), -1, true);
    } else {
      LOG(ERROR) << "cannot find the browser";
    }
  } else {
    guest_os::LaunchTerminalWithUrl(
        Profile::FromBrowserContext(browser_context()),
        display::kInvalidDisplayId, /*restore_id=*/0, GURL(*url));
  }

  return RespondNow(NoArguments());
}

TerminalPrivateOpenOptionsPageFunction::
    ~TerminalPrivateOpenOptionsPageFunction() = default;

ExtensionFunction::ResponseAction
TerminalPrivateOpenOptionsPageFunction::Run() {
  guest_os::LaunchTerminalSettings(
      Profile::FromBrowserContext(browser_context()));
  return RespondNow(NoArguments());
}

TerminalPrivateOpenSettingsSubpageFunction::
    ~TerminalPrivateOpenSettingsSubpageFunction() = default;

ExtensionFunction::ResponseAction
TerminalPrivateOpenSettingsSubpageFunction::Run() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  // Ignore params->subpage for now, and always open crostini.
  if (ash::features::IsOsSettingsRevampWayfindingEnabled()) {
    if (crostini::CrostiniFeatures::Get()->IsEnabled(profile)) {
      chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
          profile, chromeos::settings::mojom::kCrostiniDetailsSubpagePath);
    } else {
      chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
          profile, chromeos::settings::mojom::kAboutChromeOsSectionPath,
          chromeos::settings::mojom::Setting::kSetUpCrostini);
    }
  } else {
    chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
        profile, chromeos::settings::mojom::kCrostiniSectionPath);
  }
  return RespondNow(NoArguments());
}

TerminalPrivateGetOSInfoFunction::~TerminalPrivateGetOSInfoFunction() = default;

ExtensionFunction::ResponseAction TerminalPrivateGetOSInfoFunction::Run() {
  base::Value::Dict info;
  info.Set("tast", extensions::ExtensionRegistry::Get(browser_context())
                       ->enabled_extensions()
                       .Contains(extension_misc::kGuestModeTestExtensionId));
  return RespondNow(WithArguments(std::move(info)));
}

TerminalPrivateGetPrefsFunction::~TerminalPrivateGetPrefsFunction() = default;

ExtensionFunction::ResponseAction TerminalPrivateGetPrefsFunction::Run() {
  std::optional<GetPrefs::Params> params = GetPrefs::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  PrefService* service =
      Profile::FromBrowserContext(browser_context())->GetPrefs();
  base::Value::Dict result;

  for (const auto& path : params->paths) {
    // Ignore non-allowed paths.
    if (!base::Contains(*kPrefsReadAllowList, path)) {
      LOG(WARNING) << "Ignoring non-allowed GetPrefs path=" << path;
      continue;
    }
    if (path == guest_os::prefs::kGuestOsTerminalSettings) {
      guest_os::RecordTerminalSettingsChangesUMAs(
          Profile::FromBrowserContext(browser_context()));
    }
    result.Set(path, service->GetValue(path).Clone());
  }
  return RespondNow(WithArguments(std::move(result)));
}

TerminalPrivateSetPrefsFunction::~TerminalPrivateSetPrefsFunction() = default;

ExtensionFunction::ResponseAction TerminalPrivateSetPrefsFunction::Run() {
  std::optional<SetPrefs::Params> params = SetPrefs::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  PrefService* service =
      Profile::FromBrowserContext(browser_context())->GetPrefs();

  static const base::NoDestructor<
      base::flat_map<std::string, base::Value::Type>>
      kAllowList{{{guest_os::prefs::kGuestOsTerminalSettings,
                   base::Value::Type::DICT}}};

  for (const auto item : params->prefs.additional_properties) {
    // Write prefs if they are allowed, and match expected type, else ignore.
    auto allow_it = kAllowList->find(item.first);
    if (allow_it == kAllowList->end() ||
        allow_it->second != item.second.type()) {
      LOG(WARNING) << "Ignoring non-allowed SetPrefs path=" << item.first
                   << ", type=" << item.second.type();
      continue;
    }
    service->Set(item.first, item.second);
  }
  return RespondNow(NoArguments());
}

}  // namespace extensions
