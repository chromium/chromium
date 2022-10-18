// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_DEVICE_ATTRIBUTES_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_DEVICE_ATTRIBUTES_ASH_H_

#include "chrome/browser/ash/policy/core/device_attributes.h"
#include "chromeos/crosapi/mojom/device_attributes.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace policy {
class FakeDeviceAttributes;
}

namespace crosapi {

// The ash-chrome implementation of the DeviceAttributes crosapi interface.
// This class must only be used from the main thread.
class DeviceAttributesAsh : public mojom::DeviceAttributes {
 public:
  DeviceAttributesAsh();
  DeviceAttributesAsh(const DeviceAttributesAsh&) = delete;
  DeviceAttributesAsh& operator=(const DeviceAttributesAsh&) = delete;
  ~DeviceAttributesAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::DeviceAttributes> receiver);

  // crosapi::mojom::DeviceAttributes:
  void GetDirectoryDeviceId(GetDirectoryDeviceIdCallback callback) override;
  void GetDeviceSerialNumber(GetDeviceSerialNumberCallback callback) override;
  void GetDeviceAssetId(GetDeviceAssetIdCallback callback) override;
  void GetDeviceAnnotatedLocation(
      GetDeviceAnnotatedLocationCallback callback) override;
  void GetDeviceHostname(GetDeviceHostnameCallback callback) override;
  void GetDeviceTypeForMetrics(
      GetDeviceTypeForMetricsCallback callback) override;

  void SetDeviceAttributesForTesting(
      std::unique_ptr<policy::FakeDeviceAttributes> attributes);

 private:
  using StringResult = mojom::DeviceAttributesStringResult;

  // This class supports any number of connections.
  mojo::ReceiverSet<mojom::DeviceAttributes> receivers_;

  std::unique_ptr<policy::DeviceAttributes> attributes_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_DEVICE_ATTRIBUTES_ASH_H_
