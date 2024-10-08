// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app_shim/app_shim_controller.h"

#import <Cocoa/Cocoa.h>
#include <mach/message.h>

#include <utility>

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/apple/mach_logging.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/hash/md5.h"
#include "base/mac/launch_application.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_mach_msg_destroy.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#import "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app_shim/app_shim_delegate.h"
#include "chrome/app_shim/app_shim_render_widget_host_view_mac_delegate.h"
#include "chrome/browser/ui/cocoa/browser_window_command_handler.h"
#include "chrome/browser/ui/cocoa/chrome_command_dispatcher_delegate.h"
#include "chrome/browser/ui/cocoa/main_menu_builder.h"
#import "chrome/browser/ui/cocoa/renderer_context_menu/chrome_swizzle_services_menu_updater.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/mac/app_mode_common.h"
#include "chrome/common/process_singleton_lock_posix.h"
#include "chrome/grit/generated_resources.h"
#import "chrome/services/mac_notifications/mac_notification_service_ns.h"
#import "chrome/services/mac_notifications/mac_notification_service_un.h"
#include "components/metrics/child_histogram_fetcher_impl.h"
#include "components/remote_cocoa/app_shim/application_bridge.h"
#include "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#include "components/remote_cocoa/common/application.mojom.h"
#include "components/variations/field_trial_config/field_trial_util.h"
#include "components/variations/variations_switches.h"
#include "content/public/browser/remote_cocoa.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"
#include "ui/gfx/image/image.h"

// The ProfileMenuTarget bridges between Objective C (as the target for the
// profile menu NSMenuItems) and C++ (the mojo methods called by
// AppShimController).
@interface ProfileMenuTarget : NSObject {
  raw_ptr<AppShimController> _controller;
}
- (instancetype)initWithController:(AppShimController*)controller;
- (void)clearController;
@end

@implementation ProfileMenuTarget
- (instancetype)initWithController:(AppShimController*)controller {
  if (self = [super init])
    _controller = controller;
  return self;
}

- (void)clearController {
  _controller = nullptr;
}

- (void)profileMenuItemSelected:(id)sender {
  if (_controller)
    _controller->ProfileMenuItemSelected([sender tag]);
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  return YES;
}
@end

// The ApplicationDockMenuTarget bridges between Objective C (as the target for
// the profile menu NSMenuItems) and C++ (the mojo methods called by
// AppShimController).
@interface ApplicationDockMenuTarget : NSObject {
  raw_ptr<AppShimController> _controller;
}
- (instancetype)initWithController:(AppShimController*)controller;
- (void)clearController;
@end

@implementation ApplicationDockMenuTarget
- (instancetype)initWithController:(AppShimController*)controller {
  if (self = [super init])
    _controller = controller;
  return self;
}

- (void)clearController {
  _controller = nullptr;
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  return YES;
}

- (void)commandFromDock:(id)sender {
  if (_controller)
    _controller->CommandFromDock([sender tag]);
}

@end

namespace {
// The maximum amount of time to wait for Chrome's AppShimListener to be
// ready.
constexpr base::TimeDelta kPollTimeoutSeconds = base::Seconds(60);

// The period in between attempts to check of Chrome's AppShimListener is
// ready.
constexpr base::TimeDelta kPollPeriodMsec = base::Milliseconds(100);

// Helper that keeps stops another sequence from executing any code while this
// object is alive. The constructor waits for the other thread to be blocked,
// while the destructor signals the other thread to continue running again.
class ScopedSynchronizeThreads {
 public:
  explicit ScopedSynchronizeThreads(
      scoped_refptr<base::SequencedTaskRunner> thread_runner) {
    // This event is signalled by a task posted to the other thread as soon as
    // it starts executing, to signal that no more code is running on that
    // thread. The main thread only proceeds after this event is signalled.
    base::WaitableEvent thread_blocked;
    // Heap allocate `operation_finished` and make sure it is destroyed on the
    // thread that waits on that event. This ensures it isn't destroyed too
    // early.
    auto operation_finished = std::make_unique<base::WaitableEvent>();
    operation_finished_ = operation_finished.get();
    thread_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WaitableEvent* thread_blocked,
               std::unique_ptr<base::WaitableEvent> operation_finished) {
              thread_blocked->Signal();
              operation_finished->Wait();
            },
            &thread_blocked, std::move(operation_finished)));
    thread_blocked.Wait();
  }

  ~ScopedSynchronizeThreads() { operation_finished_->Signal(); }

 private:
  // This event is signalled by the main thread to indicate that all the work is
  // done, allowing the other thread to be unblocked again.
  raw_ptr<base::WaitableEvent> operation_finished_;
};

}  // namespace

AppShimController::Params::Params() = default;
AppShimController::Params::Params(const Params& other) = default;
AppShimController::Params::~Params() = default;

