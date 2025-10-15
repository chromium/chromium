// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_MOCK_LOCAL_HOTKEY_PANEL_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_MOCK_LOCAL_HOTKEY_PANEL_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/widget/local_hotkey_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/geometry/point.h"

namespace glic {

class MockLocalHotkeyPanel
    : public testing::NiceMock<LocalHotkeyManager::Panel> {
 public:
  MockLocalHotkeyPanel();
  ~MockLocalHotkeyPanel() override;

  MOCK_METHOD(void, FocusIfOpen, (), (override));
  MOCK_METHOD(bool, HasFocus, (), (override));
  MOCK_METHOD(bool, IsShowing, (), (const, override));
  MOCK_METHOD(void, Close, (), (override));
  MOCK_METHOD(bool, ActivateBrowser, (), (override));
  MOCK_METHOD(void, ShowTitleBarContextMenuAt, (gfx::Point), (override));
  MOCK_METHOD(base::WeakPtr<views::View>, GetView, (), (override));

  base::WeakPtr<LocalHotkeyManager::Panel> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockLocalHotkeyPanel> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_MOCK_LOCAL_HOTKEY_PANEL_H_
