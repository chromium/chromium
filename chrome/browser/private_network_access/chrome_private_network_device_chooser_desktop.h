// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVATE_NETWORK_ACCESS_CHROME_PRIVATE_NETWORK_DEVICE_CHOOSER_DESKTOP_H_
#define CHROME_BROWSER_PRIVATE_NETWORK_ACCESS_CHROME_PRIVATE_NETWORK_DEVICE_CHOOSER_DESKTOP_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/private_network_access/chrome_private_network_device_chooser.h"

namespace content {
class RenderFrameHost;
}

class PrivateNetworkDeviceChooserController;

// ChromePrivateNetworkDeviceChooserDesktop is a permission chooser prompt for
// desktop devices. Permission chooser prompt for private network access only
// shows up after pricate network access preflight response and have one device
// to show at a time.
class ChromePrivateNetworkDeviceChooserDesktop
    : public ChromePrivateNetworkDeviceChooser {
 public:
  ChromePrivateNetworkDeviceChooserDesktop();
  ~ChromePrivateNetworkDeviceChooserDesktop() override;

  static std::unique_ptr<ChromePrivateNetworkDeviceChooserDesktop> Create(
      content::RenderFrameHost* render_frame_host,
      std::unique_ptr<PrivateNetworkDeviceChooserController> controller);

  void ShowChooser(content::RenderFrameHost* render_frame_host,
                   std::unique_ptr<PrivateNetworkDeviceChooserController>
                       controller) override;

 private:
  base::ScopedClosureRunner closure_runner_;
};

#endif  // CHROME_BROWSER_PRIVATE_NETWORK_ACCESS_CHROME_PRIVATE_NETWORK_DEVICE_CHOOSER_DESKTOP_H_
