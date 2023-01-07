// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DISABLED_DETAILED_VIEW_H_
#define ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DISABLED_DETAILED_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace views {
class View;
}  // namespace views

namespace ash {

// This class encapsulates the logic of configuring the view shown for the
// detailed Bluetooth page within the quick settings when Bluetooth is disabled.
class ASH_EXPORT BluetoothDisabledDetailedView : public views::View {
 public:
  BluetoothDisabledDetailedView();
  BluetoothDisabledDetailedView(const BluetoothDisabledDetailedView&) = delete;
  BluetoothDisabledDetailedView& operator=(
      const BluetoothDisabledDetailedView&) = delete;
  ~BluetoothDisabledDetailedView() override = default;

 private:
  // views::View:
  const char* GetClassName() const override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DISABLED_DETAILED_VIEW_H_
