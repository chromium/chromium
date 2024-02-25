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
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/permissions/chooser_controller.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "third_party/blink/public/mojom/private_network_device/private_network_device.mojom.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;
}

class PrivateNetworkDevicePermissionContext;

// PrivateNetworkDeviceChooserController creates a chooser for Private Network
// Device.
class PrivateNetworkDeviceChooserController
    : public permissions::ChooserController {
 public:
  using DoneCallback = base::OnceCallback<void(
      PrivateNetworkDevicePermissionContext* permission_context,
      const url::Origin&,
      const blink::mojom::PrivateNetworkDevice&,
      bool)>;
  PrivateNetworkDeviceChooserController(
      content::RenderFrameHost* render_frame_host,
      blink::mojom::PrivateNetworkDevicePtr device,
      DoneCallback callback);

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

  void ReplaceDeviceForTesting(blink::mojom::PrivateNetworkDevicePtr device);
  void RunCallback(bool permission_granted);

 private:
  bool DisplayDevice(const blink::mojom::PrivateNetworkDevice& device) const;

  url::Origin origin_;

  base::WeakPtr<PrivateNetworkDevicePermissionContext> permission_context_;
  blink::mojom::PrivateNetworkDevicePtr device_;
  DoneCallback callback_;

  base::WeakPtrFactory<PrivateNetworkDeviceChooserController> weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_PRIVATE_NETWORK_ACCESS_PRIVATE_NETWORK_DEVICE_CHOOSER_CONTROLLER_H_
