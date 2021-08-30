// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_disabled_detailed_view.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"

namespace ash {

BluetoothDisabledDetailedView::BluetoothDisabledDetailedView() {
  DCHECK(ash::features::IsBluetoothRevampEnabled());
  AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH_DISABLED)));
}

const char* BluetoothDisabledDetailedView::GetClassName() const {
  return "BluetoothDisabledDetailedView";
}

}  // namespace ash