AppShimController::AppShimController(const Params& params)
    : params_(params),
      host_receiver_(host_.BindNewPipeAndPassReceiver()),
      delegate_([[AppShimDelegate alloc] initWithController:this]),
      profile_menu_target_([[ProfileMenuTarget alloc] initWithController:this]),
      application_dock_menu_target_(
          [[ApplicationDockMenuTarget alloc] initWithController:this]) {
  screen_ = std::make_unique<display::ScopedNativeScreen>();
  NSApp.delegate = delegate_;

  [ChromeSwizzleServicesMenuUpdater install];

  // Since this is early startup code, there is no guarantee that the state of
  // the features being tested for here matches the state from the eventualy
  // Chrome we connect to (although they will match the vast majority of the
  // time). Creating the notification service when it ends up being not needed
  // is harmless, and the only effect of not creating it early when we later
  // need it is that we might miss some notification actions, which again is
  // harmless.
  if (base::FeatureList::IsEnabled(features::kAppShimNotificationAttribution) &&
      WebAppIsAdHocSigned()) {
    // `notification_service_` needs to be created early during start up to make
    // sure it is able to install its delegate before the OS attempts to inform
    // it of any notification actions that might have happened.
    notification_service_ =
        std::make_unique<mac_notifications::MacNotificationServiceUN>(
            std::move(notification_action_handler_remote_),
            base::BindRepeating(
                &AppShimController::NotificationPermissionStatusChanged,
                base::Unretained(this)),
            UNUserNotificationCenter.currentNotificationCenter);
  }
}

AppShimController::~AppShimController() {
  // Un-set the delegate since NSApplication does not retain it.
  NSApp.delegate = nil;
  [profile_menu_target_ clearController];
  [application_dock_menu_target_ clearController];
}

// static
void AppShimController::PreInitFeatureState(
    const base::CommandLine& command_line) {
  new base::FieldTrialList();

  auto feature_list = std::make_unique<base::FeatureList>();
  base::FeatureList::FailOnFeatureAccessWithoutFeatureList();

  // App shims can generally be launched in one of two ways:
  // - By chrome itself, in which case the full feature and field trial state
  //   is passed on the command line, and any state stored in user_data_dir is
  //   ignored. In this case we could avoid re-initializing feature and field
  //   trial state in FinalizeFeatureState entirely, but since this is the case
  //   with by far the most test coverage, doing so would make it much more
  //   likely that some change accidentally slips in that would breaks with the
  //   early access and reinitialization behavior.
  // - By the OS or user directly. In which case a (possibly outdated) state is
  //   loaded from user_data_dir, but we do allow explicit feature and/or field
  //   trial overrides on the command line to help with manual
  //   testing/development.
  //
  // In both cases the state initialized here is only used during startup, and
  // as soon as a mojo connection has been established with Chrome the final
  // feature state is passed to FinalizeFeatureState below.
  //
  // Several integration tests launch app shims with somewhat of a mix of these
  // two options. Where tests try to simulate an app shim being launched by the
  // OS we still pass switches such as the kLaunchedByChromeProcessId (to ensure
  // the app shim communicates with the correct test instance), but don't pass
  // the full feature state on the command line, so having
  // kLaunchedByChromeProcessId be present does not guarantee that feature state
  // is passed on the command line as well.

  // Add command line overrides. These will always be set if this app shim is
  // launched by chrome, but for development/testing purposes can also be used
  // to override state found in the user_data_dir file if the launch was not
  // triggered by chrome. In either case, FinalizeFeatureState will reset all
  // feature and field trial state to match the state of the running Chrome
  // instance.
  variations::VariationsCommandLine::GetForCommandLine(command_line)
      .ApplyToFeatureAndFieldTrialList(feature_list.get());

  // If the shim was launched by chrome, we're done. However if the shim was
  // launched directly by the user/OS the command line parameters were merely
  // optional overrides, so read state from the file in user_data_dir to get the
  // correct feature and field trial state for features and field trials that
  // have not already been explicitly overridden.
  if (!command_line.HasSwitch(app_mode::kLaunchedByChromeProcessId)) {
    auto file_state = variations::VariationsCommandLine::ReadFromFile(
        base::PathService::CheckedGet(chrome::DIR_USER_DATA)
            .Append(app_mode::kFeatureStateFileName));
    if (file_state.has_value()) {
      file_state->ApplyToFeatureAndFieldTrialList(feature_list.get());
    }
  }

  // Until FinalizeFeatureState() is called, only features whose name is in the
  // below list are allowed to be passed to base::FeatureList::IsEnabled().
  // Attempts to check the state of any other feature will behave as if no
  // FeatureList was set yet at all (i.e. check-fail).
  base::FeatureList::SetEarlyAccessInstance(
      std::move(feature_list),
      {"AppShimLaunchChromeSilently", "AppShimNotificationAttribution",
       "DcheckIsFatal", "DisallowSpaceCharacterInURLHostParsing",
       "MojoMessageAlwaysUseLatestVersion", "MojoBindingsInlineSLS",
       "MojoInlineMessagePayloads", "MojoIpcz", "MojoIpczMemV2",
       "MojoTaskPerMessage", "StandardCompliantHostCharacters",
       "StandardCompliantNonSpecialSchemeURLParsing",
       "UseAdHocSigningForWebAppShims", "UseIDNA2008NonTransitional",
       "SonomaAccessibilityActivationRefinements", "FeatureParamWithCache"});
}

