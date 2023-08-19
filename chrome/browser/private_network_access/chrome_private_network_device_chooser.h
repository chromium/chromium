// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVATE_NETWORK_ACCESS_CHROME_PRIVATE_NETWORK_DEVICE_CHOOSER_H_
#define CHROME_BROWSER_PRIVATE_NETWORK_ACCESS_CHROME_PRIVATE_NETWORK_DEVICE_CHOOSER_H_

#include <string>

#include "base/functional/callback.h"

namespace content {
class RenderFrameHost;
}  // namespace content

class PrivateNetworkDeviceChooserController;

// Token representing a private network device chooser prompt. Destroying this
// object should cancel the prompt.
class ChromePrivateNetworkDeviceChooser {
 public:
  ChromePrivateNetworkDeviceChooser(const ChromePrivateNetworkDeviceChooser&) =
      delete;
  ChromePrivateNetworkDeviceChooser& operator=(
      const ChromePrivateNetworkDeviceChooser&) = delete;
  virtual ~ChromePrivateNetworkDeviceChooser();

 protected:
  ChromePrivateNetworkDeviceChooser();

  void ShowChooser(
      content::RenderFrameHost* render_frame_host,
      std::unique_ptr<PrivateNetworkDeviceChooserController> controller);
};

#endif  // CHROME_BROWSER_PRIVATE_NETWORK_ACCESS_CHROME_PRIVATE_NETWORK_DEVICE_CHOOSER_H_
