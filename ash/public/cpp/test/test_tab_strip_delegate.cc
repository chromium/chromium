// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/test_tab_strip_delegate.h"

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/tab_strip_delegate.h"

namespace ash {

TestTabStripDelegate::TestTabStripDelegate() = default;
TestTabStripDelegate::~TestTabStripDelegate() = default;

std::vector<TabInfo> TestTabStripDelegate::GetTabsListForWindow(
    aura::Window* window) const {
  return {};
}
}  // namespace ash
