// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app_shim/app_shim_controller.h"

#import <Cocoa/Cocoa.h>
#include <mach/message.h>

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/hash/md5.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/launch_services_util.h"
#include "base/mac/mach_logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app_shim/app_shim_delegate.h"
#include "chrome/browser/ui/cocoa/browser_window_command_handler.h"
#include "chrome/browser/ui/cocoa/chrome_command_dispatcher_delegate.h"
#include "chrome/browser/ui/cocoa/main_menu_builder.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/mac/app_mode_common.h"
#include "chrome/common/mac/app_shim.mojom.h"
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
#include "ui/gfx/image/image.h"

// The ProfileMenuTarget bridges between Objective C (as the target for the
// profile menu NSMenuItems) and C++ (the mojo methods called by
// AppShimController).
@interface ProfileMenuTarget : NSObject {
  AppShimController* controller_;
}
- (id)initWithController:(AppShimController*)controller;
- (void)clearController;
@end

@implementation ProfileMenuTarget
- (id)initWithController:(AppShimController*)controller {
  if (self = [super init])
    controller_ = controller;
  return self;
}

- (void)clearController {
  controller_ = nullptr;
}

- (void)profileMenuItemSelected:(id)sender {
  if (controller_)
    controller_->ProfileMenuItemSelected([sender tag]);
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  return YES;
}
@end

namespace {
// The maximum amount of time to wait for Chrome's AppShimListener to be
// ready.
constexpr base::TimeDelta kPollTimeoutSeconds =
    base::TimeDelta::FromSeconds(60);

// The period in between attempts to check of Chrome's AppShimListener is
// ready.
constexpr base::TimeDelta kPollPeriodMsec =
    base::TimeDelta::FromMilliseconds(100);

}  // namespace

AppShimController::Params::Params() = default;
AppShimController::Params::Params(const Params& other) = default;
AppShimController::Params::~Params() = default;

AppShimController::AppShimController(const Params& params)
    : params_(params),
      host_receiver_(host_.BindNewPipeAndPassReceiver()),
      delegate_([[AppShimDelegate alloc] init]),
      launch_app_done_(false),
      attention_request_id_(0),
      profile_menu_target_(
          [[ProfileMenuTarget alloc] initWithController:this]) {
  // Since AppShimController is created before the main message loop starts,
  // NSApp will not be set, so use sharedApplication.
  NSApplication* sharedApplication = [NSApplication sharedApplication];
  [sharedApplication setDelegate:delegate_];

  // Ensure Chrome is launched.
  FindOrLaunchChrome();

  // Start polling to see if Chrome is ready to connect.
  PollForChromeReady(kPollTimeoutSeconds);
}

AppShimController::~AppShimController() {
  // Un-set the delegate since NSApplication does not retain it.
  NSApplication* sharedApplication = [NSApplication sharedApplication];
  [sharedApplication setDelegate:nil];
  [profile_menu_target_ clearController];
}

void AppShimController::FindOrLaunchChrome() {
  DCHECK(!chrome_to_connect_to_);
  DCHECK(!chrome_launched_by_app_);

  // If this shim was launched by Chrome, open that specified process.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          app_mode::kLaunchedByChromeProcessId)) {
    std::string chrome_pid_string =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            app_mode::kLaunchedByChromeProcessId);
    int chrome_pid;
    if (!base::StringToInt(chrome_pid_string, &chrome_pid))
      LOG(FATAL) << "Invalid PID: " << chrome_pid_string;

    chrome_to_connect_to_.reset([NSRunningApplication
        runningApplicationWithProcessIdentifier:chrome_pid]);
    if (!chrome_to_connect_to_)
      LOG(FATAL) << "Failed to open process with PID: " << chrome_pid;
    return;
  }

  // Query the singleton lock. If the lock exists and specifies a running
  // Chrome, then connect to that process. Otherwise, launch a new Chrome
  // process.
  chrome_to_connect_to_ = FindChromeFromSingletonLock();
  if (chrome_to_connect_to_)
    return;

  // In tests, launching Chrome does nothing.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          app_mode::kLaunchedForTest)) {
    return;
  }

  // Otherwise, launch Chrome.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch(switches::kSilentLaunch);
  command_line.AppendSwitchPath(switches::kProfileDirectory,
                                params_.profile_dir);
  command_line.AppendSwitchPath(switches::kUserDataDir, params_.user_data_dir);
  chrome_launched_by_app_.reset(base::mac::OpenApplicationWithPath(
      base::mac::OuterBundlePath(), command_line,
      NSWorkspaceLaunchNewInstance));
  if (!chrome_launched_by_app_)
    LOG(FATAL) << "Failed to launch Chrome.";
}