// static
void AppShimController::FinalizeFeatureState(
    const variations::VariationsCommandLine& feature_state,
    const scoped_refptr<base::SequencedTaskRunner>& io_thread_runner) {
  // This code assumes no other threads are running. So make sure there is no
  // started ThreadPoolInstance, and block the IO thread for the duration of
  // this method.
  CHECK(!base::ThreadPoolInstance::Get() ||
        !base::ThreadPoolInstance::Get()->WasStarted());
  ScopedSynchronizeThreads block_io_thread(io_thread_runner);

  // Recreate FieldTrialList.
  std::unique_ptr<base::FieldTrialList> old_field_trial_list(
      base::FieldTrialList::ResetInstance());
  CHECK(old_field_trial_list);
  // This is intentionally leaked since it needs to live for the duration of
  // the app shim process and there's no benefit in cleaning it up at exit.
  auto* field_trial_list = new base::FieldTrialList();
  ANNOTATE_LEAKING_OBJECT_PTR(field_trial_list);
  std::ignore = field_trial_list;

  // Reset FieldTrial parameter cache.
  base::FieldTrialParamAssociator::GetInstance()->ClearAllCachedParams({});

  // Create a new FeatureList and field trial state using what was passed by the
  // browser process.
  auto feature_list = std::make_unique<base::FeatureList>();
  feature_state.ApplyToFeatureAndFieldTrialList(feature_list.get());

  base::FeatureList::SetInstance(std::move(feature_list));
}

void AppShimController::OnAppFinishedLaunching(
    bool launched_by_notification_action) {
  DCHECK_EQ(init_state_, InitState::kWaitingForAppToFinishLaunch);
  init_state_ = InitState::kWaitingForChromeReady;
  launched_by_notification_action_ = launched_by_notification_action;

  if (FindOrLaunchChrome()) {
    // Start polling to see if Chrome is ready to connect.
    PollForChromeReady(kPollTimeoutSeconds);
  }

  // Otherwise, Chrome is in the process of launching and `PollForChromeReady`
  // will be called when launching is complete.
}

bool AppShimController::FindOrLaunchChrome() {
  DCHECK(!chrome_to_connect_to_);
  DCHECK(!chrome_launched_by_app_);
  const base::CommandLine* app_command_line =
      base::CommandLine::ForCurrentProcess();

  // If this shim was launched by Chrome, only connect to that that specific
  // process.
  if (app_command_line->HasSwitch(app_mode::kLaunchedByChromeProcessId)) {
    std::string chrome_pid_string = app_command_line->GetSwitchValueASCII(
        app_mode::kLaunchedByChromeProcessId);

    int chrome_pid;
    if (!base::StringToInt(chrome_pid_string, &chrome_pid)) {
      LOG(FATAL) << "Invalid PID: " << chrome_pid_string;
    }

    chrome_to_connect_to_ = [NSRunningApplication
        runningApplicationWithProcessIdentifier:chrome_pid];
    if (!chrome_to_connect_to_) {
      // Sometimes runningApplicationWithProcessIdentifier fails to return the
      // application, even though it exists. If that happens, try to find the
      // running application in the full list of running applications manually.
      // See https://crbug.com/1426897.
      NSArray<NSRunningApplication*>* apps =
          NSWorkspace.sharedWorkspace.runningApplications;
      for (unsigned i = 0; i < apps.count; ++i) {
        if (apps[i].processIdentifier == chrome_pid) {
          chrome_to_connect_to_ = apps[i];
        }
      }
      if (!chrome_to_connect_to_) {
        LOG(FATAL) << "Failed to open process with PID: " << chrome_pid;
      }
    }

    return true;
  }

  // Query the singleton lock. If the lock exists and specifies a running
  // Chrome, then connect to that process. Otherwise, launch a new Chrome
  // process.
  chrome_to_connect_to_ = FindChromeFromSingletonLock(params_.user_data_dir);
  if (chrome_to_connect_to_) {
    return true;
  }

  // In tests, launching Chrome does nothing.
  if (app_command_line->HasSwitch(app_mode::kLaunchedForTest)) {
    return true;
  }

  // Otherwise, launch Chrome.
  base::FilePath chrome_bundle_path = base::apple::OuterBundlePath();
  LOG(INFO) << "Launching " << chrome_bundle_path.value();
  base::CommandLine browser_command_line(base::CommandLine::NO_PROGRAM);
  browser_command_line.AppendSwitchPath(switches::kUserDataDir,
                                        params_.user_data_dir);

  // Forward feature and field trial related switches to Chrome to aid in
  // testing and development with custom feature or field trial configurations.
  static constexpr const char* switches_to_forward[] = {
      switches::kEnableFeatures, switches::kDisableFeatures,
      switches::kForceFieldTrials,
      variations::switches::kForceFieldTrialParams};
  for (const char* switch_name : switches_to_forward) {
    if (app_command_line->HasSwitch(switch_name)) {
      browser_command_line.AppendSwitchASCII(
          switch_name, app_command_line->GetSwitchValueASCII(switch_name));
    }
  }

  const bool silent_chrome_launch =
      base::FeatureList::IsEnabled(features::kAppShimLaunchChromeSilently);
  if (silent_chrome_launch) {
    browser_command_line.AppendSwitch(switches::kNoStartupWindow);
  }

  base::mac::LaunchApplication(
      chrome_bundle_path, browser_command_line, /*url_specs=*/{},
      {.create_new_instance = true,
       .hidden_in_background = silent_chrome_launch},
      base::BindOnce(
          [](AppShimController* shim_controller, NSRunningApplication* app,
             NSError* error) {
            if (error) {
              LOG(FATAL) << "Failed to launch Chrome.";
            }

            shim_controller->chrome_launched_by_app_ = app;

            // Start polling to see if Chrome is ready to connect.
            shim_controller->PollForChromeReady(kPollTimeoutSeconds);
          },
          // base::Unretained is safe because this is a singleton.
          base::Unretained(this)));

  return false;
}

