// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/account_manager/profile_account_manager.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/mock_account_manager_facade.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class ProfileAccountManagerTest : public testing::Test {
 public:
  ProfileAccountManagerTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {
    CHECK(testing_profile_manager_.SetUp());
    testing_profile_manager_.SetAccountProfileMapper(
        std::make_unique<AccountProfileMapper>(
            &mock_facade_, storage(),
            testing_profile_manager_.local_state()->Get()));
  }

  AccountProfileMapper* mapper() {
    return testing_profile_manager_.profile_manager()
        ->GetAccountProfileMapper();
  }

 private:
  ProfileAttributesStorage* storage() {
    return &testing_profile_manager_.profile_manager()
                ->GetProfileAttributesStorage();
  }

  content::BrowserTaskEnvironment task_environment_;
  account_manager::MockAccountManagerFacade mock_facade_;
  TestingProfileManager testing_profile_manager_;
};

TEST_F(ProfileAccountManagerTest, Observer) {
  const base::FilePath kProfilePath("/Profile/Path");
  const account_manager::Account kAccount{
      {"GaiaID", account_manager::AccountType::kGaia}, "raw_email"};
  ProfileAccountManager manager(mapper(), kProfilePath);
  account_manager::MockAccountManagerFacadeObserver mock_observer;
  base::ScopedObservation<ProfileAccountManager,
                          account_manager::AccountManagerFacade::Observer>
      observation{&mock_observer};
  observation.Observe(&manager);
  // Observer is called for the relevant profile.
  EXPECT_CALL(mock_observer,
              OnAccountUpserted(testing::Field(&account_manager::Account::key,
                                               kAccount.key)));
  manager.OnAccountUpserted(kProfilePath, kAccount);
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  EXPECT_CALL(mock_observer,
              OnAccountRemoved(testing::Field(&account_manager::Account::key,
                                              kAccount.key)));
  manager.OnAccountRemoved(kProfilePath, kAccount);
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  // Observer is not called for another profile.
  const base::FilePath kOtherPath("/Other/Path");
  EXPECT_CALL(mock_observer, OnAccountUpserted(testing::_)).Times(0);
  EXPECT_CALL(mock_observer, OnAccountRemoved(testing::_)).Times(0);
  manager.OnAccountUpserted(kOtherPath, kAccount);
  manager.OnAccountRemoved(kOtherPath, kAccount);
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
}
