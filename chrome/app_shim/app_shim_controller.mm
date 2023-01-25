// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app_shim/app_shim_controller.h"

#import <Cocoa/Cocoa.h>
#include <mach/message.h>

#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/hash/md5.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/launch_application.h"
#include "base/mac/mac_util.h"
#include "base/mac/mach_logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#import "base/task/single_thread_task_runner.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app_shim/app_shim_delegate.h"
#include "chrome/app_shim/app_shim_render_widget_host_view_mac_delegate.h"
#include "chrome/browser/ui/cocoa/browser_window_command_handler.h"
#include "chrome/browser/ui/cocoa/chrome_command_dispatcher_delegate.h"
#include "chrome/browser/ui/cocoa/main_menu_builder.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/mac/app_mode_common.h"
#include "chrome/common/process_singleton_lock_posix.h"
#include "chrome/grit/generated_resources.h"
#include "components/remote_cocoa/app_shim/application_bridge.h"
#include "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#include "components/remote_cocoa/common/application.mojom.h"
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
  // Since AppShimController is created before the main message loop starts,
  // NSApp will not be set, so use sharedApplication.
  NSApplication* sharedApplication = [NSApplication sharedApplication];
  [sharedApplication setDelegate:delegate_];
}

AppShimController::~AppShimController() {
  // Un-set the delegate since NSApplication does not retain it.
  NSApplication* sharedApplication = [NSApplication sharedApplication];
  [sharedApplication setDelegate:nil];
  [profile_menu_target_ clearController];
  [application_dock_menu_target_ clearController];
}

