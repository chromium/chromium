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

}  // namespace

// static
QuickPairBrowserDelegate* QuickPairBrowserDelegate::Get() {
  DCHECK(g_instance);
  return g_instance;
}

QuickPairBrowserDelegate::QuickPairBrowserDelegate() {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

QuickPairBrowserDelegate::~QuickPairBrowserDelegate() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace quick_pair
}  // namespace ash
