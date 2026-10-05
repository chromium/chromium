// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <string>

#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/devtools_policy_dialog.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/policy/core/browser/developer_tools_availability.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

constexpr char kBlockedByPolicyHistogram[] = "DevTools.BlockedByPolicy";

class TestObserverImpl : public DevToolsPolicyDialog::TestObserver {
 public:
  ~TestObserverImpl() override = default;

  void OnDialogShown(DevToolsPolicyDialog* dialog) override { shown_count_++; }
  void OnDialogDestroyed(DevToolsPolicyDialog* dialog) override {
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  int shown_count() const { return shown_count_; }

  void SetQuitClosure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

 private:
  int shown_count_ = 0;
  base::OnceClosure quit_closure_;
};

class DevToolsPolicyDialogTest : public PlatformBrowserTest {
 public:
  DevToolsPolicyDialogTest() = default;
  ~DevToolsPolicyDialogTest() override = default;
  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    DevToolsPolicyDialog::SetTestObserver(&observer_);
  }
  void TearDownOnMainThread() override {
    DevToolsPolicyDialog::SetTestObserver(nullptr);
    PlatformBrowserTest::TearDownOnMainThread();
  }

 protected:
  // Sets the value the DeveloperToolsAvailability policy would set for the
  // test profile.
  void SetDevToolsAvailability(
      policy::DeveloperToolsAvailability availability) {
    chrome_test_utils::GetProfile(this)->GetPrefs()->SetInteger(
        prefs::kDevToolsAvailability, static_cast<int>(availability));
  }

  TestObserverImpl observer_;
};

// Runs once per DeveloperToolsAvailability policy value.
class DevToolsPolicyDialogHistogramTest
    : public DevToolsPolicyDialogTest,
      public testing::WithParamInterface<policy::DeveloperToolsAvailability> {};

std::string DeveloperToolsAvailabilityToString(
    const testing::TestParamInfo<policy::DeveloperToolsAvailability>& info) {
  switch (info.param) {
    case policy::DeveloperToolsAvailability::
        kDisallowedForForceInstalledExtensions:
      return "DisallowedForForceInstalledExtensions";
    case policy::DeveloperToolsAvailability::kAllowed:
      return "Allowed";
    case policy::DeveloperToolsAvailability::kDisallowed:
      return "Disallowed";
  }
  NOTREACHED();
}

}  // namespace

IN_PROC_BROWSER_TEST_F(DevToolsPolicyDialogTest, ShowDialogOnce) {
  base::HistogramTester histogram_tester;
  auto* web_contents = chrome_test_utils::GetActiveWebContents(this);
  DevToolsPolicyDialog::Show(web_contents);
  EXPECT_EQ(1, observer_.shown_count());
  // Trying to show it again on the same WebContents should not create a new
  // dialog.
  DevToolsPolicyDialog::Show(web_contents);
  EXPECT_EQ(1, observer_.shown_count());
  // Only the dialog that was actually shown is counted, bucketed by the
  // effective DeveloperToolsAvailability policy value (default when unset).
  histogram_tester.ExpectUniqueSample(
      kBlockedByPolicyHistogram,
      policy::DeveloperToolsAvailability::
          kDisallowedForForceInstalledExtensions,
      1);
}

IN_PROC_BROWSER_TEST_F(DevToolsPolicyDialogTest, ShowDialogOnTwoWebContents) {
  base::HistogramTester histogram_tester;
  auto* web_contents1 = chrome_test_utils::GetActiveWebContents(this);
  DevToolsPolicyDialog::Show(web_contents1);
  EXPECT_EQ(1, observer_.shown_count());

  // Open a new tab.
  auto* tab_list = TabListInterface::From(GetBrowserWindowInterface());
  ASSERT_TRUE(tab_list);
  auto* tab2 = tab_list->OpenTab(GURL("about:blank"), 1);
  ASSERT_TRUE(tab2);
  auto* web_contents2 = tab2->GetContents();
  ASSERT_NE(web_contents1, web_contents2);
  DevToolsPolicyDialog::Show(web_contents2);
  EXPECT_EQ(2, observer_.shown_count());
  // A dialog on a different tab is a separate block.
  histogram_tester.ExpectTotalCount(kBlockedByPolicyHistogram, 2);
}