// static
NSRunningApplication* AppShimController::FindChromeFromSingletonLock(
    const base::FilePath& user_data_dir) {
  base::FilePath lock_symlink_path =
      user_data_dir.Append(chrome::kSingletonLockFilename);
  std::string hostname;
  int pid = -1;
  if (!ParseProcessSingletonLock(lock_symlink_path, &hostname, &pid)) {
    // This indicates that there is no Chrome process running (or that has been
    // running long enough to get the lock).
    LOG(INFO) << "Singleton lock not found at " << lock_symlink_path.value();
    return nil;
  }

  // Open the associated pid. This could be invalid if Chrome terminated
  // abnormally and didn't clean up.
  NSRunningApplication* process_from_lock =
      [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
  if (!process_from_lock) {
    LOG(WARNING) << "Singleton lock pid " << pid << " invalid.";
    return nil;
  }

  // Check the process' bundle id. As above, the specified pid could have been
  // reused by some other process.
  NSString* expected_bundle_id = base::apple::OuterBundle().bundleIdentifier;
  NSString* lock_bundle_id = process_from_lock.bundleIdentifier;
  if (![expected_bundle_id isEqualToString:lock_bundle_id]) {
    LOG(WARNING) << "Singleton lock pid " << pid
                 << " has unexpected bundle id.";
    return nil;
  }

  return process_from_lock;
}

void AppShimController::PollForChromeReady(
    const base::TimeDelta& time_until_timeout) {
  // If the Chrome process we planned to connect to is not running anymore,
  // quit.
  if (chrome_to_connect_to_ && chrome_to_connect_to_.terminated) {
    LOG(FATAL) << "Running chrome instance terminated before connecting.";
  }

  // If we launched a Chrome process and it has terminated, then that most
  // likely means that it did not get the singleton lock (which means that we
  // should find the processes that did below).
  bool launched_chrome_is_terminated =
      chrome_launched_by_app_ && chrome_launched_by_app_.terminated;

  // If we haven't found the Chrome process that got the singleton lock, check
  // now.
  if (!chrome_to_connect_to_)
    chrome_to_connect_to_ = FindChromeFromSingletonLock(params_.user_data_dir);

  // If our launched Chrome has terminated, then there should have existed a
  // process holding the singleton lock.
  if (launched_chrome_is_terminated && !chrome_to_connect_to_)
    LOG(FATAL) << "Launched Chrome has exited and singleton lock not taken.";

  // Poll to see if the mojo channel is ready. Of note is that we don't actually
  // verify that |endpoint| is connected to |chrome_to_connect_to_|.
  {
    mojo::PlatformChannelEndpoint endpoint;
    NSString* browser_bundle_id =
        base::apple::ObjCCast<NSString>([NSBundle.mainBundle
            objectForInfoDictionaryKey:app_mode::kBrowserBundleIDKey]);
    CHECK(browser_bundle_id);
    const std::string server_name = base::StringPrintf(
        "%s.%s.%s", base::SysNSStringToUTF8(browser_bundle_id).c_str(),
        app_mode::kAppShimBootstrapNameFragment,
        base::MD5String(params_.user_data_dir.value()).c_str());
    endpoint = ConnectToBrowser(server_name);
    if (endpoint.is_valid()) {
      LOG(INFO) << "Connected to " << server_name;
      SendBootstrapOnShimConnected(std::move(endpoint));
      return;
    }
  }

  // Otherwise, try again after a brief delay.
  if (time_until_timeout < kPollPeriodMsec)
    LOG(FATAL) << "Timed out waiting for running chrome instance to be ready.";
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AppShimController::PollForChromeReady,
                     base::Unretained(this),
                     time_until_timeout - kPollPeriodMsec),
      kPollPeriodMsec);
}

