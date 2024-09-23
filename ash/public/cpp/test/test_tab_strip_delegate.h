// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_TEST_TAB_STRIP_DELEGATE_H_
#define ASH_PUBLIC_CPP_TEST_TEST_TAB_STRIP_DELEGATE_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/tab_strip_delegate.h"

namespace ash {

class TestTabStripDelegate : public ash::TabStripDelegate {
 public:
  TestTabStripDelegate();
  TestTabStripDelegate(TestTabStripDelegate&) = delete;
  TestTabStripDelegate& operator=(TestTabStripDelegate&) = delete;
  ~TestTabStripDelegate() override;
  std::vector<TabInfo> GetTabsListForWindow(
      aura::Window* window) const override;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_TEST_TAB_STRIP_DELEGATE_H_
