// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/public/cpp/tablet_mode.h"
#include "chrome/browser/ash/hats/hats_config.h"
#include "chrome/browser/ash/login/hats_unlock_survey_trigger.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/session_manager/session_manager_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

const char kUserEmail[] = "example@gmail.com";
const char kAuthMethodKey[] = "authMethod";
const char kTabletModeKey[] = "tabletMode";
const char kSmartLockEnabledKey[] = "smartLockEnabled";
const char kSmartLockGetRemoteStatusUnlockKey[] =
    "smartLockGetRemoteStatusUnlock";
const char kAuthMethodSmartlockValue[] = "smartlock";
const char kAuthMethodPasswordValue[] = "password";
const char kFalse[] = "false";
const char kTrue[] = "true";

class FakeImpl : public HatsUnlockSurveyTrigger::Impl {
 public:
  bool ShouldShowSurveyToProfile(Profile* profile,
                                 const HatsConfig& hats_config) override {
    return should_show_survey_;
  }

  void ShowSurvey(Profile* profile,
                  const HatsConfig& hats_config,
                  const base::flat_map<std::string, std::string>&
                      product_specific_data) override {
    show_survey_called_ = true;
    product_specific_data_ = product_specific_data;
    hats_config_ = &hats_config;
  }

  bool should_show_survey_ = true;
  bool show_survey_called_ = false;
  base::flat_map<std::string, std::string> product_specific_data_;
  const HatsConfig* hats_config_;
};

}  // namespace

class HatsUnlockSurveyTriggerTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    // SessionManager is created by
    // |AshTestHelper::bluetooth_config_test_helper()|.
    session_manager()->SetSessionState(session_manager::SessionState::LOCKED);

    account_id_ = AccountId::FromUserEmail(kUserEmail);

    TabletMode::Get()->SetEnabledForTest(false);

    std::unique_ptr<FakeImpl> impl = std::make_unique<FakeImpl>();
    fake_impl_ = impl.get();
    unlock_survey_trigger_ =
        std::make_unique<HatsUnlockSurveyTrigger>(std::move(impl));

    unlock_survey_trigger_->SetProfileForTesting(profile());
  }

 protected:
  session_manager::SessionManager* session_manager() {
    return session_manager::SessionManager::Get();
  }

  AccountId account_id_;
  FakeImpl* fake_impl_;
  std::unique_ptr<HatsUnlockSurveyTrigger> unlock_survey_trigger_;
};

TEST_F(HatsUnlockSurveyTriggerTest, ShowSurveyCalled) {
  unlock_survey_trigger_->ShowSurveyIfSelected(
      account_id_, HatsUnlockSurveyTrigger::AuthMethod::kPassword);
  EXPECT_TRUE(fake_impl_->show_survey_called_);
}

TEST_F(HatsUnlockSurveyTriggerTest, ShowSurveyNotCalledIfSessionNotLocked) {
  session_manager()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);

  unlock_survey_trigger_->ShowSurveyIfSelected(
      account_id_, HatsUnlockSurveyTrigger::AuthMethod::kPassword);
  EXPECT_FALSE(fake_impl_->show_survey_called_);
}

TEST_F(HatsUnlockSurveyTriggerTest, ShouldShowSurvey) {
  fake_impl_->should_show_survey_ = false;

  unlock_survey_trigger_->ShowSurveyIfSelected(
      account_id_, HatsUnlockSurveyTrigger::AuthMethod::kPassword);
  EXPECT_FALSE(fake_impl_->show_survey_called_);
}

TEST_F(HatsUnlockSurveyTriggerTest, SmartLockConfig) {
  unlock_survey_trigger_->ShowSurveyIfSelected(
      account_id_, HatsUnlockSurveyTrigger::AuthMethod::kSmartlock);
  ASSERT_TRUE(fake_impl_->show_survey_called_);
  EXPECT_EQ(&kHatsSmartLockSurvey, fake_impl_->hats_config_);
}

TEST_F(HatsUnlockSurveyTriggerTest, NonSmartLockConfig) {
  unlock_survey_trigger_->ShowSurveyIfSelected(
      account_id_, HatsUnlockSurveyTrigger::AuthMethod::kPassword);
  ASSERT_TRUE(fake_impl_->show_survey_called_);
  EXPECT_EQ(&kHatsUnlockSurvey, fake_impl_->hats_config_);
}

TEST_F(HatsUnlockSurveyTriggerTest, ProductSpecificDataPasswordNoTablet) {
  unlock_survey_trigger_->ShowSurveyIfSelected(
      account_id_, HatsUnlockSurveyTrigger::AuthMethod::kPassword);
  ASSERT_TRUE(fake_impl_->show_survey_called_);

  base::flat_map<std::string, std::string>& data =
      fake_impl_->product_specific_data_;
  ASSERT_TRUE(data.contains(kAuthMethodKey));
  ASSERT_TRUE(data.contains(kTabletModeKey));
  ASSERT_TRUE(data.contains(kSmartLockEnabledKey));
  ASSERT_TRUE(data.contains(kSmartLockGetRemoteStatusUnlockKey));
  EXPECT_EQ(kAuthMethodPasswordValue, data[kAuthMethodKey]);
  EXPECT_EQ(kFalse, data[kTabletModeKey]);
  EXPECT_EQ(kFalse, data[kSmartLockEnabledKey]);
}

TEST_F(HatsUnlockSurveyTriggerTest, ProductSpecificDataSmartLockTablet) {
  TabletMode::Get()->SetEnabledForTest(true);
  unlock_survey_trigger_->ShowSurveyIfSelected(
      account_id_, HatsUnlockSurveyTrigger::AuthMethod::kSmartlock);
  ASSERT_TRUE(fake_impl_->show_survey_called_);
  TabletMode::Get()->SetEnabledForTest(false);

  base::flat_map<std::string, std::string>& data =
      fake_impl_->product_specific_data_;
  ASSERT_TRUE(data.contains(kAuthMethodKey));
  ASSERT_TRUE(data.contains(kTabletModeKey));
  ASSERT_TRUE(data.contains(kSmartLockEnabledKey));
  ASSERT_TRUE(data.contains(kSmartLockGetRemoteStatusUnlockKey));
  EXPECT_EQ(kAuthMethodSmartlockValue, data[kAuthMethodKey]);
  EXPECT_EQ(kTrue, data[kTabletModeKey]);
  EXPECT_EQ(kFalse, data[kSmartLockEnabledKey]);
}

}  // namespace ash
