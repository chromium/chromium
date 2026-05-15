// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/common/panel_visibility_dependent_hotkey_manager.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/common/application_hotkey_delegate.h"
#include "chrome/browser/glic/test_support/mock_local_hotkey_panel.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/views/focus/focus_manager.h"

namespace glic {

namespace {

class MockAcceleratorTarget : public ui::AcceleratorTarget {
 public:
  MOCK_METHOD(bool,
              AcceleratorPressed,
              (const ui::Accelerator& accelerator),
              (override));
  MOCK_METHOD(bool, CanHandleAccelerators, (), (const, override));

  base::WeakPtr<MockAcceleratorTarget> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockAcceleratorTarget> weak_factory_{this};
};

class PanelVisibilityDependentHotkeyManagerTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    manager_.emplace(mock_panel_.GetWeakPtr());
    registration_delegate_.emplace();
  }

  void TearDownOnMainThread() override {
    manager_.reset();
    registration_delegate_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  BrowserView* GetBrowserView() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  BrowserView* GetBrowserViewForBrowser(Browser* b) {
    return BrowserView::GetBrowserViewForBrowser(b);
  }

  MockLocalHotkeyPanel mock_panel_;
  std::optional<PanelVisibilityDependentHotkeyManager> manager_;
  std::optional<ApplicationScopedRegistrationDelegate> registration_delegate_;
  base::UserActionTester user_action_tester_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(PanelVisibilityDependentHotkeyManagerTest,
                       AcceleratorPressedFocusToggle) {
  EXPECT_CALL(mock_panel_, FocusIfOpen()).Times(1);
  EXPECT_CALL(mock_panel_, IsShowing()).WillOnce(testing::Return(true));

  EXPECT_TRUE(
      manager_->AcceleratorPressed(LocalHotkeyManager::Command::kFocusToggle));
  EXPECT_EQ(1, user_action_tester_.GetActionCount("Glic.FocusHotKey"));
}

IN_PROC_BROWSER_TEST_F(PanelVisibilityDependentHotkeyManagerTest,
                       AcceleratorPressedControllerNull) {
  // Create a manager with an invalidated controller WeakPtr.
  auto panel = std::make_unique<MockLocalHotkeyPanel>();
  auto manager_with_null_controller =
      std::make_unique<PanelVisibilityDependentHotkeyManager>(
          panel->GetWeakPtr());
  panel.reset();  // Invalidate the WeakPtr

  EXPECT_FALSE(manager_with_null_controller->AcceleratorPressed(
      LocalHotkeyManager::Command::kFocusToggle));
  EXPECT_EQ(0, user_action_tester_.GetActionCount("Glic.FocusHotKey"));
}

IN_PROC_BROWSER_TEST_F(PanelVisibilityDependentHotkeyManagerTest,
                       CreateScopedHotkeyRegistration) {
  ui::Accelerator test_accel(ui::VKEY_A, ui::EF_NONE);
  MockAcceleratorTarget mock_target;

  ASSERT_TRUE(GetBrowserView()) << "BrowserView not available";
  views::FocusManager* focus_manager = GetBrowserView()->GetFocusManager();
  ASSERT_TRUE(focus_manager) << "FocusManager not available";

  EXPECT_FALSE(focus_manager->IsAcceleratorRegistered(test_accel));

  auto registration = registration_delegate_->CreateScopedHotkeyRegistration(
      test_accel, mock_target.GetWeakPtr());
  ASSERT_TRUE(registration);

  // Verify registration with the first browser.
  EXPECT_TRUE(focus_manager->IsAcceleratorRegistered(test_accel));

  // Test adding a new browser after registration.
  Browser* browser2 = CreateBrowser(browser()->profile());
  BrowserView* browser_view2 = GetBrowserViewForBrowser(browser2);
  ASSERT_TRUE(browser_view2);
  views::FocusManager* focus_manager2 = nullptr;

  focus_manager2 = browser_view2->GetFocusManager();
  ASSERT_TRUE(focus_manager2);

  EXPECT_TRUE(focus_manager2->IsAcceleratorRegistered(test_accel));

  registration.reset();

  EXPECT_FALSE(focus_manager->IsAcceleratorRegistered(test_accel));
  EXPECT_FALSE(focus_manager2->IsAcceleratorRegistered(test_accel));
}

}  // namespace glic
