// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_lookup.h"
#include "ash/constants/ash_features.h"
#include "ash/quick_pair/fast_pair_handshake/async_fast_pair_handshake_lookup_impl.h"
#include "ash/quick_pair/fast_pair_handshake/fake_fast_pair_handshake_lookup.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_lookup_impl.h"

namespace ash {
namespace quick_pair {

bool is_unittest = false;

// static
FastPairHandshakeLookup* FastPairHandshakeLookup::GetInstance() {
  if (is_unittest) {
    // TODO(b/265853116): Remove the GetFakeInstance() and provide the fake via
    // constructor injection instead so we don't have to include the fake in the
    // production source set.
    return FakeFastPairHandshakeLookup::GetFakeInstance();
  }

  if (!ash::features::IsFastPairHandshakeLongTermRefactorEnabled()) {
    return FastPairHandshakeLookupImpl::GetImplInstance();
  } else {
    return AsyncFastPairHandshakeLookupImpl::GetAsyncInstance();
  }
}

// static
void FastPairHandshakeLookup::UseFakeInstance() {
  is_unittest = true;
}
}  // namespace quick_pair
}  // namespace ash
