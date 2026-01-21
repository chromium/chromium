// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/test/supervised_user/supervision_mixin.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {
namespace {

class AllIncognitoBrowsersClosedWaiter : public BrowserCollectionObserver {
 public:
  AllIncognitoBrowsersClosedWaiter() {
    observation_.Observe(GlobalBrowserCollection::GetInstance());
  }

  AllIncognitoBrowsersClosedWaiter(const AllIncognitoBrowsersClosedWaiter&) =
      delete;
  AllIncognitoBrowsersClosedWaiter& operator=(
      const AllIncognitoBrowsersClosedWaiter&) = delete;

  ~AllIncognitoBrowsersClosedWaiter() override = default;

  void Wait() { run_loop_.Run(); }

  void OnBrowserClosed(BrowserWindowInterface* browser) override {
    if (chrome::GetIncognitoBrowserCount() == 0u) {
      run_loop_.Quit();
    }
  }

 private:
  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      observation_{this};
  base::RunLoop run_loop_;
};

class SupervisedUserIncognitoBrowserTest
    : public MixinBasedInProcessBrowserTest {
 protected:
  SupervisionMixin supervision_mixin{
      mixin_host_,
      this,
      embedded_test_server(),
      {.sign_in_mode = SupervisionMixin::SignInMode::kSignedOut}};
};

// ChromeOS Ash does not support the browser being signed out on a supervised
// device.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SupervisedUserIncognitoBrowserTest,
                       UnsupervisedSignInDoesNotCloseIncognito) {
  // Create a new incognito windows (this is allowed as the user is not signed
  // in).
  CHECK_EQ(chrome::GetIncognitoBrowserCount(), 0u);
  CreateIncognitoBrowser();
  CHECK_EQ(chrome::GetIncognitoBrowserCount(), 1u);

  // Sign in as a regular user.
  supervision_mixin.SignIn(SupervisionMixin::SignInMode::kRegular);

  // Check the incognito window remains open.
  base::RunLoop().RunUntilIdle();
  CHECK_EQ(chrome::GetIncognitoBrowserCount(), 1u);
}

IN_PROC_BROWSER_TEST_F(SupervisedUserIncognitoBrowserTest,
                       SupervisedSignInClosesIncognito) {
  // Create a new incognito window (this is allowed as the user is not signed
  // in).
  CHECK_EQ(chrome::GetIncognitoBrowserCount(), 0u);
  CreateIncognitoBrowser();
  CHECK_EQ(chrome::GetIncognitoBrowserCount(), 1u);

  AllIncognitoBrowsersClosedWaiter incognito_closed_waiter;

  // Sign in as a supervised user.
  supervision_mixin.SignIn(SupervisionMixin::SignInMode::kSupervised);

  // Check the incognito window remains open.
  incognito_closed_waiter.Wait();
  ASSERT_EQ(chrome::GetIncognitoBrowserCount(), 0u);
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace
}  // namespace supervised_user
