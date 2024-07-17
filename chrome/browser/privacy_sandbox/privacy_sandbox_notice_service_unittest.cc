// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_notice_service.h"

#include "base/json/values_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/prefs/testing_pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_notice_constants.h"
#include "components/privacy_sandbox/privacy_sandbox_notice_storage.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {
namespace {

// TODO(crbug.com/333408794): Add browser tests ensuring prefs are migrated
// after notice flow completion.
class PrivacySandboxNoticeServiceTest : public testing::Test {
 public:
  PrivacySandboxNoticeServiceTest()
      : task_env_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    RegisterProfilePrefs(prefs()->registry());
  }

  std::string GetTopicsNoticeName() {
#if BUILDFLAG(IS_ANDROID)
    return kTopicsConsentModalClankBrApp;
#else
    return kTopicsConsentModal;
#endif
  }

  TestingPrefServiceSimple* prefs() { return &prefs_; }

 private:
  base::test::TaskEnvironment task_env_;
  TestingPrefServiceSimple prefs_;
};

// If original prefs are set from Settings, then new prefs shouldn't be set and
// should be their default values.
TEST_F(PrivacySandboxNoticeServiceTest,
       NoticeActionSetWhenMigratedFromSettings) {
  prefs()->SetBoolean(prefs::kPrivacySandboxTopicsConsentGiven, true);
  prefs()->SetInteger(prefs::kPrivacySandboxTopicsConsentLastUpdateReason,
                      static_cast<int>(TopicsConsentUpdateSource::kSettings));
  prefs()->SetTime(prefs::kPrivacySandboxTopicsConsentLastUpdateTime,
                   base::Time::FromMillisecondsSinceUnixEpoch(100));
  auto notice_service = PrivacySandboxNoticeService(prefs());
  const auto result = notice_service.GetNoticeStorage()->ReadNoticeData(
      prefs(), GetTopicsNoticeName());
  EXPECT_EQ(result->schema_version, 1);
  EXPECT_EQ(result->notice_action_taken,
            NoticeActionTaken::kUnknownActionPreMigration);
  EXPECT_EQ(base::TimeToValue(result->notice_action_taken_time), "0");
}

TEST_F(PrivacySandboxNoticeServiceTest,
       PrefsNotSetWhenMigratedFromTopicsWithNoConsentDecision) {
  prefs()->SetInteger(
      prefs::kPrivacySandboxTopicsConsentLastUpdateReason,
      static_cast<int>(TopicsConsentUpdateSource::kConfirmation));
  auto notice_service = privacy_sandbox::PrivacySandboxNoticeService(prefs());
  const auto result = notice_service.GetNoticeStorage()->ReadNoticeData(
      prefs(), GetTopicsNoticeName());
  EXPECT_EQ(result, std::nullopt);
}

TEST_F(PrivacySandboxNoticeServiceTest,
       NoticeActionAndTimeSetWhenMigratedFromTopicsWithConsent) {
  prefs()->SetInteger(
      prefs::kPrivacySandboxTopicsConsentLastUpdateReason,
      static_cast<int>(TopicsConsentUpdateSource::kConfirmation));
  prefs()->SetBoolean(prefs::kPrivacySandboxTopicsConsentGiven, true);
  prefs()->SetTime(prefs::kPrivacySandboxTopicsConsentLastUpdateTime,
                   base::Time::FromMillisecondsSinceUnixEpoch(100));
  auto notice_service = PrivacySandboxNoticeService(prefs());
  const auto result = notice_service.GetNoticeStorage()->ReadNoticeData(
      prefs(), GetTopicsNoticeName());
  EXPECT_EQ(result->schema_version, 1);
  EXPECT_EQ(result->notice_action_taken, NoticeActionTaken::kOptIn);
  EXPECT_EQ(result->notice_action_taken_time,
            base::Time::FromMillisecondsSinceUnixEpoch(100));
}

TEST_F(PrivacySandboxNoticeServiceTest,
       NoticeActionAndTimeSetWhenMigratedFromTopicsWithoutConsent) {
  prefs()->SetInteger(
      prefs::kPrivacySandboxTopicsConsentLastUpdateReason,
      static_cast<int>(TopicsConsentUpdateSource::kConfirmation));
  prefs()->SetBoolean(prefs::kPrivacySandboxTopicsConsentGiven, false);
  prefs()->SetTime(prefs::kPrivacySandboxTopicsConsentLastUpdateTime,
                   base::Time::FromMillisecondsSinceUnixEpoch(100));
  auto notice_service = PrivacySandboxNoticeService(prefs());
  const auto result = notice_service.GetNoticeStorage()->ReadNoticeData(
      prefs(), GetTopicsNoticeName());
  EXPECT_EQ(result->schema_version, 1);
  EXPECT_EQ(result->notice_action_taken, NoticeActionTaken::kOptOut);
  EXPECT_EQ(result->notice_action_taken_time,
            base::Time::FromMillisecondsSinceUnixEpoch(100));
}

}  // namespace
}  // namespace privacy_sandbox
