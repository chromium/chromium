// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/mock_fast_pair_repository.h"

namespace ash {
namespace quick_pair {

MockFastPairRepository::MockFastPairRepository() {
  SetInstanceForTesting(this);
}

MockFastPairRepository::~MockFastPairRepository() {
  SetInstanceForTesting(nullptr);
}

}  // namespace quick_pair
}  // namespace ash
