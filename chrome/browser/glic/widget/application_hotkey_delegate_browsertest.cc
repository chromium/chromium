// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/application_hotkey_delegate.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/test_support/mock_glic_window_controller.h"
#include "chrome/browser/ui/browser_list.h"
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

class ApplicationHotkeyDelegateTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    delegate_.emplace(mock_controller_.GetWeakPtr());
  }

  BrowserView* GetBrowserView() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  BrowserView* GetBrowserViewForBrowser(Browser* b) {
    return BrowserView::GetBrowserViewForBrowser(b);
  }

  MockGlicWindowController mock_controller_;
  std::optional<ApplicationHotkeyDelegate> delegate_;
  base::UserActionTester user_action_tester_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ApplicationHotkeyDelegateTest, GetSupportedHotkeys) {
  CHECK(g_browser_process);
  auto supported = delegate_->GetSupportedHotkeys();
  // kFocusToggle is the only supported hotkey for application-wide scope.
  EXPECT_THAT(supported,
              testing::ElementsAre(LocalHotkeyManager::Hotkey::kFocusToggle));
}

IN_PROC_BROWSER_TEST_F(ApplicationHotkeyDelegateTest,
                       AcceleratorPressedFocusToggle) {
  EXPECT_CALL(mock_controller_, FocusIfOpen()).Times(1);

  EXPECT_TRUE(
      delegate_->AcceleratorPressed(LocalHotkeyManager::Hotkey::kFocusToggle));
  EXPECT_EQ(1, user_action_tester_.GetActionCount("Glic.FocusHotKey"));
}

IN_PROC_BROWSER_TEST_F(ApplicationHotkeyDelegateTest,
                       AcceleratorPressedControllerNull) {
  // Create a delegate with an invalidated controller WeakPtr.
  auto controller = std::make_unique<MockGlicWindowController>();
  auto delegate_with_null_controller =
      std::make_unique<ApplicationHotkeyDelegate>(controller->GetWeakPtr());
  controller.reset();  // Invalidate the WeakPtr

  EXPECT_FALSE(delegate_with_null_controller->AcceleratorPressed(
      LocalHotkeyManager::Hotkey::kFocusToggle));
  EXPECT_EQ(0, user_action_tester_.GetActionCount("Glic.FocusHotKey"));
}

IN_PROC_BROWSER_TEST_F(ApplicationHotkeyDelegateTest,
                       CreateScopedHotkeyRegistration) {
  ui::Accelerator test_accel(ui::VKEY_A, ui::EF_NONE);
  MockAcceleratorTarget mock_target;

  ASSERT_TRUE(GetBrowserView()) << "BrowserView not available";
  views::FocusManager* focus_manager = GetBrowserView()->GetFocusManager();
  ASSERT_TRUE(focus_manager) << "FocusManager not available";

  EXPECT_FALSE(focus_manager->IsAcceleratorRegistered(test_accel));

  auto registration = delegate_->CreateScopedHotkeyRegistration(
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

  // BrowserList will take care of cleaning up browser2.
}

IN_PROC_BROWSER_TEST_F(ApplicationHotkeyDelegateTest,
                       MakeApplicationHotkeyManager) {
  auto manager = MakeApplicationHotkeyManager(mock_controller_.GetWeakPtr());
  EXPECT_TRUE(manager);
}

}  // namespace glic