#if !BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(DevToolsPolicyDialogTest, ShowDialogTwice) {
  base::HistogramTester histogram_tester;
  auto* web_contents = chrome_test_utils::GetActiveWebContents(this);
  DevToolsPolicyDialog::Show(web_contents);
  EXPECT_EQ(1, observer_.shown_count());
  DevToolsPolicyDialog::TestOnlyCloseDialog(web_contents);
  DevToolsPolicyDialog::Show(web_contents);
  EXPECT_EQ(2, observer_.shown_count());
  // Showing the dialog again after it was closed is a new block.
  histogram_tester.ExpectTotalCount(kBlockedByPolicyHistogram, 2);
}
#endif

IN_PROC_BROWSER_TEST_F(DevToolsPolicyDialogTest,
                       DialogCleanedUpOnWebContentsDestruction) {
  auto* web_contents = chrome_test_utils::GetActiveWebContents(this);
  DevToolsPolicyDialog::Show(web_contents);
  EXPECT_EQ(1u, DevToolsPolicyDialog::GetCurrentDialogsSizeForTesting());

  base::RunLoop run_loop;
  observer_.SetQuitClosure(run_loop.QuitClosure());

  auto* tab_list = TabListInterface::From(GetBrowserWindowInterface());
  ASSERT_TRUE(tab_list);
  auto* tab = tab_list->GetTab(0);
  ASSERT_TRUE(tab);
  tab_list->CloseTab(tab->GetHandle());

  run_loop.Run();

  EXPECT_EQ(0u, DevToolsPolicyDialog::GetCurrentDialogsSizeForTesting());
}

// The recorded bucket must be the effective DeveloperToolsAvailability value,
// so that blocks under the default or Allowed values (contexts admins did not
// explicitly disallow) can be told apart from admin-intended blocks under
// Disallowed.
IN_PROC_BROWSER_TEST_P(DevToolsPolicyDialogHistogramTest,
                       RecordsEffectivePolicyValue) {
  base::HistogramTester histogram_tester;
  SetDevToolsAvailability(GetParam());

  DevToolsPolicyDialog::Show(chrome_test_utils::GetActiveWebContents(this));

  EXPECT_EQ(1, observer_.shown_count());
  histogram_tester.ExpectUniqueSample(kBlockedByPolicyHistogram, GetParam(), 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DevToolsPolicyDialogHistogramTest,
    testing::Values(policy::DeveloperToolsAvailability::
                        kDisallowedForForceInstalledExtensions,
                    policy::DeveloperToolsAvailability::kAllowed,
                    policy::DeveloperToolsAvailability::kDisallowed),
    &DeveloperToolsAvailabilityToString);

// End-to-end: an actual attempt to open DevTools that the policy blocks must
// go through the dialog and be counted.
IN_PROC_BROWSER_TEST_F(DevToolsPolicyDialogTest,
                       RecordedWhenOpeningDevToolsIsBlocked) {
  base::HistogramTester histogram_tester;
  auto* web_contents = chrome_test_utils::GetActiveWebContents(this);
  SetDevToolsAvailability(policy::DeveloperToolsAvailability::kDisallowed);

  DevToolsWindow::OpenDevToolsWindow(web_contents,
                                     DevToolsOpenedByAction::kUnknown);

  EXPECT_FALSE(
      DevToolsWindow::GetInstanceForInspectedWebContents(web_contents));
  EXPECT_EQ(1, observer_.shown_count());
  histogram_tester.ExpectUniqueSample(
      kBlockedByPolicyHistogram,
      policy::DeveloperToolsAvailability::kDisallowed, 1);
}

// Negative control: with the default policy value DevTools are allowed on
// regular pages, so opening them must neither show the dialog nor be counted.
IN_PROC_BROWSER_TEST_F(DevToolsPolicyDialogTest,
                       NotRecordedWhenOpeningDevToolsIsAllowed) {
  base::HistogramTester histogram_tester;
  auto* web_contents = chrome_test_utils::GetActiveWebContents(this);

  DevToolsWindow* window = DevToolsWindowTesting::OpenDevToolsWindowSync(
      web_contents, /*is_docked=*/true);
  ASSERT_TRUE(window);

  EXPECT_EQ(0, observer_.shown_count());
  histogram_tester.ExpectTotalCount(kBlockedByPolicyHistogram, 0);

  DevToolsWindowTesting::CloseDevToolsWindowSync(window);
}