// static
mojo::PlatformChannelEndpoint AppShimController::ConnectToBrowser(
    const mojo::NamedPlatformChannel::ServerName& server_name) {
  // Normally NamedPlatformChannel is used for point-to-point peer
  // communication. For apps shims, the same server is used to establish
  // connections between multiple shim clients and the server. To do this,
  // the shim creates a local PlatformChannel and sends the local (send)
  // endpoint to the server in a raw Mach message. The server uses that to
  // establish an IsolatedConnection, which the client does as well with the
  // remote (receive) end.
  mojo::PlatformChannelEndpoint server_endpoint =
      mojo::NamedPlatformChannel::ConnectToServer(server_name);
  // The browser may still be in the process of launching, so the endpoint
  // may not yet be available.
  if (!server_endpoint.is_valid())
    return mojo::PlatformChannelEndpoint();

  mojo::PlatformChannel channel;
  mach_msg_base_t message{};
  base::ScopedMachMsgDestroy scoped_message(&message.header);
  message.header.msgh_id = app_mode::kBootstrapMsgId;
  message.header.msgh_bits =
      MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND, MACH_MSG_TYPE_MOVE_SEND);
  message.header.msgh_size = sizeof(message);
  message.header.msgh_local_port =
      channel.TakeLocalEndpoint().TakePlatformHandle().ReleaseMachSendRight();
  message.header.msgh_remote_port =
      server_endpoint.TakePlatformHandle().ReleaseMachSendRight();
  kern_return_t kr = mach_msg_send(&message.header);
  if (kr == KERN_SUCCESS) {
    scoped_message.Disarm();
  } else {
    MACH_LOG(ERROR, kr) << "mach_msg_send";
    return mojo::PlatformChannelEndpoint();
  }
  return channel.TakeRemoteEndpoint();
}

void AppShimController::SendBootstrapOnShimConnected(
    mojo::PlatformChannelEndpoint endpoint) {
  DCHECK_EQ(init_state_, InitState::kWaitingForChromeReady);
  init_state_ = InitState::kHasSentOnShimConnected;

  // Chrome will relaunch shims when relaunching apps.
  [NSApp disableRelaunchOnLogin];
  CHECK(!params_.user_data_dir.empty());

  mojo::ScopedMessagePipeHandle message_pipe =
      bootstrap_mojo_connection_.Connect(std::move(endpoint));
  CHECK(message_pipe.is_valid());
  host_bootstrap_.Bind(mojo::PendingRemote<chrome::mojom::AppShimHostBootstrap>(
      std::move(message_pipe), 0));
  host_bootstrap_.set_disconnect_with_reason_handler(base::BindOnce(
      &AppShimController::BootstrapChannelError, base::Unretained(this)));

  auto app_shim_info = chrome::mojom::AppShimInfo::New();
  app_shim_info->profile_path = params_.profile_dir;
  app_shim_info->app_id = params_.app_id;
  app_shim_info->app_url = params_.app_url;
  // If the app shim was launched for a notification action, we don't want to
  // automatically launch the app as well. So do a kRegisterOnly launch
  // instead.
  app_shim_info->launch_type =
      launched_by_notification_action_
          ? chrome::mojom::AppShimLaunchType::kNotificationAction
      : (base::CommandLine::ForCurrentProcess()->HasSwitch(
             app_mode::kLaunchedByChromeProcessId) &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             app_mode::kIsNormalLaunch))
          ? chrome::mojom::AppShimLaunchType::kRegisterOnly
          : chrome::mojom::AppShimLaunchType::kNormal;
  app_shim_info->files = launch_files_;
  app_shim_info->urls = launch_urls_;

  if (base::mac::WasLaunchedAsHiddenLoginItem()) {
    app_shim_info->login_item_restore_state =
        chrome::mojom::AppShimLoginItemRestoreState::kHidden;
  } else if (base::mac::WasLaunchedAsLoginOrResumeItem()) {
    app_shim_info->login_item_restore_state =
        chrome::mojom::AppShimLoginItemRestoreState::kWindowed;
  } else {
    app_shim_info->login_item_restore_state =
        chrome::mojom::AppShimLoginItemRestoreState::kNone;
  }

  app_shim_info->notification_action_handler =
      std::move(notification_action_handler_receiver_);

  host_bootstrap_->OnShimConnected(
      std::move(host_receiver_), std::move(app_shim_info),
      base::BindOnce(&AppShimController::OnShimConnectedResponse,
                     base::Unretained(this)));
  LOG(INFO) << "Sent OnShimConnected";
}

