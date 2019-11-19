// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/usb/arc_usb_host_bridge_delegate.h"

#include "chrome/browser/chromeos/usb/cros_usb_detector.h"
#include "components/arc/arc_util.h"

namespace arc {

void ArcUsbHostBridgeDelegate::AttachDevicesToArcVm() {
  auto* const usb_detector = chromeos::CrosUsbDetector::Get();
  if (usb_detector && IsArcVmEnabled())
    usb_detector->ConnectSharedDevicesOnVmStartup(kArcVmName);
}

}  // namespace arc
