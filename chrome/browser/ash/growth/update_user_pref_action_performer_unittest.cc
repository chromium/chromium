// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/growth/update_user_pref_action_performer.h"

#include <memory>
#include <optional>

#include "base/containers/contains.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/growth/campaigns_logger.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
inline constexpr char kTestProfileName[] = "profile@test.com";

inline constexpr char kStringPref[] = "string_pref";
inline constexpr char kListPref[] = "list_pref";
inline constexpr char kInvalidPref[] = "invalid_pref";

inline constexpr char kDefaultValue[] = "pref_value";
inline constexpr char kRemoveValue[] = "remove";
inline constexpr char kTestValue[] = "test";

constexpr char kUpdateUserPrefTemplate[] = R"(
    {
      "name": "%s",
      "type": %d,
      "value": "%s"
    }
)";

constexpr char kClearUserPrefTemplate[] = R"(
    {
      "name": "%s",
      "type": %d
    }
)";
}  // namespace

class UpdateUserPrefActionPerformerTest : public testing::Test {
 public:
  UpdateUserPrefActionPerformerTest() = default;
  UpdateUserPrefActionPerformerTest(const UpdateUserPrefActionPerformerTest&) =
      delete;
  UpdateUserPrefActionPerformerTest& operator=(
      const UpdateUserPrefActionPerformerTest&) = delete;
  ~UpdateUserPrefActionPerformerTest() override = default;

  void SetUp() override {
    user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    const user_manager::User* user =
        user_manager_->AddUser(AccountId::FromUserEmail(kTestProfileName));
    user_manager_->LoginUser(user->GetAccountId());
    user_manager_->SwitchActiveUser(user->GetAccountId());

    // Note that user profiles are created after user login in reality.
    // profile_ = profile_manager_->CreateTestingProfile(kTestProfileName);
    auto prefs = GetDefaultPrefs();
    profile_manager_->CreateTestingProfile(kTestProfileName, std::move(prefs),
                                           u"User Name", /*avatar_id=*/0,
                                           TestingProfile::TestingFactories());

    action_ = std::make_unique<UpdateUserPrefActionPerformer>();
  }

  void TearDown() override {
    // action_->SetPrefsForTesting(nullptr);
    profile_manager_->DeleteAllTestingProfiles();
    profile_manager_.reset();
    testing::Test::TearDown();
  }

  UpdateUserPrefActionPerformer& action() { return *action_; }

  void RunActionPerformerCallback(
      growth::ActionResult result,
      std::optional<growth::ActionResultReason> reason) {
    if (result == growth::ActionResult::kSuccess) {
      std::move(action_success_closure_).Run();
    } else {
      std::move(action_failed_closure_).Run();
    }
  }

  bool VerifyActionResult(bool success) {
    if (success) {
      action_success_run_loop_.Run();
    } else {
      action_failed_run_loop_.Run();
    }
    return true;
  }

  bool VerifyListPrefsEmpty() {
    auto* prefs_ = ProfileManager::GetActiveUserProfile()->GetPrefs();
    auto& pref_value = prefs_->GetList(kListPref);
    // LOG(ERROR) << "pref_value " << pref_value;
    return pref_value.empty();
  }

  bool VerifyPrefsValueEqual(const std::string& value) {
    auto* prefs_ = ProfileManager::GetActiveUserProfile()->GetPrefs();
    auto pref_value = prefs_->GetString(kStringPref);
    return value == pref_value;
  }

  bool VerifyListPrefsContainsValue(const std::string& value) {
    auto* prefs_ = ProfileManager::GetActiveUserProfile()->GetPrefs();
    return base::Contains(prefs_->GetList(kListPref), value);
  }

 private:
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
  GetDefaultPrefs() const {
    auto prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(prefs->registry());
    prefs->registry()->RegisterStringPref(kStringPref, std::string());
    prefs->registry()->RegisterListPref(kListPref, base::Value::List());
    prefs->SetString(kStringPref, kDefaultValue);
    prefs->SetList(
        kListPref,
        base::Value::List().Append(kDefaultValue).Append(kRemoveValue));
    return prefs;
  }

  content::BrowserTaskEnvironment task_environment_;

  base::RunLoop action_success_run_loop_;
  base::RunLoop action_failed_run_loop_;

  base::OnceClosure action_success_closure_ =
      action_success_run_loop_.QuitClosure();
  base::OnceClosure action_failed_closure_ =
      action_failed_run_loop_.QuitClosure();

  std::unique_ptr<UpdateUserPrefActionPerformer> action_;
  growth::CampaignsLogger logger_;

  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      user_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
};

