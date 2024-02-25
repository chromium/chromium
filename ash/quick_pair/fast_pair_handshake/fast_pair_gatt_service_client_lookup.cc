// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client_lookup.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_gatt_service_client_lookup_impl.h"

namespace ash {
namespace quick_pair {

// static
FastPairGattServiceClientLookup*
FastPairGattServiceClientLookup::GetInstance() {
  return FastPairGattServiceClientLookupImpl::GetImplInstance();
}

}  // namespace quick_pair
}  // namespace ash
