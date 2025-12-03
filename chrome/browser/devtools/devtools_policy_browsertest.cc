// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "chrome/browser/devtools/devtools_policy_dialog.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

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

class DevToolsPolicyDialogTest : public InProcessBrowserTest {
 public:
  DevToolsPolicyDialogTest() = default;
  ~DevToolsPolicyDialogTest() override = default;
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    DevToolsPolicyDialog::SetTestObserver(&observer_);
  }
  void TearDownOnMainThread() override {
    DevToolsPolicyDialog::SetTestObserver(nullptr);
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  TestObserverImpl observer_;
};
}  // namespace

IN_PROC_BROWSER_TEST_F(DevToolsPolicyDialogTest, ShowDialogOnce) {
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  DevToolsPolicyDialog::Show(web_contents);
  EXPECT_EQ(1, observer_.shown_count());
  // Trying to show it again on the same WebContents should not create a new
  // dialog.
  DevToolsPolicyDialog::Show(web_contents);
  EXPECT_EQ(1, observer_.shown_count());
}

IN_PROC_BROWSER_TEST_F(DevToolsPolicyDialogTest, ShowDialogOnTwoWebContents) {
  auto* web_contents1 = browser()->tab_strip_model()->GetActiveWebContents();
  DevToolsPolicyDialog::Show(web_contents1);
  EXPECT_EQ(1, observer_.shown_count());
  // Open a new tab.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));
  auto* web_contents2 = browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_NE(web_contents1, web_contents2);
  DevToolsPolicyDialog::Show(web_contents2);
  EXPECT_EQ(2, observer_.shown_count());
}

#if !BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(DevToolsPolicyDialogTest, ShowDialogTwice) {
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  DevToolsPolicyDialog::Show(web_contents);
  EXPECT_EQ(1, observer_.shown_count());
  DevToolsPolicyDialog::TestOnlyCloseDialog(web_contents);
  DevToolsPolicyDialog::Show(web_contents);
  EXPECT_EQ(2, observer_.shown_count());
}
#endif

IN_PROC_BROWSER_TEST_F(DevToolsPolicyDialogTest,
                       DialogCleanedUpOnWebContentsDestruction) {
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  DevToolsPolicyDialog::Show(web_contents);
  EXPECT_EQ(1u, DevToolsPolicyDialog::GetCurrentDialogsSizeForTesting());

  base::RunLoop run_loop;
  observer_.SetQuitClosure(run_loop.QuitClosure());
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);
  run_loop.Run();

  EXPECT_EQ(0u, DevToolsPolicyDialog::GetCurrentDialogsSizeForTesting());
}
