// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_ui_controller.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace skills {

class SkillsUiControllerBrowserTest : public InProcessBrowserTest {
 public:
  SkillsUiController* controller() {
    return SkillsUiController::From(browser());
  }
};

IN_PROC_BROWSER_TEST_F(SkillsUiControllerBrowserTest, OnSkillSavedShowToast) {
  // Ensure no toast is initially showing.
  const auto* toast_controller = browser()->GetFeatures().toast_controller();
  EXPECT_FALSE(toast_controller->IsShowingToast());

  // Call OnSkillSaved with an empty skill ID.
  controller()->OnSkillSaved("");

  // Verify that the toast is now showing.
  EXPECT_TRUE(toast_controller->IsShowingToast());
  EXPECT_EQ(toast_controller->GetCurrentToastId(), ToastId::kSkillSaved);
}

IN_PROC_BROWSER_TEST_F(SkillsUiControllerBrowserTest, OnSkillDeletedShowToast) {
  // Ensure no toast is initially showing.
  const auto* toast_controller = browser()->GetFeatures().toast_controller();
  EXPECT_FALSE(toast_controller->IsShowingToast());

  controller()->OnSkillDeleted();

  // Verify that the toast is now showing.
  EXPECT_TRUE(toast_controller->IsShowingToast());
  EXPECT_EQ(toast_controller->GetCurrentToastId(), ToastId::kSkillDeleted);
}

}  // namespace skills
