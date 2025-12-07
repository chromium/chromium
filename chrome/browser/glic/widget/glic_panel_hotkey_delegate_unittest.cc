// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_panel_hotkey_delegate.h"

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/test/metrics/user_action_tester.h"
#include "build/build_config.h"
#include "chrome/browser/glic/test_support/mock_local_hotkey_panel.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/local_hotkey_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/view.h"

namespace glic {

namespace {

// MockGlicView is needed because the delegate interacts with the view provided
// by the controller to manage accelerators.
class MockGlicView : public views::View {
 public:
  MockGlicView() = default;
  ~MockGlicView() override = default;

  base::WeakPtr<MockGlicView> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockGlicView> weak_ptr_factory_{this};
};

class GlicPanelHotkeyDelegateTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    mock_panel_ = std::make_unique<MockLocalHotkeyPanel>();
    mock_glic_view_ = std::make_unique<MockGlicView>();
    ON_CALL(*mock_panel_, GetView())
        .WillByDefault(testing::Return(mock_glic_view_->GetWeakPtr()));
    ON_CALL(*mock_panel_, HasFocus()).WillByDefault(testing::Return(true));
    delegate_ =
        std::make_unique<GlicPanelHotkeyDelegate>(mock_panel_->GetWeakPtr());
  }

  void TearDown() override {
    delegate_.reset();
    mock_glic_view_.reset();
    mock_panel_.reset();
    testing::Test::TearDown();
  }

 protected:
  std::unique_ptr<MockLocalHotkeyPanel> mock_panel_;
  std::unique_ptr<MockGlicView> mock_glic_view_;
  std::unique_ptr<GlicPanelHotkeyDelegate> delegate_;
  base::UserActionTester user_action_tester_;
};

}  // namespace

TEST_F(GlicPanelHotkeyDelegateTest, GetSupportedHotkeys) {
  auto supported = delegate_->GetSupportedHotkeys();
  const std::vector<LocalHotkeyManager::Hotkey> kExpectedHotkeys = {
      LocalHotkeyManager::Hotkey::kClose,
      LocalHotkeyManager::Hotkey::kFocusToggle,
#if BUILDFLAG(IS_WIN)
      LocalHotkeyManager::Hotkey::kTitleBarContextMenu,
#endif
  };
  EXPECT_THAT(supported, testing::ElementsAreArray(kExpectedHotkeys));
}

TEST_F(GlicPanelHotkeyDelegateTest, AcceleratorPressedClose) {
  EXPECT_CALL(*mock_panel_, Close()).Times(1);
  EXPECT_TRUE(
      delegate_->AcceleratorPressed(LocalHotkeyManager::Hotkey::kClose));
}

TEST_F(GlicPanelHotkeyDelegateTest, AcceleratorPressedFocusToggle) {
  EXPECT_CALL(*mock_panel_, ActivateBrowser()).WillOnce(testing::Return(true));
  EXPECT_TRUE(
      delegate_->AcceleratorPressed(LocalHotkeyManager::Hotkey::kFocusToggle));
  EXPECT_EQ(1, user_action_tester_.GetActionCount("Glic.FocusHotKey"));
}

TEST_F(GlicPanelHotkeyDelegateTest,
       AcceleratorPressedFocusToggleNoBrowserToActivate) {
  EXPECT_CALL(*mock_panel_, ActivateBrowser()).WillOnce(testing::Return(false));
  EXPECT_FALSE(
      delegate_->AcceleratorPressed(LocalHotkeyManager::Hotkey::kFocusToggle));
  EXPECT_EQ(0, user_action_tester_.GetActionCount("Glic.FocusHotKey"));
}

#if BUILDFLAG(IS_WIN)
TEST_F(GlicPanelHotkeyDelegateTest, AcceleratorPressedTitleBarContextMenu) {
  EXPECT_CALL(*mock_panel_, ShowTitleBarContextMenuAt(gfx::Point())).Times(1);
  EXPECT_TRUE(delegate_->AcceleratorPressed(
      LocalHotkeyManager::Hotkey::kTitleBarContextMenu));
}
#endif

TEST_F(GlicPanelHotkeyDelegateTest, CreateScopedHotkeyRegistration) {
  ui::Accelerator test_accel(ui::VKEY_A, ui::EF_NONE);

  auto registration =
      delegate_->CreateScopedHotkeyRegistration(test_accel, nullptr);
  ASSERT_TRUE(registration);
  EXPECT_THAT(mock_glic_view_->GetAccelerators(),
              testing::ElementsAre(test_accel));

  registration.reset();
  EXPECT_EQ(mock_glic_view_->GetAccelerators().size(), 0u);
}

TEST_F(GlicPanelHotkeyDelegateTest, MakeGlicWindowHotkeyManager) {
  auto manager = MakeGlicWindowHotkeyManager(mock_panel_->GetWeakPtr());
  EXPECT_TRUE(manager);
}

}  // namespace glic
