// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/os_feedback/chrome_os_feedback_delegate.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr char kPageUrl[] = "https://www.google.com/?q=123";
constexpr char kSignedInUserEmail[] = "test_user_email@gmail.com";

}  // namespace

class ChromeOsFeedbackDelegateTest : public InProcessBrowserTest {
 public:
  ChromeOsFeedbackDelegateTest() = default;
  ~ChromeOsFeedbackDelegateTest() override = default;

  absl::optional<GURL> GetLastActivePageUrl() {
    ChromeOsFeedbackDelegate feedback_delegate_(browser()->profile());
    return feedback_delegate_.GetLastActivePageUrl();
  }
};

// Test GetApplicationLocale returns a valid locale.
IN_PROC_BROWSER_TEST_F(ChromeOsFeedbackDelegateTest, GetApplicationLocale) {
  ChromeOsFeedbackDelegate feedback_delegate_(browser()->profile());
  EXPECT_EQ(feedback_delegate_.GetApplicationLocale(), "en-US");
}

// Test GetLastActivePageUrl returns last active page url if any.
IN_PROC_BROWSER_TEST_F(ChromeOsFeedbackDelegateTest, GetLastActivePageUrl) {
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 1u);
  EXPECT_EQ(GetLastActivePageUrl()->spec(), "about:blank");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kPageUrl)));
  EXPECT_EQ(GetLastActivePageUrl()->spec(), kPageUrl);
}

// Test GetSignedInUserEmail returns primary account of signed in user if any..
IN_PROC_BROWSER_TEST_F(ChromeOsFeedbackDelegateTest, GetSignedInUserEmail) {
  auto* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(identity_manager);

  ChromeOsFeedbackDelegate feedback_delegate_(browser()->profile());
  EXPECT_EQ(feedback_delegate_.GetSignedInUserEmail(), "");

  signin::MakePrimaryAccountAvailable(identity_manager, kSignedInUserEmail,
                                      signin::ConsentLevel::kSignin);
  EXPECT_EQ(feedback_delegate_.GetSignedInUserEmail(), kSignedInUserEmail);
}

}  // namespace ash
