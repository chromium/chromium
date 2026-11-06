// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/growth/update_user_pref_action_performer.h"

#include <memory>
#include <optional>

#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/login/test/chrome_user_session_test_environment_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/growth/campaigns_logger.h"
#include "components/account_id/account_id_literal.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr auto kAccountId =
    AccountId::Literal::FromUserEmailGaiaId("profile@test.com",
                                            GaiaId::Literal("1234567890"));

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
    auto* browser_process = TestingBrowserProcess::GetGlobal();
    user_session_test_environment_ = std::make_unique<
        ash::test::UserSessionTestEnvironment>(
        browser_process->local_state(),
        std::make_unique<ash::test::ChromeUserSessionTestEnvironmentDelegate>(
            browser_process));
    ASSERT_TRUE(user_session_test_environment_->AddRegularUser(kAccountId));
    user_session_test_environment_->LogIn(kAccountId);
    profile_ = static_cast<TestingProfile*>(Profile::FromBrowserContext(
        ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
            kAccountId)));
    ASSERT_TRUE(profile_);

    sync_preferences::TestingPrefServiceSyncable* prefs =
        profile_->GetTestingPrefService();
    prefs->registry()->RegisterStringPref(kStringPref, std::string());
    prefs->registry()->RegisterListPref(kListPref, base::ListValue());
    prefs->SetString(kStringPref, kDefaultValue);
    prefs->SetList(
        kListPref,
        base::ListValue().Append(kDefaultValue).Append(kRemoveValue));

    action_ = std::make_unique<UpdateUserPrefActionPerformer>();
  }

  void TearDown() override {
    action_.reset();
    profile_ = nullptr;
    user_session_test_environment_.reset();
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
    return profile_->GetPrefs()->GetList(kListPref).empty();
  }

  bool VerifyPrefsValueEqual(const std::string& value) {
    return value == profile_->GetPrefs()->GetString(kStringPref);
  }

  bool VerifyListPrefsContainsValue(const std::string& value) {
    return profile_->GetPrefs()->GetList(kListPref).contains(value);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  base::RunLoop action_success_run_loop_;
  base::RunLoop action_failed_run_loop_;

  base::OnceClosure action_success_closure_ =
      action_success_run_loop_.QuitClosure();
  base::OnceClosure action_failed_closure_ =
      action_failed_run_loop_.QuitClosure();

  growth::CampaignsLogger logger_;

  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
  raw_ptr<TestingProfile> profile_ = nullptr;
  std::unique_ptr<UpdateUserPrefActionPerformer> action_;
};

TEST_F(UpdateUserPrefActionPerformerTest, TestValidSetPref) {
  const auto validUpdateUserPrefParam =
      base::StringPrintf(kUpdateUserPrefTemplate, kStringPref, 0, kTestValue);
  auto value = base::JSONReader::Read(validUpdateUserPrefParam,
                                      base::JSON_PARSE_CHROMIUM_EXTENSIONS);
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
  auto value = base::JSONReader::Read(validUpdateUserPrefParam,
                                      base::JSON_PARSE_CHROMIUM_EXTENSIONS);
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
  auto value = base::JSONReader::Read(validUpdateUserPrefParam,
                                      base::JSON_PARSE_CHROMIUM_EXTENSIONS);
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
  auto value = base::JSONReader::Read(validUpdateUserPrefParam,
                                      base::JSON_PARSE_CHROMIUM_EXTENSIONS);
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
  auto value = base::JSONReader::Read(validUpdateUserPrefParam,
                                      base::JSON_PARSE_CHROMIUM_EXTENSIONS);
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
  auto value = base::JSONReader::Read(validUpdateUserPrefParam,
                                      base::JSON_PARSE_CHROMIUM_EXTENSIONS);
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
  auto value = base::JSONReader::Read(validUpdateUserPrefParam,
                                      base::JSON_PARSE_CHROMIUM_EXTENSIONS);
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
  auto value = base::JSONReader::Read(validUpdateUserPrefParam,
                                      base::JSON_PARSE_CHROMIUM_EXTENSIONS);
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
  auto value = base::JSONReader::Read(validUpdateUserPrefParam,
                                      base::JSON_PARSE_CHROMIUM_EXTENSIONS);
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
  auto value = base::JSONReader::Read(validUpdateUserPrefParam,
                                      base::JSON_PARSE_CHROMIUM_EXTENSIONS);
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
  auto value = base::JSONReader::Read(validUpdateUserPrefParam,
                                      base::JSON_PARSE_CHROMIUM_EXTENSIONS);
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
  auto value = base::JSONReader::Read(validUpdateUserPrefParam,
                                      base::JSON_PARSE_CHROMIUM_EXTENSIONS);
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
  auto value = base::JSONReader::Read(validUpdateUserPrefParam,
                                      base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(value.has_value());
  action().Run(
      /*campaign_id=*/1, /*group_id=*/std::nullopt, &value->GetDict(),
      base::BindOnce(
          &UpdateUserPrefActionPerformerTest::RunActionPerformerCallback,
          base::Unretained(this)));

  EXPECT_TRUE(VerifyActionResult(/*success=*/false));
}