TEST_F(UpdateUserPrefActionPerformerTest, TestValidSetPref) {
  const auto validUpdateUserPrefParam =
      base::StringPrintf(kUpdateUserPrefTemplate, kStringPref, 0, kTestValue);
  auto value = base::JSONReader::Read(validUpdateUserPrefParam);
  ASSERT_TRUE(value.has_value());
  EXPECT_TRUE(VerifyPrefsValueEqual(kDefaultValue));
  action().Run(
      /*campaign_id=*/1, /*group_id=*/std::nullopt, &value->GetDict(),
      base::BindOnce(
          &UpdateUserPrefActionPerformerTest::RunActionPerformerCallback,
          base::Unretained(this)));

  EXPECT_TRUE(VerifyActionResult(/*success=*/true));
  EXPECT_TRUE(VerifyPrefsValueEqual(kTestValue));
}

TEST_F(UpdateUserPrefActionPerformerTest, TestSetNonExistPref) {
  const auto validUpdateUserPrefParam =
      base::StringPrintf(kUpdateUserPrefTemplate, kInvalidPref, 0, kTestValue);
  auto value = base::JSONReader::Read(validUpdateUserPrefParam);
  ASSERT_TRUE(value.has_value());
  action().Run(
      /*campaign_id=*/1, /*group_id=*/std::nullopt, &value->GetDict(),
      base::BindOnce(
          &UpdateUserPrefActionPerformerTest::RunActionPerformerCallback,
          base::Unretained(this)));

  EXPECT_TRUE(VerifyActionResult(/*success=*/false));
}

TEST_F(UpdateUserPrefActionPerformerTest, TestSetWrongTypePref) {
  const auto validUpdateUserPrefParam =
      base::StringPrintf(kUpdateUserPrefTemplate, kListPref, 0, kTestValue);
  auto value = base::JSONReader::Read(validUpdateUserPrefParam);
  ASSERT_TRUE(value.has_value());
  action().Run(
      /*campaign_id=*/1, /*group_id=*/std::nullopt, &value->GetDict(),
      base::BindOnce(
          &UpdateUserPrefActionPerformerTest::RunActionPerformerCallback,
          base::Unretained(this)));

  EXPECT_TRUE(VerifyActionResult(/*success=*/false));
}

TEST_F(UpdateUserPrefActionPerformerTest, TestSetPrefMissingValue) {
  const auto validUpdateUserPrefParam =
      base::StringPrintf(kClearUserPrefTemplate, kListPref, 0);
  auto value = base::JSONReader::Read(validUpdateUserPrefParam);
  ASSERT_TRUE(value.has_value());
  action().Run(
      /*campaign_id=*/1, /*group_id=*/std::nullopt, &value->GetDict(),
      base::BindOnce(
          &UpdateUserPrefActionPerformerTest::RunActionPerformerCallback,
          base::Unretained(this)));

  EXPECT_TRUE(VerifyActionResult(/*success=*/false));
}

TEST_F(UpdateUserPrefActionPerformerTest, TestValidClearStringPref) {
  const auto validUpdateUserPrefParam =
      base::StringPrintf(kClearUserPrefTemplate, kStringPref, 1);
  auto value = base::JSONReader::Read(validUpdateUserPrefParam);
  ASSERT_TRUE(value.has_value());
  EXPECT_TRUE(VerifyPrefsValueEqual(kDefaultValue));
  action().Run(
      /*campaign_id=*/1, /*group_id=*/std::nullopt, &value->GetDict(),
      base::BindOnce(
          &UpdateUserPrefActionPerformerTest::RunActionPerformerCallback,
          base::Unretained(this)));

  EXPECT_TRUE(VerifyActionResult(/*success=*/true));
  EXPECT_TRUE(VerifyPrefsValueEqual(std::string()));
}

TEST_F(UpdateUserPrefActionPerformerTest, TestValidClearListPref) {
  const auto validUpdateUserPrefParam =
      base::StringPrintf(kClearUserPrefTemplate, kListPref, 1);
  auto value = base::JSONReader::Read(validUpdateUserPrefParam);
  ASSERT_TRUE(value.has_value());
  EXPECT_FALSE(VerifyListPrefsEmpty());
  action().Run(
      /*campaign_id=*/1, /*group_id=*/std::nullopt, &value->GetDict(),
      base::BindOnce(
          &UpdateUserPrefActionPerformerTest::RunActionPerformerCallback,
          base::Unretained(this)));

  EXPECT_TRUE(VerifyActionResult(/*success=*/true));
  EXPECT_TRUE(VerifyListPrefsEmpty());
}