void AppShimController::SetUpMenu() {
  chrome::BuildMainMenu(NSApp, delegate_, params_.app_name, true);
  UpdateProfileMenu(std::vector<chrome::mojom::ProfileMenuItemPtr>());
}

void AppShimController::BootstrapChannelError(uint32_t custom_reason,
                                              const std::string& description) {
  // The bootstrap channel is expected to close after the response to
  // OnShimConnected is received.
  if (init_state_ == InitState::kHasReceivedOnShimConnectedResponse)
    return;
  LOG(ERROR) << "Bootstrap Channel error custom_reason:" << custom_reason
             << " description: " << description;
  [NSApp terminate:nil];
}

void AppShimController::ChannelError(uint32_t custom_reason,
                                     const std::string& description) {
  LOG(ERROR) << "Channel error custom_reason:" << custom_reason
             << " description: " << description;
  [NSApp terminate:nil];
}

void AppShimController::OnShimConnectedResponse(
    chrome::mojom::AppShimLaunchResult result,
    variations::VariationsCommandLine feature_state,
    mojo::PendingReceiver<chrome::mojom::AppShim> app_shim_receiver) {
  LOG(INFO) << "Received OnShimConnected.";
  DCHECK_EQ(init_state_, InitState::kHasSentOnShimConnected);
  init_state_ = InitState::kHasReceivedOnShimConnectedResponse;

  // Finalize feature state and finish up initialization that was deferred for
  // feature state to be fully setup.
  FinalizeFeatureState(feature_state, params_.io_thread_runner);
  base::ThreadPoolInstance::Get()->StartWithDefaultParams();
  SetUpMenu();

  if (result != chrome::mojom::AppShimLaunchResult::kSuccess) {
    switch (result) {
      case chrome::mojom::AppShimLaunchResult::kSuccess:
        break;
      case chrome::mojom::AppShimLaunchResult::kSuccessAndDisconnect:
        LOG(ERROR) << "Launched successfully, but do not maintain connection.";
        break;
      case chrome::mojom::AppShimLaunchResult::kDuplicateHost:
        LOG(ERROR) << "An AppShimHostBootstrap already exists for this app.";
        break;
      case chrome::mojom::AppShimLaunchResult::kProfileNotFound:
        LOG(ERROR) << "No suitable profile found.";
        break;
      case chrome::mojom::AppShimLaunchResult::kAppNotFound:
        LOG(ERROR) << "App not installed for specified profile.";
        break;
      case chrome::mojom::AppShimLaunchResult::kProfileLocked:
        LOG(ERROR) << "Profile locked.";
        break;
      case chrome::mojom::AppShimLaunchResult::kFailedValidation:
        LOG(ERROR) << "Validation failed.";
        break;
    };
    [NSApp terminate:nil];
    return;
  }
  shim_receiver_.Bind(std::move(app_shim_receiver),
                      ui::WindowResizeHelperMac::Get()->task_runner());
  shim_receiver_.set_disconnect_with_reason_handler(
      base::BindOnce(&AppShimController::ChannelError, base::Unretained(this)));

  host_bootstrap_.reset();
}

void AppShimController::CreateRemoteCocoaApplication(
    mojo::PendingAssociatedReceiver<remote_cocoa::mojom::Application>
        receiver) {
  remote_cocoa::ApplicationBridge::Get()->BindReceiver(std::move(receiver));
  remote_cocoa::ApplicationBridge::Get()->SetContentNSViewCreateCallbacks(
      base::BindRepeating(&AppShimController::CreateRenderWidgetHostNSView),
      base::BindRepeating(remote_cocoa::CreateWebContentsNSView));
}

void AppShimController::CreateRenderWidgetHostNSView(
    uint64_t view_id,
    mojo::ScopedInterfaceEndpointHandle host_handle,
    mojo::ScopedInterfaceEndpointHandle view_request_handle) {
  remote_cocoa::RenderWidgetHostViewMacDelegateCallback
      responder_delegate_creation_callback =
          base::BindOnce(&AppShimController::GetDelegateForHost, view_id);
  remote_cocoa::CreateRenderWidgetHostNSView(
      view_id, std::move(host_handle), std::move(view_request_handle),
      std::move(responder_delegate_creation_callback));
}

