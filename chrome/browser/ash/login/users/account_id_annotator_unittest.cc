// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/account_id_annotator.h"

#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class AccountIdAnnotatorTest : public testing::Test {
 public:
  void SetUp() override {
    // Instantiate ProfileHelper.
    ProfileHelper::Get();

    ASSERT_TRUE(testing_profile_manager_.SetUp());
    annotator_ = std::make_unique<AccountIdAnnotator>(
        testing_profile_manager_.profile_manager(),
        user_manager::UserManager::Get());
  }

  test::UserSessionTestEnvironment& user_session_test_environment() {
    return user_session_test_environment_;
  }

  TestingProfileManager& testing_profile_manager() {
    return testing_profile_manager_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ScopedCrosSettingsTestHelper settings_helper_;
  test::UserSessionTestEnvironment user_session_test_environment_{
      TestingBrowserProcess::GetGlobal()->local_state()};
  // To follow the destruction order in the production, declare annotator's
  // pointer first.
  std::unique_ptr<AccountIdAnnotator> annotator_;
  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};
};

TEST_F(AccountIdAnnotatorTest, AnnotateAccountId) {
  const AccountId kAccountId = AccountId::FromUserEmailGaiaId(
      "account@example.com", GaiaId("1234567890"));

  // Log in the user and create the profile.
  ASSERT_TRUE(user_session_test_environment().AddRegularUser(kAccountId));
  user_session_test_environment().LogIn(kAccountId);

  // Trigger OnProfileCreationStarted() which annotates AccountId.
  auto* profile =
      testing_profile_manager().CreateTestingProfile(kAccountId.GetUserEmail());

  auto* account_id = ash::AnnotatedAccountId::Get(profile);
  ASSERT_TRUE(account_id);
  EXPECT_EQ(*account_id, kAccountId);
}

}  // namespace ash