base::scoped_nsobject<NSRunningApplication>
AppShimController::FindChromeFromSingletonLock() const {
  base::FilePath lock_symlink_path =
      params_.user_data_dir.Append(chrome::kSingletonLockFilename);
  std::string hostname;
  int pid = -1;
  if (!ParseProcessSingletonLock(lock_symlink_path, &hostname, &pid)) {
    // This indicates that there is no Chrome process running (or that has been
    // running long enough to get the lock).
    return base::scoped_nsobject<NSRunningApplication>();
  }

  // Open the associated pid. This could be invalid if Chrome terminated
  // abnormally and didn't clean up.
  base::scoped_nsobject<NSRunningApplication> process_from_lock(
      [NSRunningApplication runningApplicationWithProcessIdentifier:pid]);
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
    chrome_to_connect_to_ = FindChromeFromSingletonLock();

  // If our launched Chrome has terminated, then there should have existed a
  // process holding the singleton lock.
  if (launched_chrome_is_terminated && !chrome_to_connect_to_)
    LOG(FATAL) << "Launched Chrome has exited and singleton lock not taken.";

  // Poll to see if the mojo channel is ready. Of note is that we don't actually
  // verify that |endpoint| is connected to |chrome_to_connect_to_|.
  mojo::PlatformChannelEndpoint endpoint = GetBrowserEndpoint();
  if (endpoint.is_valid()) {
    InitBootstrapPipe(std::move(endpoint));
    return;
  }

  // Otherwise, try again after a brief delay.
  if (time_until_timeout < kPollPeriodMsec)
    LOG(FATAL) << "Timed out waiting for running chrome instance to be ready.";
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AppShimController::PollForChromeReady,
                     base::Unretained(this),
                     time_until_timeout - kPollPeriodMsec),
      kPollPeriodMsec);
}

mojo::PlatformChannelEndpoint AppShimController::GetBrowserEndpoint() {
  NSString* browser_bundle_id =
      base::mac::ObjCCast<NSString>([[NSBundle mainBundle]
          objectForInfoDictionaryKey:app_mode::kBrowserBundleIDKey]);
  CHECK(browser_bundle_id);

  std::string name_fragment = base::StringPrintf(
      "%s.%s.%s", base::SysNSStringToUTF8(browser_bundle_id).c_str(),
      app_mode::kAppShimBootstrapNameFragment,
      base::MD5String(params_.user_data_dir.value()).c_str());
  return ConnectToBrowser(name_fragment);
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

void AppShimController::InitBootstrapPipe(
    mojo::PlatformChannelEndpoint endpoint) {
  SetUpMenu();

  // Chrome will relaunch shims when relaunching apps.
  [NSApp disableRelaunchOnLogin];
  CHECK(!params_.user_data_dir.empty());

  CreateChannelAndSendLaunchApp(std::move(endpoint));
}

void AppShimController::CreateChannelAndSendLaunchApp(
    mojo::PlatformChannelEndpoint endpoint) {
  mojo::ScopedMessagePipeHandle message_pipe =
      bootstrap_mojo_connection_.Connect(std::move(endpoint));
  CHECK(message_pipe.is_valid());
  host_bootstrap_.Bind(mojo::PendingRemote<chrome::mojom::AppShimHostBootstrap>(
      std::move(message_pipe), 0));
  host_bootstrap_.set_disconnect_with_reason_handler(base::BindOnce(
      &AppShimController::BootstrapChannelError, base::Unretained(this)));
  [delegate_ setController:this];

  auto app_shim_info = chrome::mojom::AppShimInfo::New();
  app_shim_info->profile_path = params_.profile_dir;
  app_shim_info->app_id = params_.app_id;
  app_shim_info->app_url = params_.app_url;
  app_shim_info->launch_type =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          app_mode::kLaunchedByChromeProcessId)
          ? apps::APP_SHIM_LAUNCH_REGISTER_ONLY
          : apps::APP_SHIM_LAUNCH_NORMAL;
  [delegate_ getFilesToOpenAtStartup:&app_shim_info->files];

  host_bootstrap_->OnShimConnected(
      std::move(host_receiver_), std::move(app_shim_info),
      base::BindOnce(&AppShimController::OnShimConnectedResponse,
                     base::Unretained(this)));
}

