// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_window_hotkey_delegate.h"

#include <memory>
#include <vector>

#include "base/containers/contains.h"
#include "base/memory/weak_ptr.h"
#include "base/test/metrics/user_action_tester.h"
#include "build/build_config.h"
#include "chrome/browser/glic/test_support/mock_glic_window_controller.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/glic/widget/local_hotkey_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/view.h"

namespace glic {

namespace {

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

class MockBrowserWindow : public TestBrowserWindow {
 public:
  MOCK_METHOD(void, Activate, ());
};

class GlicWindowHotkeyDelegateTest : public testing::Test {
 public:
  GlicWindowHotkeyDelegateTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    testing::Test::SetUp();

    // Profile needed for Browser::Create
    profile_ = std::make_unique<TestingProfile>();

    // Create the mock window first
    mock_browser_window_ = std::make_unique<MockBrowserWindow>();

    // Create the fake Browser, providing the mock window.
    // This Browser instance will be added to the BrowserList automatically.
    Browser::CreateParams params(profile_.get(), true);
    params.window = mock_browser_window_.get();  // Assign raw pointer
    fake_browser_ = std::unique_ptr<Browser>(Browser::Create(params));

    // Create other mocks
    mock_controller_ = std::make_unique<MockGlicWindowController>();
    mock_glic_view_ = std::make_unique<MockGlicView>();
    ON_CALL(*mock_controller_, GetGlicViewAsView())
        .WillByDefault(testing::Return(mock_glic_view_->GetWeakPtr()));

    delegate_ = std::make_unique<GlicWindowHotkeyDelegate>(
        mock_controller_->GetWeakPtr());
  }

  void TearDown() override {
    delegate_.reset();
    mock_glic_view_.reset();
    mock_controller_.reset();
    // Destroy the browser before the window it points to.
    // The Browser destructor should remove it from the BrowserList.
    fake_browser_.reset();
    mock_browser_window_.reset();
    profile_.reset();

    testing::Test::TearDown();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<MockGlicWindowController> mock_controller_;
  std::unique_ptr<MockGlicView> mock_glic_view_;
  std::unique_ptr<GlicWindowHotkeyDelegate> delegate_;
  base::UserActionTester user_action_tester_;
  ScopedTestingLocalState local_state_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<MockBrowserWindow> mock_browser_window_;
  std::unique_ptr<Browser>
      fake_browser_;  // Use real Browser, created minimally
};

}  // namespace

TEST_F(GlicWindowHotkeyDelegateTest, GetSupportedHotkeys) {
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

TEST_F(GlicWindowHotkeyDelegateTest, AcceleratorPressedClose) {
  EXPECT_CALL(*mock_controller_, Close()).Times(1);
  EXPECT_TRUE(
      delegate_->AcceleratorPressed(LocalHotkeyManager::Hotkey::kClose));
}

TEST_F(GlicWindowHotkeyDelegateTest, AcceleratorPressedFocusToggleAttached) {
  EXPECT_CALL(*mock_controller_, IsAttached()).WillOnce(testing::Return(true));
  // Provide the fake browser to the mock controller
  ON_CALL(*mock_controller_, attached_browser())
      .WillByDefault(testing::Return(fake_browser_.get()));

  EXPECT_CALL(*mock_browser_window_, Activate()).Times(1);

  EXPECT_TRUE(
      delegate_->AcceleratorPressed(LocalHotkeyManager::Hotkey::kFocusToggle));
  EXPECT_EQ(0, user_action_tester_.GetActionCount("Glic.FocusHotKey"));
}

TEST_F(GlicWindowHotkeyDelegateTest,
       AcceleratorPressedFocusToggleDetachedWithLastActive) {
  // Set the fake browser as the last active one for this scenario.
  // It's already in the BrowserList thanks to Browser::Create in SetUp.
  BrowserList::SetLastActive(fake_browser_.get());

  EXPECT_CALL(*mock_controller_, IsAttached()).WillOnce(testing::Return(false));
  EXPECT_CALL(*mock_browser_window_, Activate()).Times(1);

  EXPECT_TRUE(
      delegate_->AcceleratorPressed(LocalHotkeyManager::Hotkey::kFocusToggle));
  EXPECT_EQ(1, user_action_tester_.GetActionCount("Glic.FocusHotKey"));
}

TEST_F(GlicWindowHotkeyDelegateTest,
       AcceleratorPressedFocusToggleDetachedNoLastActive) {
  fake_browser_.reset();

  EXPECT_CALL(*mock_controller_, IsAttached()).WillOnce(testing::Return(false));
  // Ensure BrowserList returns nullptr
  ASSERT_EQ(BrowserList::GetInstance()->GetLastActive(), nullptr);

  EXPECT_FALSE(
      delegate_->AcceleratorPressed(LocalHotkeyManager::Hotkey::kFocusToggle));
  EXPECT_EQ(0, user_action_tester_.GetActionCount("Glic.FocusHotKey"));
}

#if BUILDFLAG(IS_WIN)
TEST_F(GlicWindowHotkeyDelegateTest, AcceleratorPressedTitleBarContextMenu) {
  EXPECT_CALL(*mock_controller_, ShowTitleBarContextMenuAt(gfx::Point()))
      .Times(1);
  EXPECT_TRUE(delegate_->AcceleratorPressed(
      LocalHotkeyManager::Hotkey::kTitleBarContextMenu));
}
#endif

TEST_F(GlicWindowHotkeyDelegateTest, CreateScopedHotkeyRegistration) {
  ui::Accelerator test_accel(ui::VKEY_A, ui::EF_NONE);

  auto registration =
      delegate_->CreateScopedHotkeyRegistration(test_accel, nullptr);
  ASSERT_TRUE(registration);
  EXPECT_THAT(mock_glic_view_->GetAccelerators(),
              testing::ElementsAre(test_accel));

  registration.reset();
  EXPECT_EQ(mock_glic_view_->GetAccelerators().size(), 0u);
}

TEST_F(GlicWindowHotkeyDelegateTest, MakeGlicWindowHotkeyManager) {
  auto manager = MakeGlicWindowHotkeyManager(mock_controller_->GetWeakPtr());
  EXPECT_TRUE(manager);
}

}  // namespace glic
