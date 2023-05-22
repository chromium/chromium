// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVATE_NETWORK_ACCESS_PRIVATE_NETWORK_DEVICE_CHOOSER_CONTROLLER_H_
#define CHROME_BROWSER_PRIVATE_NETWORK_ACCESS_PRIVATE_NETWORK_DEVICE_CHOOSER_CONTROLLER_H_

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/private_network_access/chrome_private_network_device_chooser.h"
#include "components/permissions/chooser_controller.h"
#include "third_party/blink/public/mojom/private_network_device/private_network_device.mojom.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;
}

// PrivateNetworkDeviceChooserController creates a chooser for Private Network
// Device.
class PrivateNetworkDeviceChooserController
    : public permissions::ChooserController {
 public:
  PrivateNetworkDeviceChooserController(
      content::RenderFrameHost* render_frame_host,
      std::unique_ptr<blink::mojom::PrivateNetworkDevice> device,
      const ChromePrivateNetworkDeviceChooser::EventHandler& event_handler);

  PrivateNetworkDeviceChooserController(
      const PrivateNetworkDeviceChooserController&) = delete;
  PrivateNetworkDeviceChooserController& operator=(
      const PrivateNetworkDeviceChooserController&) = delete;

  ~PrivateNetworkDeviceChooserController() override;

  // Permission::ChooserController:
  std::u16string GetOkButtonLabel() const override;
  std::u16string GetNoOptionsText() const override;
  std::pair<std::u16string, std::u16string> GetThrobberLabelAndTooltip()
      const override;
  size_t NumOptions() const override;
  std::u16string GetOption(size_t index) const override;
  void Select(const std::vector<size_t>& indices) override;
  void OpenHelpCenterUrl() const override;
  void Cancel() override;
  void Close() override;

  void ReplaceDeviceForTesting(
      std::unique_ptr<blink::mojom::PrivateNetworkDevice> device);

 private:
  bool DisplayDevice(const blink::mojom::PrivateNetworkDevice& device) const;

  url::Origin origin_;

  std::unique_ptr<blink::mojom::PrivateNetworkDevice> device_;
  const ChromePrivateNetworkDeviceChooser::EventHandler& event_handler_;

  base::WeakPtrFactory<PrivateNetworkDeviceChooserController> weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_PRIVATE_NETWORK_ACCESS_PRIVATE_NETWORK_DEVICE_CHOOSER_CONTROLLER_H_
