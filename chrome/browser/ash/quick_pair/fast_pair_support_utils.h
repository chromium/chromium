// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_QUICK_PAIR_FAST_PAIR_SUPPORT_UTILS_H_
#define CHROME_BROWSER_ASH_QUICK_PAIR_FAST_PAIR_SUPPORT_UTILS_H_

#include "base/memory/scoped_refptr.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace ash {
namespace quick_pair {

// Returns true if the device has the minimum requirements to support
// fast pair, such as hardware support and available Google API keys.
bool IsFastPairSupported(scoped_refptr<device::BluetoothAdapter> adapter);

}  // namespace quick_pair
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_QUICK_PAIR_FAST_PAIR_SUPPORT_UTILS_H_
