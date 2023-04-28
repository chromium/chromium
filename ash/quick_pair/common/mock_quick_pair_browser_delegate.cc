// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/mock_quick_pair_browser_delegate.h"

namespace ash {
namespace quick_pair {

MockQuickPairBrowserDelegate::MockQuickPairBrowserDelegate() {
  SetInstance(this);
}

MockQuickPairBrowserDelegate::~MockQuickPairBrowserDelegate() {
  SetInstance(nullptr);
}

}  // namespace quick_pair
}  // namespace ash