TEST_F(UpdateUserPrefActionPerformerTest, TestClearNonExistPref) {
  const auto validUpdateUserPrefParam =
      base::StringPrintf(kClearUserPrefTemplate, kInvalidPref, 1);
  auto value = base::JSONReader::Read(validUpdateUserPrefParam);
  ASSERT_TRUE(value.has_value());
  action().Run(
      /*campaign_id=*/1, /*group_id=*/std::nullopt, &value->GetDict(),
      base::BindOnce(
          &UpdateUserPrefActionPerformerTest::RunActionPerformerCallback,
          base::Unretained(this)));

  EXPECT_TRUE(VerifyActionResult(/*success=*/false));
}
TEST_F(UpdateUserPrefActionPerformerTest, TestAppendToPref) {
  const auto validUpdateUserPrefParam =
      base::StringPrintf(kUpdateUserPrefTemplate, kListPref, 2, kTestValue);
  auto value = base::JSONReader::Read(validUpdateUserPrefParam);
  ASSERT_TRUE(value.has_value());
  EXPECT_FALSE(VerifyListPrefsContainsValue(kTestValue));
  action().Run(
      /*campaign_id=*/1, /*group_id=*/std::nullopt, &value->GetDict(),
      base::BindOnce(
          &UpdateUserPrefActionPerformerTest::RunActionPerformerCallback,
          base::Unretained(this)));

  EXPECT_TRUE(VerifyActionResult(/*success=*/true));
  EXPECT_TRUE(VerifyListPrefsContainsValue(kTestValue));
}

TEST_F(UpdateUserPrefActionPerformerTest, TestAppendToNonListPref) {
  const auto validUpdateUserPrefParam =
      base::StringPrintf(kUpdateUserPrefTemplate, kStringPref, 2, kTestValue);
  auto value = base::JSONReader::Read(validUpdateUserPrefParam);
  ASSERT_TRUE(value.has_value());
  EXPECT_TRUE(VerifyPrefsValueEqual(kDefaultValue));
  action().Run(
      /*campaign_id=*/1, /*group_id=*/std::nullopt, &value->GetDict(),
      base::BindOnce(
          &UpdateUserPrefActionPerformerTest::RunActionPerformerCallback,
          base::Unretained(this)));

  EXPECT_TRUE(VerifyActionResult(/*success=*/false));
  EXPECT_TRUE(VerifyPrefsValueEqual(kDefaultValue));
}

TEST_F(UpdateUserPrefActionPerformerTest, TestAppendToNonExistPref) {
  const auto validUpdateUserPrefParam =
      base::StringPrintf(kUpdateUserPrefTemplate, kInvalidPref, 2, kTestValue);
  auto value = base::JSONReader::Read(validUpdateUserPrefParam);
  ASSERT_TRUE(value.has_value());
  action().Run(
      /*campaign_id=*/1, /*group_id=*/std::nullopt, &value->GetDict(),
      base::BindOnce(
          &UpdateUserPrefActionPerformerTest::RunActionPerformerCallback,
          base::Unretained(this)));

  EXPECT_TRUE(VerifyActionResult(/*success=*/false));
}

TEST_F(UpdateUserPrefActionPerformerTest, TestRemoveFromPref) {
  const auto validUpdateUserPrefParam =
      base::StringPrintf(kUpdateUserPrefTemplate, kListPref, 3, kRemoveValue);
  auto value = base::JSONReader::Read(validUpdateUserPrefParam);
  ASSERT_TRUE(value.has_value());
  EXPECT_TRUE(VerifyListPrefsContainsValue(kRemoveValue));
  action().Run(
      /*campaign_id=*/1, /*group_id=*/std::nullopt, &value->GetDict(),
      base::BindOnce(
          &UpdateUserPrefActionPerformerTest::RunActionPerformerCallback,
          base::Unretained(this)));

  EXPECT_TRUE(VerifyActionResult(/*success=*/true));
  EXPECT_FALSE(VerifyListPrefsContainsValue(kRemoveValue));
}

TEST_F(UpdateUserPrefActionPerformerTest, TestRemoveFromNonListPref) {
  const auto validUpdateUserPrefParam =
      base::StringPrintf(kUpdateUserPrefTemplate, kStringPref, 3, kRemoveValue);
  auto value = base::JSONReader::Read(validUpdateUserPrefParam);
  ASSERT_TRUE(value.has_value());
  EXPECT_TRUE(VerifyPrefsValueEqual(kDefaultValue));
  action().Run(
      /*campaign_id=*/1, /*group_id=*/std::nullopt, &value->GetDict(),
      base::BindOnce(
          &UpdateUserPrefActionPerformerTest::RunActionPerformerCallback,
          base::Unretained(this)));

  EXPECT_TRUE(VerifyActionResult(/*success=*/false));
  EXPECT_TRUE(VerifyPrefsValueEqual(kDefaultValue));
}

TEST_F(UpdateUserPrefActionPerformerTest, TestRemoveFromNonExistPref) {
  const auto validUpdateUserPrefParam =
      base::StringPrintf(kUpdateUserPrefTemplate, kInvalidPref, 3, kTestValue);
  auto value = base::JSONReader::Read(validUpdateUserPrefParam);
  ASSERT_TRUE(value.has_value());
  action().Run(
      /*campaign_id=*/1, /*group_id=*/std::nullopt, &value->GetDict(),
      base::BindOnce(
          &UpdateUserPrefActionPerformerTest::RunActionPerformerCallback,
          base::Unretained(this)));

  EXPECT_TRUE(VerifyActionResult(/*success=*/false));
}
