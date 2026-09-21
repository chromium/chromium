// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_COMMON_CONSTANTS_H_
#define ASH_QUICK_PAIR_COMMON_CONSTANTS_H_

namespace ash::quick_pair {

// The 16-bit Fast Pair GATT service UUID. device::BluetoothUUID is not
// constexpr constructible, so callers build one on demand from this.
inline constexpr char kFastPairBluetoothUuid[] = "0xFE2C";

}  // namespace ash::quick_pair

#endif  // ASH_QUICK_PAIR_COMMON_CONSTANTS_H_