NSObject<RenderWidgetHostViewMacDelegate>*
AppShimController::GetDelegateForHost(uint64_t view_id) {
  return [[AppShimRenderWidgetHostViewMacDelegate alloc]
      initWithRenderWidgetHostNSViewID:view_id];
}

void AppShimController::CreateCommandDispatcherForWidget(uint64_t widget_id) {
  if (auto* bridge =
          remote_cocoa::NativeWidgetNSWindowBridge::GetFromId(widget_id)) {
    bridge->SetCommandDispatcher([[ChromeCommandDispatcherDelegate alloc] init],
                                 [[BrowserWindowCommandHandler alloc] init]);
  } else {
    LOG(ERROR) << "Failed to find host for command dispatcher.";
  }
}

void AppShimController::SetBadgeLabel(const std::string& badge_label) {
  NSApp.dockTile.badgeLabel = base::SysUTF8ToNSString(badge_label);
}

void AppShimController::UpdateProfileMenu(
    std::vector<chrome::mojom::ProfileMenuItemPtr> profile_menu_items) {
  profile_menu_items_ = std::move(profile_menu_items);

  NSMenuItem* cocoa_profile_menu =
      [NSApp.mainMenu itemWithTag:IDC_PROFILE_MAIN_MENU];
  if (profile_menu_items_.empty()) {
    cocoa_profile_menu.submenu = nil;
    cocoa_profile_menu.hidden = YES;
    return;
  }
  cocoa_profile_menu.hidden = NO;

  NSMenu* menu = [[NSMenu alloc]
      initWithTitle:l10n_util::GetNSStringWithFixup(IDS_PROFILES_MENU_NAME)];
  [cocoa_profile_menu setSubmenu:menu];

  // Note that this code to create menu items is nearly identical to the code
  // in ProfileMenuController in the browser process.
  for (size_t i = 0; i < profile_menu_items_.size(); ++i) {
    const auto& mojo_item = profile_menu_items_[i];
    NSString* name = base::SysUTF16ToNSString(mojo_item->name);
    NSMenuItem* item =
        [[NSMenuItem alloc] initWithTitle:name
                                   action:@selector(profileMenuItemSelected:)
                            keyEquivalent:@""];
    item.tag = mojo_item->menu_index;
    item.state =
        mojo_item->active ? NSControlStateValueOn : NSControlStateValueOff;
    item.target = profile_menu_target_;
    gfx::Image icon(mojo_item->icon);
    item.image = icon.AsNSImage();
    [menu insertItem:item atIndex:i];
  }
}

void AppShimController::UpdateApplicationDockMenu(
    std::vector<chrome::mojom::ApplicationDockMenuItemPtr> dock_menu_items) {
  dock_menu_items_ = std::move(dock_menu_items);
}

void AppShimController::BindNotificationProvider(
    mojo::PendingReceiver<mac_notifications::mojom::MacNotificationProvider>
        provider) {
  notifications_receiver_.reset();
  notifications_receiver_.Bind(std::move(provider));
}

void AppShimController::RequestNotificationPermission(
    RequestNotificationPermissionCallback callback) {
  if (!notification_service_un()) {
    std::move(callback).Run(
        mac_notifications::mojom::RequestPermissionResult::kRequestFailed);
    return;
  }
  notification_service_un()->RequestPermission(std::move(callback));
}

void AppShimController::BindNotificationService(
    mojo::PendingReceiver<mac_notifications::mojom::MacNotificationService>
        service,
    mojo::PendingRemote<mac_notifications::mojom::MacNotificationActionHandler>
        handler) {
  CHECK(
      base::FeatureList::IsEnabled(features::kAppShimNotificationAttribution));
  // TODO(crbug.com/40616749): Once ad-hoc signed app shims become the
  // default on supported platforms, change this to always use the
  // UNUserNotification API (and not support notification attribution on other
  // platforms at all).
  if (WebAppIsAdHocSigned()) {
    // While the constructor should have created the `notification_service_`
    // instance already, it is possible that the base::FeatureList state at the
    // time did not match the current Chrome state, so make sure to create the
    // service now if it wasn't created already.
    if (!notification_service_) {
      CHECK(notification_action_handler_remote_);
      notification_service_ =
          std::make_unique<mac_notifications::MacNotificationServiceUN>(
              std::move(notification_action_handler_remote_),
              base::BindRepeating(
                  &AppShimController::NotificationPermissionStatusChanged,
                  base::Unretained(this)),
              UNUserNotificationCenter.currentNotificationCenter);
    }
    // Note that `handler` as passed in to this method is ignored. Notification
    // actions instead will be dispatched to the app-shim scoped mojo pipe that
    // was established earlier during startup, to allow notification actions to
    // be triggered before the browser process tries to connect to the
    // notification service.
    notification_service_un()->Bind(std::move(service));
    // TODO(crbug.com/40616749): Determine when to ask for permissions.
    notification_service_un()->RequestPermission(base::DoNothing());
  } else {
    // NSUserNotificationCenter is in the process of being replaced, and
    // warnings about its deprecation are not helpful. https://crbug.com/1127306
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    notification_service_ =
        std::make_unique<mac_notifications::MacNotificationServiceNS>(
            std::move(service), std::move(handler),
            [NSUserNotificationCenter defaultUserNotificationCenter]);
#pragma clang diagnostic pop
  }
}

