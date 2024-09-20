// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/test/supervised_user/supervision_mixin.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class AllIncognitoBrowsersClosedWaiter : public BrowserListObserver {
 public:
  AllIncognitoBrowsersClosedWaiter() { BrowserList::AddObserver(this); }

  AllIncognitoBrowsersClosedWaiter(const AllIncognitoBrowsersClosedWaiter&) =
      delete;
  AllIncognitoBrowsersClosedWaiter& operator=(
      const AllIncognitoBrowsersClosedWaiter&) = delete;

  ~AllIncognitoBrowsersClosedWaiter() override {
    BrowserList::RemoveObserver(this);
  }

  void Wait() { run_loop_.Run(); }

  // BrowserListObserver implementation.
  void OnBrowserRemoved(Browser* browser) override {
    if (BrowserList::GetInstance()->GetIncognitoBrowserCount() == 0u) {
      run_loop_.Quit();
    }
  }

 private:
  base::RunLoop run_loop_;
};

class SupervisedUserIncognitoBrowserTest
    : public MixinBasedInProcessBrowserTest,
      public ::testing::WithParamInterface<
          supervised_user::SupervisionMixin::SignInMode> {
 protected:
  supervised_user::SupervisionMixin supervision_mixin{
      mixin_host_,
      this,
      embedded_test_server(),
      {.sign_in_mode =
           supervised_user::SupervisionMixin::SignInMode::kSignedOut}};
};

// ChromeOS Ash does not support the browser being signed out on a supervised
// device.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SupervisedUserIncognitoBrowserTest,
                       UnsupervisedSignInDoesNotCloseIncognito) {
  BrowserList* browser_list = BrowserList::GetInstance();

  // Create a new incognito windows (this is allowed as the user is not signed
  // in).
  CHECK_EQ(browser_list->GetIncognitoBrowserCount(), 0u);
  CreateIncognitoBrowser();
  CHECK_EQ(browser_list->GetIncognitoBrowserCount(), 1u);

  // Sign in as a regular user.
  supervision_mixin.SignIn(
      supervised_user::SupervisionMixin::SignInMode::kRegular);

  // Check the incognito window remains open.
  base::RunLoop().RunUntilIdle();
  CHECK_EQ(browser_list->GetIncognitoBrowserCount(), 1u);
}

IN_PROC_BROWSER_TEST_F(SupervisedUserIncognitoBrowserTest,
                       SupervisedSignInClosesIncognito) {
  BrowserList* browser_list = BrowserList::GetInstance();

  // Create a new incognito window (this is allowed as the user is not signed
  // in).
  CHECK_EQ(browser_list->GetIncognitoBrowserCount(), 0u);
  CreateIncognitoBrowser();
  CHECK_EQ(browser_list->GetIncognitoBrowserCount(), 1u);

  AllIncognitoBrowsersClosedWaiter incognito_closed_waiter;

  // Sign in as a supervised user.
  supervision_mixin.SignIn(
      supervised_user::SupervisionMixin::SignInMode::kSupervised);

  // Check the incognito window remains open.
  incognito_closed_waiter.Wait();
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace
