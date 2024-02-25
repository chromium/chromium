// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_BLUETOOTH_DISABLED_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_BLUETOOTH_DISABLED_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/phonehub/phone_hub_content_view.h"

namespace ash {

// An interstitial view representing an error state where the Phone Hub
// feature is not available because Bluetooth is turned off on this device.
class ASH_EXPORT BluetoothDisabledView : public PhoneHubContentView {
  METADATA_HEADER(BluetoothDisabledView, PhoneHubContentView)

 public:
  BluetoothDisabledView();
  BluetoothDisabledView(const BluetoothDisabledView&) = delete;
  BluetoothDisabledView& operator=(const BluetoothDisabledView&) = delete;
  ~BluetoothDisabledView() override;

  // PhoneHubContentView:
  phone_hub_metrics::Screen GetScreenForMetrics() const override;

 private:
  void LearnMoreButtonPressed();
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_BLUETOOTH_DISABLED_VIEW_H_
