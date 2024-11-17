// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/external_app_redirect_checking.h"

#include "base/json/values_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/browser/db/test_database_manager.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

using ::testing::_;
using ::testing::Invoke;

namespace {

class MockSafeBrowsingDatabaseManager : public TestSafeBrowsingDatabaseManager {
 public:
  MockSafeBrowsingDatabaseManager()
      : TestSafeBrowsingDatabaseManager(
            base::SequencedTaskRunner::GetCurrentDefault()) {}

  MOCK_METHOD(void,
              CheckUrlForHighConfidenceAllowlist,
              (const GURL&,
               SafeBrowsingDatabaseManager::
                   CheckUrlForHighConfidenceAllowlistCallback));

 private:
  ~MockSafeBrowsingDatabaseManager() override = default;
};

class ExternalAppRedirectCheckingTest : public ::testing::Test {
 public:
  ExternalAppRedirectCheckingTest()
      : feature_list_(kExternalAppRedirectTelemetry),
        mock_database_(base::MakeRefCounted<MockSafeBrowsingDatabaseManager>()),
        web_contents_(
            content::WebContentsTester::CreateTestWebContents(&profile_,
                                                              nullptr)) {}
  ~ExternalAppRedirectCheckingTest() override = default;

  void SetUp() override {
    SetSafeBrowsingState(profile_.GetPrefs(),
                         SafeBrowsingState::ENHANCED_PROTECTION);
    SetSitesAllowlisted(/*allowlisted=*/false);
  }

 protected:
  content::WebContents* web_contents() { return web_contents_.get(); }
  scoped_refptr<MockSafeBrowsingDatabaseManager> mock_database() {
    return mock_database_;
  }
  TestingProfile& profile() { return profile_; }
  void SetSitesAllowlisted(bool allowlisted) {
    ON_CALL(*mock_database_, CheckUrlForHighConfidenceAllowlist(_, _))
        .WillByDefault(Invoke(
            [allowlisted](
                const GURL& url,
                SafeBrowsingDatabaseManager::
                    CheckUrlForHighConfidenceAllowlistCallback callback) {
              std::move(callback).Run(allowlisted, std::nullopt);
            }));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  TestingProfile profile_;
  scoped_refptr<MockSafeBrowsingDatabaseManager> mock_database_;
  std::unique_ptr<content::WebContents> web_contents_;
};

}  // namespace

TEST_F(ExternalAppRedirectCheckingTest, ShouldReportExternalAppRedirect) {
  base::test::TestFuture<bool> future;
  ShouldReportExternalAppRedirect(mock_database(), web_contents(), "test.app",
                                  "https://evil.com", future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(ExternalAppRedirectCheckingTest,
       ShouldReportExternalAppRedirect_Allowlisted) {
  SetSitesAllowlisted(/*allowlisted=*/true);

  base::test::TestFuture<bool> future;
  ShouldReportExternalAppRedirect(mock_database(), web_contents(), "test.app",
                                  "https://evil.com", future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(ExternalAppRedirectCheckingTest,
       ShouldReportExternalAppRedirect_RecentAppVisit) {
  base::Value::Dict initial_timestamps;
  initial_timestamps.Set("test_app", base::TimeToValue(base::Time::Now()));
  profile().GetPrefs()->SetDict(prefs::kExternalAppRedirectTimestamps,
                                std::move(initial_timestamps));

  base::test::TestFuture<bool> future;
  ShouldReportExternalAppRedirect(mock_database(), web_contents(), "test.app",
                                  "https://evil.com", future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(ExternalAppRedirectCheckingTest,
       ShouldReportExternalAppRedirect_StandardSafeBrowsing) {
  SetSafeBrowsingState(profile().GetPrefs(),
                       SafeBrowsingState::STANDARD_PROTECTION);

  base::test::TestFuture<bool> future;
  ShouldReportExternalAppRedirect(mock_database(), web_contents(), "test.app",
                                  "https://evil.com", future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(ExternalAppRedirectCheckingTest,
       ShouldReportExternalAppRedirect_OffTheRecord) {
  auto otr_web_contents = content::WebContentsTester::CreateTestWebContents(
      profile().GetOffTheRecordProfile(
          Profile::OTRProfileID::CreateUniqueForTesting(),
          /*create_if_needed=*/true),
      nullptr);

  base::test::TestFuture<bool> future;
  ShouldReportExternalAppRedirect(mock_database(), otr_web_contents.get(),
                                  "test.app", "https://evil.com",
                                  future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(ExternalAppRedirectCheckingTest, ShouldReportExternalAppRedirect_NoUri) {
  base::test::TestFuture<bool> future;
  ShouldReportExternalAppRedirect(mock_database(), web_contents(), "test.app",
                                  /*uri=*/"", future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST(ExternalAppRedirectChecking, LogExternalAppRedirectTimestamp) {
  TestingPrefServiceSimple pref_service;
  RegisterProfilePrefs(pref_service.registry());

  ASSERT_EQ(pref_service.GetDict(prefs::kExternalAppRedirectTimestamps),
            base::Value::Dict());

  LogExternalAppRedirectTimestamp(pref_service, "test.app");

  std::optional<base::Time> first_time = base::ValueToTime(
      pref_service.GetDict(prefs::kExternalAppRedirectTimestamps)
          .Find("test_app"));
  ASSERT_TRUE(first_time.has_value());

  LogExternalAppRedirectTimestamp(pref_service, "test.app");

  std::optional<base::Time> second_time = base::ValueToTime(
      pref_service.GetDict(prefs::kExternalAppRedirectTimestamps)
          .Find("test_app"));
  ASSERT_TRUE(second_time.has_value());

  EXPECT_LT(first_time, second_time);
}

TEST(ExternalAppRedirectChecking, CleanupExternalAppRedirectTimestamps) {
  TestingPrefServiceSimple pref_service;
  RegisterProfilePrefs(pref_service.registry());

  base::Value::Dict initial_timestamps;
  initial_timestamps.Set("test_app", base::TimeToValue(base::Time::Now()));
  initial_timestamps.Set(
      "expired_app", base::TimeToValue(base::Time::Now() - base::Days(100)));
  pref_service.SetDict(prefs::kExternalAppRedirectTimestamps,
                       std::move(initial_timestamps));

  ASSERT_NE(pref_service.GetDict(prefs::kExternalAppRedirectTimestamps)
                .Find("test_app"),
            nullptr);
  ASSERT_NE(pref_service.GetDict(prefs::kExternalAppRedirectTimestamps)
                .Find("expired_app"),
            nullptr);

  CleanupExternalAppRedirectTimestamps(pref_service);
  EXPECT_NE(pref_service.GetDict(prefs::kExternalAppRedirectTimestamps)
                .Find("test_app"),
            nullptr);
  EXPECT_EQ(pref_service.GetDict(prefs::kExternalAppRedirectTimestamps)
                .Find("expired_app"),
            nullptr);
}

}  // namespace safe_browsing
