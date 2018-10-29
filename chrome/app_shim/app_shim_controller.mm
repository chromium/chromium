// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app_shim/app_shim_controller.h"

#import <Cocoa/Cocoa.h>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app_shim/app_shim_delegate.h"
#include "chrome/browser/ui/cocoa/main_menu_builder.h"
#include "content/public/browser/ns_view_bridge_factory_impl.h"
#include "content/public/common/ns_view_bridge_factory.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views_bridge_mac/bridge_factory_impl.h"
#include "ui/views_bridge_mac/mojo/bridge_factory.mojom.h"

AppShimController::AppShimController(
    const app_mode::ChromeAppModeInfo* app_mode_info)
    : app_mode_info_(app_mode_info),
      shim_binding_(this),
      host_request_(mojo::MakeRequest(&host_)),
      delegate_([[AppShimDelegate alloc] init]),
      launch_app_done_(false),
      ping_chrome_reply_received_(false),
      attention_request_id_(0) {
  // Since AppShimController is created before the main message loop starts,
  // NSApp will not be set, so use sharedApplication.
  NSApplication* sharedApplication = [NSApplication sharedApplication];
  [sharedApplication setDelegate:delegate_];
}

AppShimController::~AppShimController() {
  // Un-set the delegate since NSApplication does not retain it.
  NSApplication* sharedApplication = [NSApplication sharedApplication];
  [sharedApplication setDelegate:nil];
}

void AppShimController::OnPingChromeReply(bool success) {
  ping_chrome_reply_received_ = true;
  if (!success) {
    [NSApp terminate:nil];
    return;
  }

  InitBootstrapPipe();
}

void AppShimController::OnPingChromeTimeout() {
  if (!ping_chrome_reply_received_)
    [NSApp terminate:nil];
}

void AppShimController::InitBootstrapPipe() {
  SetUpMenu();

  // Chrome will relaunch shims when relaunching apps.
  [NSApp disableRelaunchOnLogin];

  // The user_data_dir for shims actually contains the app_data_path.
  // I.e. <user_data_dir>/<profile_dir>/Web Applications/_crx_extensionid/
  base::FilePath user_data_dir =
      app_mode_info_->user_data_dir.DirName().DirName().DirName();
  CHECK(!user_data_dir.empty());

  base::FilePath symlink_path =
      user_data_dir.Append(app_mode::kAppShimSocketSymlinkName);
  base::FilePath socket_path;
  if (base::ReadSymbolicLink(symlink_path, &socket_path)) {
    app_mode::VerifySocketPermissions(socket_path);
    CreateChannelAndSendLaunchApp(socket_path);
  } else {
    // The path in the user data dir is not a symlink, try connecting directly.
    CreateChannelAndSendLaunchApp(symlink_path);
  }
}

void AppShimController::CreateChannelAndSendLaunchApp(
    const base::FilePath& socket_path) {
  mojo::ScopedMessagePipeHandle message_pipe =
      bootstrap_mojo_connection_.Connect(
          mojo::NamedPlatformChannel::ConnectToServer(socket_path.value()));
  host_bootstrap_ = chrome::mojom::AppShimHostBootstrapPtr(
      chrome::mojom::AppShimHostBootstrapPtrInfo(std::move(message_pipe), 0));
  host_bootstrap_.set_connection_error_with_reason_handler(base::BindOnce(
      &AppShimController::BootstrapChannelError, base::Unretained(this)));

  bool launched_by_chrome = base::CommandLine::ForCurrentProcess()->HasSwitch(
      app_mode::kLaunchedByChromeProcessId);
  apps::AppShimLaunchType launch_type =
      launched_by_chrome ? apps::APP_SHIM_LAUNCH_REGISTER_ONLY
                         : apps::APP_SHIM_LAUNCH_NORMAL;

  [delegate_ setController:this];

  std::vector<base::FilePath> files;
  [delegate_ getFilesToOpenAtStartup:&files];

  host_bootstrap_->LaunchApp(std::move(host_request_),
                             app_mode_info_->profile_dir,
                             app_mode_info_->app_mode_id, launch_type, files,
                             base::BindOnce(&AppShimController::LaunchAppDone,
                                            base::Unretained(this)));
}

void AppShimController::SetUpMenu() {
  chrome::BuildMainMenu(NSApp, delegate_, app_mode_info_->app_mode_name, true);
}

void AppShimController::BootstrapChannelError(uint32_t custom_reason,
                                              const std::string& description) {
  // The bootstrap channel is expected to close after sending LaunchAppDone.
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

void AppShimController::LaunchAppDone(
    apps::AppShimLaunchResult result,
    chrome::mojom::AppShimRequest app_shim_request) {
  if (result != apps::APP_SHIM_LAUNCH_SUCCESS) {
    Close();
    return;
  }
  shim_binding_.Bind(std::move(app_shim_request),
                     ui::WindowResizeHelperMac::Get()->task_runner());
  shim_binding_.set_connection_error_with_reason_handler(
      base::BindOnce(&AppShimController::ChannelError, base::Unretained(this)));

  std::vector<base::FilePath> files;
  if ([delegate_ getFilesToOpenAtStartup:&files])
    SendFocusApp(apps::APP_SHIM_FOCUS_OPEN_FILES, files);

  launch_app_done_ = true;
  host_bootstrap_.reset();
}

void AppShimController::CreateViewsBridgeFactory(
    views_bridge_mac::mojom::BridgeFactoryAssociatedRequest request) {
  views_bridge_mac::BridgeFactoryImpl::Get()->BindRequest(std::move(request));
}

void AppShimController::CreateContentNSViewBridgeFactory(
    content::mojom::NSViewBridgeFactoryAssociatedRequest request) {
  content::NSViewBridgeFactoryImpl::Get()->BindRequest(std::move(request));
}

void AppShimController::Hide() {
  [NSApp hide:nil];
}

void AppShimController::UnhideWithoutActivation() {
  [NSApp unhideWithoutActivation];
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
  [delegate_ terminateNow];
}

bool AppShimController::SendFocusApp(apps::AppShimFocusType focus_type,
                                     const std::vector<base::FilePath>& files) {
  if (launch_app_done_) {
    host_->FocusApp(focus_type, files);
    return true;
  }

  return false;
}