void AppShimController::OnAppFinishedLaunching() {
  DCHECK_EQ(init_state_, InitState::kWaitingForAppToFinishLaunch);
  init_state_ = InitState::kWaitingForChromeReady;

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

    chrome_to_connect_to_.reset(
        [NSRunningApplication
            runningApplicationWithProcessIdentifier:chrome_pid],
        base::scoped_policy::RETAIN);
    if (!chrome_to_connect_to_) {
      LOG(FATAL) << "Failed to open process with PID: " << chrome_pid;
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
  base::FilePath chrome_bundle_path = base::mac::OuterBundlePath();
  LOG(INFO) << "Launching " << chrome_bundle_path.value();
  base::CommandLine browser_command_line(base::CommandLine::NO_PROGRAM);
  browser_command_line.AppendSwitchPath(switches::kUserDataDir,
                                        params_.user_data_dir);
  if (app_command_line->HasSwitch(switches::kEnableFeatures)) {
    browser_command_line.AppendSwitchASCII(
        switches::kEnableFeatures,
        app_command_line->GetSwitchValueASCII(switches::kEnableFeatures));
  }
  if (app_command_line->HasSwitch(switches::kDisableFeatures)) {
    browser_command_line.AppendSwitchASCII(
        switches::kDisableFeatures,
        app_command_line->GetSwitchValueASCII(switches::kDisableFeatures));
  }

  base::mac::LaunchApplication(
      chrome_bundle_path, browser_command_line, /*url_specs=*/{},
      {.create_new_instance = true},
      base::BindOnce(
          [](AppShimController* shim_controller,
             base::expected<NSRunningApplication*, NSError*> result) {
            if (!result.has_value()) {
              LOG(FATAL) << "Failed to launch Chrome.";
            }

            shim_controller->chrome_launched_by_app_.reset(
                result.value(), base::scoped_policy::RETAIN);

            // Start polling to see if Chrome is ready to connect.
            shim_controller->PollForChromeReady(kPollTimeoutSeconds);
          },
          // base::Unretained is safe because this is a singleton.
          base::Unretained(this)));

  return false;
}

// static
base::scoped_nsobject<NSRunningApplication>
AppShimController::FindChromeFromSingletonLock(
    const base::FilePath& user_data_dir) {
  base::FilePath lock_symlink_path =
      user_data_dir.Append(chrome::kSingletonLockFilename);
  std::string hostname;
  int pid = -1;
  if (!ParseProcessSingletonLock(lock_symlink_path, &hostname, &pid)) {
    // This indicates that there is no Chrome process running (or that has been
    // running long enough to get the lock).
    LOG(INFO) << "Singleton lock not found at " << lock_symlink_path.value();
    return base::scoped_nsobject<NSRunningApplication>();
  }

  // Open the associated pid. This could be invalid if Chrome terminated
  // abnormally and didn't clean up.
  base::scoped_nsobject<NSRunningApplication> process_from_lock(
      [NSRunningApplication runningApplicationWithProcessIdentifier:pid],
      base::scoped_policy::RETAIN);
  if (!process_from_lock) {
    LOG(WARNING) << "Singleton lock pid " << pid << " invalid.";
    return base::scoped_nsobject<NSRunningApplication>();
  }

  // Check the process' bundle id. As above, the specified pid could have been
  // reused by some other process.
  NSString* expected_bundle_id = [base::mac::OuterBundle() bundleIdentifier];
  NSString* lock_bundle_id = [process_from_lock bundleIdentifier];
  if (![expected_bundle_id isEqualToString:lock_bundle_id]) {
    LOG(WARNING) << "Singleton lock pid " << pid
                 << " has unexpected bundle id.";
    return base::scoped_nsobject<NSRunningApplication>();
  }

  return process_from_lock;
}

void AppShimController::PollForChromeReady(
    const base::TimeDelta& time_until_timeout) {
  // If the Chrome process we planned to connect to is not running anymore,
  // quit.
  if (chrome_to_connect_to_ && [chrome_to_connect_to_ isTerminated])
    LOG(FATAL) << "Running chrome instance terminated before connecting.";

  // If we launched a Chrome process and it has terminated, then that most
  // likely means that it did not get the singleton lock (which means that we
  // should find the processes that did below).
  bool launched_chrome_is_terminated =
      chrome_launched_by_app_ && [chrome_launched_by_app_ isTerminated];

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
        base::mac::ObjCCast<NSString>([[NSBundle mainBundle]
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
  message.header.msgh_id = app_mode::kBootstrapMsgId;
  message.header.msgh_bits =
      MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND, MACH_MSG_TYPE_MOVE_SEND);
  message.header.msgh_size = sizeof(message);
  message.header.msgh_local_port =
      channel.TakeLocalEndpoint().TakePlatformHandle().ReleaseMachSendRight();
  message.header.msgh_remote_port =
      server_endpoint.TakePlatformHandle().ReleaseMachSendRight();
  kern_return_t kr = mach_msg_send(&message.header);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "mach_msg_send";
    return mojo::PlatformChannelEndpoint();
  }
  return channel.TakeRemoteEndpoint();
}

void AppShimController::SendBootstrapOnShimConnected(
    mojo::PlatformChannelEndpoint endpoint) {
  DCHECK_EQ(init_state_, InitState::kWaitingForChromeReady);
  init_state_ = InitState::kHasSentOnShimConnected;

  SetUpMenu();

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
  app_shim_info->launch_type =
      (base::CommandLine::ForCurrentProcess()->HasSwitch(
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
    mojo::PendingReceiver<chrome::mojom::AppShim> app_shim_receiver) {
  LOG(INFO) << "Received OnShimConnected.";
  DCHECK_EQ(init_state_, InitState::kHasSentOnShimConnected);
  init_state_ = InitState::kHasReceivedOnShimConnectedResponse;

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
      responder_delegate_creation_callback = base::BindOnce(
          &AppShimController::CreateRenderWidgetHostViewDelegate, view_id);
  remote_cocoa::CreateRenderWidgetHostNSView(
      view_id, std::move(host_handle), std::move(view_request_handle),
      std::move(responder_delegate_creation_callback));
}

NSObject<RenderWidgetHostViewMacDelegate>*
AppShimController::CreateRenderWidgetHostViewDelegate(uint64_t view_id) {
  return [[AppShimRenderWidgetHostViewMacDelegate alloc]
      initWithRenderWidgetHostNSViewID:view_id];
}

void AppShimController::CreateCommandDispatcherForWidget(uint64_t widget_id) {
  if (auto* bridge =
          remote_cocoa::NativeWidgetNSWindowBridge::GetFromId(widget_id)) {
    bridge->SetCommandDispatcher(
        [[[ChromeCommandDispatcherDelegate alloc] init] autorelease],
        [[[BrowserWindowCommandHandler alloc] init] autorelease]);
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
      [[NSApp mainMenu] itemWithTag:IDC_PROFILE_MAIN_MENU];
  if (profile_menu_items_.empty()) {
    [cocoa_profile_menu setSubmenu:nil];
    [cocoa_profile_menu setHidden:YES];
    return;
  }
  [cocoa_profile_menu setHidden:NO];

  base::scoped_nsobject<NSMenu> menu([[NSMenu alloc]
      initWithTitle:l10n_util::GetNSStringWithFixup(IDS_PROFILES_MENU_NAME)]);
  [cocoa_profile_menu setSubmenu:menu];

  // Note that this code to create menu items is nearly identical to the code
  // in ProfileMenuController in the browser process.
  for (size_t i = 0; i < profile_menu_items_.size(); ++i) {
    const auto& mojo_item = profile_menu_items_[i];
    NSString* name = base::SysUTF16ToNSString(mojo_item->name);
    NSMenuItem* item =
        [[[NSMenuItem alloc] initWithTitle:name
                                    action:@selector(profileMenuItemSelected:)
                             keyEquivalent:@""] autorelease];
    [item setTag:mojo_item->menu_index];
    [item setState:mojo_item->active ? NSControlStateValueOn
                                     : NSControlStateValueOff];
    [item setTarget:profile_menu_target_.get()];
    gfx::Image icon(mojo_item->icon);
    [item setImage:icon.AsNSImage()];
    [menu insertItem:item atIndex:i];
  }
}

void AppShimController::UpdateApplicationDockMenu(
    std::vector<chrome::mojom::ApplicationDockMenuItemPtr> dock_menu_items) {
  dock_menu_items_ = std::move(dock_menu_items);
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

NSMenu* AppShimController::GetApplicationDockMenu() {
  if (init_state_ == InitState::kWaitingForAppToFinishLaunch ||
      dock_menu_items_.size() == 0)
    return nullptr;

  NSMenu* dockMenu = [[[NSMenu alloc] initWithTitle:@""] autorelease];

  for (size_t i = 0; i < dock_menu_items_.size(); ++i) {
    const auto& mojo_item = dock_menu_items_[i];
    NSString* name = base::SysUTF16ToNSString(mojo_item->name);
    NSMenuItem* item =
        [[[NSMenuItem alloc] initWithTitle:name
                                    action:@selector(commandFromDock:)
                             keyEquivalent:@""] autorelease];
    [item setTag:i];
    [item setTarget:application_dock_menu_target_];
    [item setEnabled:[application_dock_menu_target_
                         validateUserInterfaceItem:item]];
    [dockMenu addItem:item];
  }

  return dockMenu;
}
