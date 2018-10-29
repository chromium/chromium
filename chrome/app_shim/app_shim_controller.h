// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_SHIM_APP_SHIM_CONTROLLER_H_
#define CHROME_APP_SHIM_APP_SHIM_CONTROLLER_H_

#include "base/files/file_path.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/common/mac/app_mode_common.h"
#include "chrome/common/mac/app_shim.mojom.h"
#include "chrome/common/mac/app_shim_param_traits.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/isolated_connection.h"

@class AppShimDelegate;

// The AppShimController is responsible for communication with the main Chrome
// process, and generally controls the lifetime of the app shim process.
class AppShimController : public chrome::mojom::AppShim {
 public:
  explicit AppShimController(const app_mode::ChromeAppModeInfo* app_mode_info);
  ~AppShimController() override;

  // Called when the main Chrome process responds to the Apple Event ping that
  // was sent, or when the ping fails (if |success| is false).
  void OnPingChromeReply(bool success);

  // Called |kPingChromeTimeoutSeconds| after startup, to allow a timeout on the
  // ping event to be detected.
  void OnPingChromeTimeout();

  // Connects to Chrome and sends a LaunchApp message.
  void InitBootstrapPipe();

  chrome::mojom::AppShimHost* host() const { return host_.get(); }

  // Called when the app is activated, e.g. by clicking on it in the dock, by
  // dropping a file on the dock icon, or by Cmd+Tabbing to it.
  // Returns whether the message was sent.
  bool SendFocusApp(apps::AppShimFocusType focus_type,
                    const std::vector<base::FilePath>& files);

 private:
  // Create a channel from |socket_path| and send a LaunchApp message.
  void CreateChannelAndSendLaunchApp(const base::FilePath& socket_path);
  // Builds main menu bar items.
  void SetUpMenu();
  void ChannelError(uint32_t custom_reason, const std::string& description);
  void BootstrapChannelError(uint32_t custom_reason,
                             const std::string& description);
  void LaunchAppDone(apps::AppShimLaunchResult result,
                     chrome::mojom::AppShimRequest app_shim_request);

  // chrome::mojom::AppShim implementation.
  void CreateViewsBridgeFactory(
      views_bridge_mac::mojom::BridgeFactoryAssociatedRequest request) override;
  void CreateContentNSViewBridgeFactory(
      content::mojom::NSViewBridgeFactoryAssociatedRequest request) override;
  void Hide() override;
  void UnhideWithoutActivation() override;
  void SetUserAttention(apps::AppShimAttentionType attention_type) override;

  // Terminates the app shim process.
  void Close();

  const app_mode::ChromeAppModeInfo* const app_mode_info_;

  mojo::IsolatedConnection bootstrap_mojo_connection_;
  chrome::mojom::AppShimHostBootstrapPtr host_bootstrap_;

  mojo::Binding<chrome::mojom::AppShim> shim_binding_;
  chrome::mojom::AppShimHostPtr host_;
  chrome::mojom::AppShimHostRequest host_request_;

  base::scoped_nsobject<AppShimDelegate> delegate_;
  bool launch_app_done_;
  bool ping_chrome_reply_received_;
  NSInteger attention_request_id_;

  DISALLOW_COPY_AND_ASSIGN(AppShimController);
};

#endif  // CHROME_APP_SHIM_APP_SHIM_CONTROLLER_H_
