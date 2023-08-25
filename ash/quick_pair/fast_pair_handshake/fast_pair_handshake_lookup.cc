// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_lookup.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_handshake_lookup_impl.h"

namespace ash {
namespace quick_pair {

// static
FastPairHandshakeLookup* FastPairHandshakeLookup::GetInstance() {
  return FastPairHandshakeLookupImpl::GetImplInstance();
}

// static
void FastPairHandshakeLookup::SetCreateFunctionForTesting(
    CreateFunction create_function) {
  FastPairHandshakeLookupImpl::SetImplCreateFunctionForTesting(
      std::move(create_function));
}
}  // namespace quick_pair
}  // namespace ash
