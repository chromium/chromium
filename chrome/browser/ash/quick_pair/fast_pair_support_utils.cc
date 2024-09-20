// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/quick_pair/fast_pair_support_utils.h"

#include "ash/constants/ash_features.h"
#include "ash/quick_pair/feature_status_tracker/fast_pair_support_utils.h"
#include "base/functional/bind.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "google_apis/google_api_keys.h"

namespace ash {
namespace quick_pair {

bool IsFastPairSupported(scoped_refptr<device::BluetoothAdapter> adapter) {
  return HasHardwareSupport(adapter) && google_apis::HasAPIKeyConfigured() &&
         google_apis::IsGoogleChromeAPIKeyUsed();
}

}  // namespace quick_pair
}  // namespace ash
