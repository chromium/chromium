// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/usb/arc_usb_host_bridge_delegate.h"

#include "ash/components/arc/arc_util.h"
#include "chrome/browser/ash/usb/cros_usb_detector.h"

namespace arc {

void ArcUsbHostBridgeDelegate::AttachDevicesToArcVm() {
  auto* const usb_detector = ash::CrosUsbDetector::Get();
  if (usb_detector && IsArcVmEnabled())
    usb_detector->ConnectSharedDevicesOnVmStartup(kArcVmName);
}

}  // namespace arc