void AppShimController::SetUpMenu() {
  chrome::BuildMainMenu(NSApp, delegate_, params_.app_name, true);

  // Initialize the profiles menu to be empty. It will be updated from the
  // browser.
  UpdateProfileMenu(std::vector<chrome::mojom::ProfileMenuItemPtr>());
}

void AppShimController::BootstrapChannelError(uint32_t custom_reason,
                                              const std::string& description) {
  // The bootstrap channel is expected to close after sending
  // OnShimConnectedResponse.
  if (launch_app_done_)
    return;
  LOG(ERROR) << "Channel error custom_reason:" << custom_reason
             << " description: " << description;
  Close();
}

void AppShimController::ChannelError(uint32_t custom_reason,
                                     const std::string& description) {
  LOG(ERROR) << "Channel error custom_reason:" << custom_reason
             << " description: " << description;
  Close();
}

void AppShimController::OnShimConnectedResponse(
    apps::AppShimLaunchResult result,
    mojo::PendingReceiver<chrome::mojom::AppShim> app_shim_receiver) {
  if (result != apps::APP_SHIM_LAUNCH_SUCCESS) {
    Close();
    return;
  }
  shim_receiver_.Bind(std::move(app_shim_receiver),
                      ui::WindowResizeHelperMac::Get()->task_runner());
  shim_receiver_.set_disconnect_with_reason_handler(
      base::BindOnce(&AppShimController::ChannelError, base::Unretained(this)));

  std::vector<base::FilePath> files;
  if ([delegate_ getFilesToOpenAtStartup:&files])
    SendFocusApp(apps::APP_SHIM_FOCUS_OPEN_FILES, files);

  launch_app_done_ = true;
  host_bootstrap_.reset();
}

void AppShimController::CreateRemoteCocoaApplication(
    mojo::PendingAssociatedReceiver<remote_cocoa::mojom::Application>
        receiver) {
  remote_cocoa::ApplicationBridge::Get()->BindReceiver(std::move(receiver));
  remote_cocoa::ApplicationBridge::Get()->SetContentNSViewCreateCallbacks(
      base::BindRepeating(remote_cocoa::CreateRenderWidgetHostNSView),
      base::BindRepeating(remote_cocoa::CreateWebContentsNSView));
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

  base::scoped_nsobject<NSMenu> menu(
      [[NSMenu alloc] initWithTitle:l10n_util::GetNSStringWithFixup(
                                        IDS_PROFILES_OPTIONS_GROUP_NAME)]);
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
    [item setState:mojo_item->active ? NSOnState : NSOffState];
    [item setTarget:profile_menu_target_.get()];
    gfx::Image icon(mojo_item->icon);
    [item setImage:icon.ToNSImage()];
    [menu insertItem:item atIndex:i];
  }
}

void AppShimController::SetUserAttention(
    apps::AppShimAttentionType attention_type) {
  switch (attention_type) {
    case apps::APP_SHIM_ATTENTION_CANCEL:
      [NSApp cancelUserAttentionRequest:attention_request_id_];
      attention_request_id_ = 0;
      break;
    case apps::APP_SHIM_ATTENTION_CRITICAL:
      attention_request_id_ = [NSApp requestUserAttention:NSCriticalRequest];
      break;
    case apps::APP_SHIM_ATTENTION_INFORMATIONAL:
      attention_request_id_ =
          [NSApp requestUserAttention:NSInformationalRequest];
      break;
    case apps::APP_SHIM_ATTENTION_NUM_TYPES:
      NOTREACHED();
  }
}

void AppShimController::Close() {
  [NSApp terminate:nil];
}

bool AppShimController::SendFocusApp(apps::AppShimFocusType focus_type,
                                     const std::vector<base::FilePath>& files) {
  if (launch_app_done_) {
    host_->FocusApp(focus_type, files);
    return true;
  }

  return false;
}

void AppShimController::ProfileMenuItemSelected(uint32_t index) {
  for (const auto& mojo_item : profile_menu_items_) {
    if (mojo_item->menu_index == index) {
      host_->ProfileSelectedFromMenu(mojo_item->profile_path);
      return;
    }
  }
}
