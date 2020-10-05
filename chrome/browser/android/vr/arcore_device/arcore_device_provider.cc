// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/arcore_device_provider.h"

#include "chrome/browser/android/vr/arcore_device/arcore_java_utils.h"
#include "components/webxr/mailbox_to_surface_bridge_impl.h"
#include "device/vr/android/arcore/ar_image_transport.h"
#include "device/vr/android/arcore/arcore_device.h"
#include "device/vr/android/arcore/arcore_impl.h"
#include "device/vr/android/arcore/arcore_shim.h"

namespace device {

ArCoreDeviceProvider::ArCoreDeviceProvider() = default;

ArCoreDeviceProvider::~ArCoreDeviceProvider() = default;

void ArCoreDeviceProvider::Initialize(
    base::RepeatingCallback<void(mojom::XRDeviceId,
                                 mojom::VRDisplayInfoPtr,
                                 mojom::XRDeviceDataPtr,
                                 mojo::PendingRemote<mojom::XRRuntime>)>
        add_device_callback,
    base::RepeatingCallback<void(mojom::XRDeviceId)> remove_device_callback,
    base::OnceClosure initialization_complete) {
  if (vr::IsArCoreSupported()) {
    DVLOG(2) << __func__ << ": ARCore is supported, creating device";
    arcore_device_ = std::make_unique<ArCoreDevice>(
        std::make_unique<ArCoreImplFactory>(),
        std::make_unique<ArImageTransportFactory>(),
        std::make_unique<webxr::MailboxToSurfaceBridgeFactoryImpl>(),
        std::make_unique<vr::ArCoreJavaUtils>());

    add_device_callback.Run(
        arcore_device_->GetId(), arcore_device_->GetVRDisplayInfo(),
        arcore_device_->GetDeviceData(), arcore_device_->BindXRRuntime());
  }
  initialized_ = true;
  std::move(initialization_complete).Run();
}

bool ArCoreDeviceProvider::Initialized() {
  return initialized_;
}

}  // namespace device
