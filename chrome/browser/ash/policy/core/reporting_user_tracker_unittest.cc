// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/reporting_user_tracker.h"

#include <memory>

#include "base/check_deref.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/account_id/account_id.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager_delegate.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class ReportingUserTrackerTest : public ::testing::Test {
 protected:
  void SetUp() override { SetUpUserManager(); }

  void TearDown() override { TearDownUserManager(); }

  user_manager::UserManager& user_manager() {
    return CHECK_DEREF(user_manager_.get());
  }
  ReportingUserTracker& tracker() { return *reporting_user_tracker_; }

  void RecreateUserManager() {
    TearDownUserManager();
    SetUpUserManager();
  }

 private:
  void SetUpUserManager() {
    user_manager_ = std::make_unique<user_manager::UserManagerImpl>(
        std::make_unique<user_manager::FakeUserManagerDelegate>(),
        TestingBrowserProcess::GetGlobal()->GetTestingLocalState());
    reporting_user_tracker_ = std::make_unique<ReportingUserTracker>(
        user_manager_.get());
  }
  void TearDownUserManager() {
    reporting_user_tracker_.reset();
    user_manager_.reset();
  }

  std::unique_ptr<user_manager::UserManager> user_manager_;
  std::unique_ptr<ReportingUserTracker> reporting_user_tracker_;
};

TEST_F(ReportingUserTrackerTest, RegularUserAffiliation) {
  constexpr char kUserEmail[] = "test@test";
  const auto account_id =
      AccountId::FromUserEmailGaiaId(kUserEmail, GaiaId("123456789"));
  ASSERT_TRUE(
      user_manager::TestHelper(&user_manager()).AddRegularUser(account_id));

  // Only users marked as affiliated are the target for reporting.
  EXPECT_FALSE(tracker().ShouldReportUser(kUserEmail));
  user_manager().SetUserPolicyStatus(account_id, /*is_managed=*/true,
                                     /*is_affiliated=*/true);
  EXPECT_TRUE(tracker().ShouldReportUser(kUserEmail));
  user_manager().SetUserPolicyStatus(account_id, /*is_managed=*/true,
                                     /*is_affiliated=*/false);
  EXPECT_FALSE(tracker().ShouldReportUser(kUserEmail));
}

TEST_F(ReportingUserTrackerTest, NonRegularUserAffiliation) {
  constexpr char kUserEmail[] = "test@test";
  const auto account_id =
      AccountId::FromUserEmailGaiaId(kUserEmail, GaiaId("123456789"));
  ASSERT_TRUE(
      user_manager::TestHelper(&user_manager()).AddChildUser(account_id));
  EXPECT_FALSE(tracker().ShouldReportUser(kUserEmail));
  user_manager().SetUserPolicyStatus(account_id, /*is_managed=*/true,
                                     /*is_affiliated=*/true);
  // No impact on setting affiliation.
  EXPECT_FALSE(tracker().ShouldReportUser(kUserEmail));
  user_manager().SetUserPolicyStatus(account_id, /*is_managed=*/true,
                                     /*is_affiliated=*/false);
  EXPECT_FALSE(tracker().ShouldReportUser(kUserEmail));
}

TEST_F(ReportingUserTrackerTest, Persistency) {
  constexpr char kUserEmail[] = "test@test";
  const auto account_id =
      AccountId::FromUserEmailGaiaId(kUserEmail, GaiaId("123456789"));
  ASSERT_TRUE(
      user_manager::TestHelper(&user_manager()).AddRegularUser(account_id));
  user_manager().SetUserPolicyStatus(account_id, /*is_managed=*/true,
                                     /*is_affiliated=*/true);
  EXPECT_TRUE(tracker().ShouldReportUser(kUserEmail));

  // Whether or not to report is persistent.
  RecreateUserManager();

  EXPECT_TRUE(tracker().ShouldReportUser(kUserEmail));
}

TEST_F(ReportingUserTrackerTest, UserRemoval) {
  // Add owner user to allow removing the following user.
  const auto owner_account_id =
      AccountId::FromUserEmailGaiaId("owner@test", GaiaId("987654321"));
  ASSERT_TRUE(user_manager::TestHelper(&user_manager())
                  .AddRegularUser(owner_account_id));
  user_manager().SetOwnerId(owner_account_id);

  constexpr char kUserEmail[] = "test@test";
  // When user is removed, ShouldReportUser should be updated, too.
  const auto account_id =
      AccountId::FromUserEmailGaiaId(kUserEmail, GaiaId("123456789"));
  ASSERT_TRUE(
      user_manager::TestHelper(&user_manager()).AddRegularUser(account_id));
  user_manager().SetUserPolicyStatus(account_id, /*is_managed=*/true,
                                     /*is_affiliated=*/true);
  EXPECT_TRUE(tracker().ShouldReportUser(kUserEmail));

  user_manager().RemoveUser(account_id,
                            user_manager::UserRemovalReason::UNKNOWN);

  EXPECT_FALSE(tracker().ShouldReportUser(kUserEmail));
}

}  // namespace policy
