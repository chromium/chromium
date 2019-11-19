// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_USB_ARC_USB_HOST_BRIDGE_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_USB_ARC_USB_HOST_BRIDGE_DELEGATE_H_

#include "components/arc/usb/usb_host_bridge.h"

namespace arc {

// Implementation of the ArcUsbHostBridge::Delegate interface.
class ArcUsbHostBridgeDelegate : public ArcUsbHostBridge::Delegate {
 public:
  // ArcUsbHostBridge::Delegate:
  void AttachDevicesToArcVm() override;
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_USB_ARC_USB_HOST_BRIDGE_DELEGATE_H_
