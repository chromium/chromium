// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_COMMON_CONSTANTS_H_
#define ASH_QUICK_PAIR_COMMON_CONSTANTS_H_

#include "base/component_export.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace ash {
namespace quick_pair {

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
extern const device::BluetoothUUID kFastPairBluetoothUuid;

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_COMMON_CONSTANTS_H_
