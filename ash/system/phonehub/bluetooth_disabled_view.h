// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_BLUETOOTH_DISABLED_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_BLUETOOTH_DISABLED_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/controls/button/button.h"

namespace ash {

class PhoneHubInterstitialView;

// An interstitial view representing an error state where the Phone Hub
// feature is not available because Bluetooth is turned off on this device.
class ASH_EXPORT BluetoothDisabledView : public views::View,
                                         public views::ButtonListener {
 public:
  METADATA_HEADER(BluetoothDisabledView);

  BluetoothDisabledView();
  BluetoothDisabledView(const BluetoothDisabledView&) = delete;
  BluetoothDisabledView& operator=(const BluetoothDisabledView&) = delete;
  ~BluetoothDisabledView() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  PhoneHubInterstitialView* content_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_BLUETOOTH_DISABLED_VIEW_H_
