// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/quick_pair_browser_delegate.h"

#include "base/check.h"
#include "base/check_op.h"

namespace ash {
namespace quick_pair {

namespace {

QuickPairBrowserDelegate* g_instance = nullptr;
QuickPairBrowserDelegate* g_test_instance = nullptr;

}  // namespace

QuickPairBrowserDelegate::QuickPairBrowserDelegate() = default;
QuickPairBrowserDelegate::~QuickPairBrowserDelegate() = default;

// static
QuickPairBrowserDelegate* QuickPairBrowserDelegate::Get() {
  if (g_test_instance) {
    return g_test_instance;
  }

  DCHECK(g_instance);
  return g_instance;
}

// static
void QuickPairBrowserDelegate::SetInstance(QuickPairBrowserDelegate* instance) {
  // We either need to set an unset instance, or reset a set instance.
  DCHECK((!g_instance && instance) || (g_instance && !instance));
  g_instance = instance;
}

// static
void QuickPairBrowserDelegate::SetInstanceForTesting(
    QuickPairBrowserDelegate* instance) {
  g_test_instance = instance;
}

}  // namespace quick_pair
}  // namespace ash
