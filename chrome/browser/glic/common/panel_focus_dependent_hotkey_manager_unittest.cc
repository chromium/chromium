// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/common/panel_focus_dependent_hotkey_manager.h"

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/glic/common/local_hotkey_manager.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/test_support/mock_local_hotkey_panel.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/common/chrome_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/view.h"

namespace glic {

namespace {

class TestView : public views::View {
 public:
  TestView() = default;
  ~TestView() override = default;

  base::WeakPtr<TestView> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<TestView> weak_ptr_factory_{this};
};

class PanelFocusDependentHotkeyManagerTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    mock_panel_ = std::make_unique<MockLocalHotkeyPanel>();
    test_view_ = std::make_unique<TestView>();
    ON_CALL(*mock_panel_, GetView())
        .WillByDefault(testing::Return(test_view_->GetWeakPtr()));
    ON_CALL(*mock_panel_, HasFocus()).WillByDefault(testing::Return(true));
    manager_ = std::make_unique<PanelFocusDependentHotkeyManager>(
        mock_panel_->GetWeakPtr());
    registration_delegate_ = std::make_unique<ViewScopedRegistrationDelegate>(
        mock_panel_->GetWeakPtr());
  }

  void TearDown() override {
    manager_.reset();
    registration_delegate_.reset();
    test_view_.reset();
    mock_panel_.reset();
    testing::Test::TearDown();
  }

 protected:
  std::unique_ptr<MockLocalHotkeyPanel> mock_panel_;
  std::unique_ptr<TestView> test_view_;
  std::unique_ptr<PanelFocusDependentHotkeyManager> manager_;
  std::unique_ptr<ViewScopedRegistrationDelegate> registration_delegate_;
  base::UserActionTester user_action_tester_;
};

}  // namespace

TEST_F(PanelFocusDependentHotkeyManagerTest, AcceleratorPressedClose) {
  EXPECT_CALL(*mock_panel_, Close(testing::_)).Times(1);
  EXPECT_TRUE(
      manager_->AcceleratorPressed(LocalHotkeyManager::Command::kClose));
}

TEST_F(PanelFocusDependentHotkeyManagerTest, AcceleratorPressedFocusToggle) {
  EXPECT_CALL(*mock_panel_, ActivateBrowser()).WillOnce(testing::Return(true));
  EXPECT_TRUE(
      manager_->AcceleratorPressed(LocalHotkeyManager::Command::kFocusToggle));
  EXPECT_EQ(1, user_action_tester_.GetActionCount("Glic.FocusHotKey"));
}

TEST_F(PanelFocusDependentHotkeyManagerTest,
       AcceleratorPressedFocusToggleNoBrowserToActivate) {
  EXPECT_CALL(*mock_panel_, ActivateBrowser()).WillOnce(testing::Return(false));
  EXPECT_FALSE(
      manager_->AcceleratorPressed(LocalHotkeyManager::Command::kFocusToggle));
  EXPECT_EQ(0, user_action_tester_.GetActionCount("Glic.FocusHotKey"));
}

TEST_F(PanelFocusDependentHotkeyManagerTest, AcceleratorPressedZoomIn) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicClientZoomControl);

  EXPECT_CALL(*mock_panel_, Zoom(mojom::ZoomAction::kZoomIn)).Times(1);
  EXPECT_TRUE(
      manager_->AcceleratorPressed(LocalHotkeyManager::Command::kZoomIn));
}

TEST_F(PanelFocusDependentHotkeyManagerTest, AcceleratorPressedZoomOut) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicClientZoomControl);

  EXPECT_CALL(*mock_panel_, Zoom(mojom::ZoomAction::kZoomOut)).Times(1);
  EXPECT_TRUE(
      manager_->AcceleratorPressed(LocalHotkeyManager::Command::kZoomOut));
}

TEST_F(PanelFocusDependentHotkeyManagerTest, AcceleratorPressedZoomReset) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicClientZoomControl);

  EXPECT_CALL(*mock_panel_, Zoom(mojom::ZoomAction::kReset)).Times(1);
  EXPECT_TRUE(
      manager_->AcceleratorPressed(LocalHotkeyManager::Command::kZoomReset));
}

TEST_F(PanelFocusDependentHotkeyManagerTest, ZoomDisabledByFlag) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kGlicClientZoomControl);

  EXPECT_CALL(*mock_panel_, Zoom(testing::_)).Times(0);
  EXPECT_FALSE(
      manager_->AcceleratorPressed(LocalHotkeyManager::Command::kZoomIn));
  EXPECT_FALSE(
      manager_->AcceleratorPressed(LocalHotkeyManager::Command::kZoomOut));
  EXPECT_FALSE(
      manager_->AcceleratorPressed(LocalHotkeyManager::Command::kZoomReset));
}

#if BUILDFLAG(IS_WIN)
TEST_F(PanelFocusDependentHotkeyManagerTest,
       AcceleratorPressedTitleBarContextMenu) {
  EXPECT_CALL(*mock_panel_, ShowTitleBarContextMenuAt(gfx::Point())).Times(1);
  EXPECT_TRUE(manager_->AcceleratorPressed(
      LocalHotkeyManager::Command::kTitleBarContextMenu));
}
#endif

TEST_F(PanelFocusDependentHotkeyManagerTest, CreateScopedHotkeyRegistration) {
  auto local_mock_panel = std::make_unique<MockLocalHotkeyPanel>();
  auto test_view = std::make_unique<TestView>();
  ON_CALL(*local_mock_panel, GetView())
      .WillByDefault(testing::Return(test_view->GetWeakPtr()));

  auto local_registration_delegate =
      std::make_unique<ViewScopedRegistrationDelegate>(
          local_mock_panel->GetWeakPtr());

  ui::Accelerator test_accel(ui::VKEY_A, ui::EF_NONE);

  auto registration =
      local_registration_delegate->CreateScopedHotkeyRegistration(test_accel,
                                                                  nullptr);
  ASSERT_TRUE(registration);
  EXPECT_THAT(test_view->GetAccelerators(), testing::ElementsAre(test_accel));

  registration.reset();
  EXPECT_EQ(test_view->GetAccelerators().size(), 0u);
}

}  // namespace glic
