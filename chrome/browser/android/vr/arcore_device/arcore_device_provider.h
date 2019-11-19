// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_DEVICE_PROVIDER_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_DEVICE_PROVIDER_H_

#include <memory>
#include "base/macros.h"
#include "device/vr/vr_device_provider.h"
#include "device/vr/vr_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace device {

class ArCoreDevice;

class ArCoreDeviceProvider : public VRDeviceProvider {
 public:
  ArCoreDeviceProvider();
  ~ArCoreDeviceProvider() override;
  void Initialize(
      base::RepeatingCallback<void(mojom::XRDeviceId,
                                   mojom::VRDisplayInfoPtr,
                                   mojo::PendingRemote<mojom::XRRuntime>)>
          add_device_callback,
      base::RepeatingCallback<void(mojom::XRDeviceId)> remove_device_callback,
      base::OnceClosure initialization_complete) override;
  bool Initialized() override;

 private:
  std::unique_ptr<ArCoreDevice> arcore_device_;
  bool initialized_ = false;
  DISALLOW_COPY_AND_ASSIGN(ArCoreDeviceProvider);
};

}  // namespace device

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_DEVICE_PROVIDER_H_