mac_notifications::MacNotificationServiceUN*
AppShimController::notification_service_un() {
  if (!WebAppIsAdHocSigned()) {
    return nullptr;
  }
  return static_cast<mac_notifications::MacNotificationServiceUN*>(
      notification_service_.get());
}

void AppShimController::NotificationPermissionStatusChanged(
    mac_notifications::mojom::PermissionStatus status) {
  host_->NotificationPermissionStatusChanged(status);
}

void AppShimController::SetUserAttention(
    chrome::mojom::AppShimAttentionType attention_type) {
  switch (attention_type) {
    case chrome::mojom::AppShimAttentionType::kCancel:
      [NSApp cancelUserAttentionRequest:attention_request_id_];
      attention_request_id_ = 0;
      break;
    case chrome::mojom::AppShimAttentionType::kCritical:
      attention_request_id_ = [NSApp requestUserAttention:NSCriticalRequest];
      break;
  }
}

void AppShimController::OpenFiles(const std::vector<base::FilePath>& files) {
  if (init_state_ == InitState::kWaitingForAppToFinishLaunch) {
    launch_files_ = files;
  } else {
    host_->FilesOpened(files);
  }
}

void AppShimController::ProfileMenuItemSelected(uint32_t index) {
  for (const auto& mojo_item : profile_menu_items_) {
    if (mojo_item->menu_index == index) {
      host_->ProfileSelectedFromMenu(mojo_item->profile_path);
      return;
    }
  }
}

void AppShimController::OpenUrls(const std::vector<GURL>& urls) {
  if (init_state_ == InitState::kWaitingForAppToFinishLaunch) {
    launch_urls_ = urls;
  } else {
    host_->UrlsOpened(urls);
  }
}

void AppShimController::CommandFromDock(uint32_t index) {
  DCHECK(0 <= index && index < dock_menu_items_.size());
  DCHECK(init_state_ != InitState::kWaitingForAppToFinishLaunch);

  [NSApp activateIgnoringOtherApps:YES];
  host_->OpenAppWithOverrideUrl(dock_menu_items_[index]->url);
}

void AppShimController::CommandDispatch(int command_id) {
  switch (command_id) {
    case IDC_WEB_APP_SETTINGS:
      host_->OpenAppSettings();
      break;
    case IDC_NEW_WINDOW:
      host_->ReopenApp();
      break;
  }
}

NSMenu* AppShimController::GetApplicationDockMenu() {
  if (init_state_ == InitState::kWaitingForAppToFinishLaunch ||
      dock_menu_items_.size() == 0)
    return nullptr;

  NSMenu* dockMenu = [[NSMenu alloc] initWithTitle:@""];

  for (size_t i = 0; i < dock_menu_items_.size(); ++i) {
    const auto& mojo_item = dock_menu_items_[i];
    NSString* name = base::SysUTF16ToNSString(mojo_item->name);
    NSMenuItem* item =
        [[NSMenuItem alloc] initWithTitle:name
                                   action:@selector(commandFromDock:)
                            keyEquivalent:@""];
    item.tag = i;
    item.target = application_dock_menu_target_;
    item.enabled =
        [application_dock_menu_target_ validateUserInterfaceItem:item];
    [dockMenu addItem:item];
  }

  return dockMenu;
}

void AppShimController::ApplicationWillTerminate() {
  // Local histogram to let tests verify that histograms are emitted properly.
  LOCAL_HISTOGRAM_BOOLEAN("AppShim.WillTerminate", true);
  host_->ApplicationWillTerminate();
}

void AppShimController::BindChildHistogramFetcherFactory(
    mojo::PendingReceiver<metrics::mojom::ChildHistogramFetcherFactory>
        receiver) {
  metrics::ChildHistogramFetcherFactoryImpl::Create(std::move(receiver));
}

bool AppShimController::WebAppIsAdHocSigned() const {
  NSNumber* isAdHocSigned =
      NSBundle.mainBundle.infoDictionary[app_mode::kCrAppModeIsAdHocSignedKey];
  return isAdHocSigned.boolValue;
}
