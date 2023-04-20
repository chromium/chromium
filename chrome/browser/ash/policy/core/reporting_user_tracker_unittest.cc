// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/reporting_user_tracker.h"

#include <memory>

#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/account_id/account_id.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class ReportingUserTrackerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    user_manager_ = std::make_unique<ash::FakeChromeUserManager>();
  }

  void TearDown() override { user_manager_.reset(); }

  ReportingUserTracker& tracker() {
    return user_manager_->reporting_user_tracker_;
  }
  ash::FakeChromeUserManager& user_manager() { return *user_manager_; }

  void RecreatUserManager() {
    user_manager_.reset();
    user_manager_ = std::make_unique<ash::FakeChromeUserManager>();
  }

 private:
  ScopedTestingLocalState scoped_local_state_{
      TestingBrowserProcess::GetGlobal()};
  std::unique_ptr<ash::FakeChromeUserManager> user_manager_;
};

TEST_F(ReportingUserTrackerTest, RegularUserAffiliation) {
  constexpr char kUserEmail[] = "test@test";
  const auto account_id = AccountId::FromUserEmail(kUserEmail);
  user_manager().AddUser(account_id);

  // Only users marked as affiliated are the target for reporting.
  EXPECT_FALSE(tracker().ShouldReportUser(kUserEmail));
  user_manager().SetUserAffiliationForTesting(account_id, true);
  EXPECT_TRUE(tracker().ShouldReportUser(kUserEmail));
  user_manager().SetUserAffiliationForTesting(account_id, false);
  EXPECT_FALSE(tracker().ShouldReportUser(kUserEmail));
}

TEST_F(ReportingUserTrackerTest, NonRegularUserAffiliation) {
  constexpr char kUserEmail[] = "test@test";
  const auto account_id = AccountId::FromUserEmail(kUserEmail);
  user_manager().AddChildUser(account_id);
  EXPECT_FALSE(tracker().ShouldReportUser(kUserEmail));
  user_manager().SetUserAffiliationForTesting(account_id, true);
  // No impact on setting affiliation.
  EXPECT_FALSE(tracker().ShouldReportUser(kUserEmail));
  user_manager().SetUserAffiliationForTesting(account_id, false);
  EXPECT_FALSE(tracker().ShouldReportUser(kUserEmail));
}

TEST_F(ReportingUserTrackerTest, Persistency) {
  constexpr char kUserEmail[] = "test@test";
  const auto account_id = AccountId::FromUserEmail(kUserEmail);
  user_manager().AddUser(account_id);
  user_manager().SetUserAffiliationForTesting(account_id, true);
  EXPECT_TRUE(tracker().ShouldReportUser(kUserEmail));

  // Whether or not to report is persistent.
  RecreatUserManager();

  EXPECT_TRUE(tracker().ShouldReportUser(kUserEmail));
}

TEST_F(ReportingUserTrackerTest, UserRemoval) {
  constexpr char kUserEmail[] = "test@test";
  // When user is removed, ShouldReportUser should be updated, too.
  const auto account_id = AccountId::FromUserEmail(kUserEmail);
  user_manager().AddUser(account_id);
  user_manager().SetUserAffiliationForTesting(account_id, true);
  EXPECT_TRUE(tracker().ShouldReportUser(kUserEmail));

  user_manager().RemoveUser(account_id,
                            user_manager::UserRemovalReason::UNKNOWN);

  EXPECT_FALSE(tracker().ShouldReportUser(kUserEmail));
}

}  // namespace policy
