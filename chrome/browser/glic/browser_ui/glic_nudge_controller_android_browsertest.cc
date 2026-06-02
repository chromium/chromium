// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_nudge_controller_android.h"

#include "base/functional/callback_helpers.h"
#include "base/test/test_future.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_delegate.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

namespace {

class MockGlicNudgeDelegate : public GlicNudgeDelegate {
 public:
  MockGlicNudgeDelegate() = default;
  ~MockGlicNudgeDelegate() override = default;

  void OnTriggerGlicNudgeUI(NudgeParams params) override {
    is_showing_glic_nudge_ = true;
    last_nudge_params_ = std::move(params);
  }

  void OnHideGlicNudgeUI() override { is_showing_glic_nudge_ = false; }

  bool GetIsShowingGlicNudge() override { return is_showing_glic_nudge_; }

  const std::optional<NudgeParams>& last_nudge_params() const {
    return last_nudge_params_;
  }

 private:
  bool is_showing_glic_nudge_ = false;
  std::optional<NudgeParams> last_nudge_params_;
};

}  // namespace

class GlicNudgeControllerAndroidBrowserTest : public GlicBrowserTest {
 public:
  GlicNudgeControllerAndroidBrowserTest() = default;
  ~GlicNudgeControllerAndroidBrowserTest() override = default;

  void SetUpOnMainThread() override {
    GlicBrowserTest::SetUpOnMainThread();
    nudge_controller_ =
        std::make_unique<GlicNudgeControllerAndroid>(GetTabListInterface());
    nudge_controller_->SetTabStripDelegate(&mock_delegate_);
  }

  void TearDownOnMainThread() override {
    nudge_controller_.reset();
    GlicBrowserTest::TearDownOnMainThread();
  }

 protected:
  MockGlicNudgeDelegate mock_delegate_;
  std::unique_ptr<GlicNudgeControllerAndroid> nudge_controller_;
};

IN_PROC_BROWSER_TEST_F(GlicNudgeControllerAndroidBrowserTest, ShowsNudge) {
  content::WebContents* web_contents =
      GetTabListInterface()->GetActiveTab()->GetContents();

  EXPECT_FALSE(mock_delegate_.GetIsShowingGlicNudge());

  base::test::TestFuture<GlicNudgeActivity> future;
  nudge_controller_->UpdateNudgeLabel(
      web_contents, "Nudge Label", "Prompt Suggestion", "Anchored Message Text",
      std::nullopt, future.GetRepeatingCallback());

  EXPECT_TRUE(mock_delegate_.GetIsShowingGlicNudge());
  EXPECT_EQ(future.Get(), GlicNudgeActivity::kNudgeShown);
  ASSERT_TRUE(mock_delegate_.last_nudge_params().has_value());
  EXPECT_EQ(mock_delegate_.last_nudge_params()->label, "Nudge Label");
}

IN_PROC_BROWSER_TEST_F(GlicNudgeControllerAndroidBrowserTest, HidesNudge) {
  content::WebContents* web_contents =
      GetTabListInterface()->GetActiveTab()->GetContents();

  EXPECT_FALSE(mock_delegate_.GetIsShowingGlicNudge());

  base::test::TestFuture<GlicNudgeActivity> show_future;
  nudge_controller_->UpdateNudgeLabel(
      web_contents, "Nudge Label", "Prompt Suggestion", "Anchored Message Text",
      std::nullopt, show_future.GetRepeatingCallback());

  EXPECT_TRUE(mock_delegate_.GetIsShowingGlicNudge());
  EXPECT_EQ(show_future.Get(), GlicNudgeActivity::kNudgeShown);

  base::test::TestFuture<GlicNudgeActivity> hide_future;
  nudge_controller_->UpdateNudgeLabel(
      web_contents, std::string(), std::nullopt, std::string(),
      GlicNudgeActivity::kNudgeDismissed, hide_future.GetRepeatingCallback());

  EXPECT_FALSE(mock_delegate_.GetIsShowingGlicNudge());
  EXPECT_EQ(hide_future.Get(), GlicNudgeActivity::kNudgeDismissed);
}

IN_PROC_BROWSER_TEST_F(GlicNudgeControllerAndroidBrowserTest,
                       HidesNudgeOnActiveTabChanged) {
  content::WebContents* web_contents =
      GetTabListInterface()->GetActiveTab()->GetContents();

  EXPECT_FALSE(mock_delegate_.GetIsShowingGlicNudge());

  base::test::TestFuture<GlicNudgeActivity> future;
  nudge_controller_->UpdateNudgeLabel(
      web_contents, "Nudge Label", "Prompt Suggestion", "Anchored Message Text",
      std::nullopt, future.GetRepeatingCallback());

  EXPECT_TRUE(mock_delegate_.GetIsShowingGlicNudge());
  EXPECT_EQ(future.Take(), GlicNudgeActivity::kNudgeShown);

  // Open and activate a new tab.
  CreateAndActivateTab(GetSimpleTestUrl());

  EXPECT_FALSE(mock_delegate_.GetIsShowingGlicNudge());
  EXPECT_EQ(future.Take(), GlicNudgeActivity::kNudgeIgnoredActiveTabChanged);
}

IN_PROC_BROWSER_TEST_F(GlicNudgeControllerAndroidBrowserTest,
                       DoesNotShowNudgeForInactiveTab) {
  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  tabs::TabInterface* inactive_tab = CreateAndActivateTab(GetSimpleTestUrl());
  GetTabListInterface()->ActivateTab(active_tab->GetHandle());

  EXPECT_FALSE(mock_delegate_.GetIsShowingGlicNudge());

  base::test::TestFuture<GlicNudgeActivity> future;
  nudge_controller_->UpdateNudgeLabel(
      inactive_tab->GetContents(), "Nudge Label", "Prompt Suggestion",
      "Anchored Message Text", std::nullopt, future.GetRepeatingCallback());

  EXPECT_FALSE(mock_delegate_.GetIsShowingGlicNudge());
  EXPECT_EQ(future.Get(), GlicNudgeActivity::kNudgeNotShownWebContents);
}

IN_PROC_BROWSER_TEST_F(GlicNudgeControllerAndroidBrowserTest,
                       GetAndClearPromptSuggestion) {
  content::WebContents* web_contents =
      GetTabListInterface()->GetActiveTab()->GetContents();

  base::test::TestFuture<GlicNudgeActivity> future;
  nudge_controller_->UpdateNudgeLabel(
      web_contents, "Nudge Label", "Prompt Suggestion", "Anchored Message Text",
      std::nullopt, future.GetRepeatingCallback());

  EXPECT_EQ(nudge_controller_->GetPromptSuggestion(), "Prompt Suggestion");
  nudge_controller_->ClearPromptSuggestion();
  EXPECT_FALSE(nudge_controller_->GetPromptSuggestion().has_value());
}

}  // namespace glic
