// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/feature_discovery_duration_reporter_impl.h"

#include "ash/public/cpp/ash_prefs.h"
#include "ash/public/cpp/feature_discovery_metric_util.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/contains.h"
#include "base/test/metrics/histogram_tester.h"

namespace ash {

namespace {

// A mock primary user's email.
constexpr char kPrimaryUserEmail[] = "user1@example.com";

// A mock secondary user's email.
constexpr char kSecondaryUserEmail[] = "user2@example.com";

// The mock features' histograms.
constexpr char kMockHistogram[] = "FeatureDiscoveryTestMockFeature";
const char kMockFeatureClamshellHistogram[] =
    "FeatureDiscoveryTestMockFeature.clamshell";
const char kMockFeatureTabletHistogram[] =
    "FeatureDiscoveryTestMockFeature.tablet";

SessionControllerImpl* GetSessionController() {
  return Shell::Get()->session_controller();
}

FeatureDiscoveryDurationReporterImpl* GetFeatureDiscoveryDurationReporter() {
  return Shell::Get()->feature_discover_reporter();
}

}  // namespace

class FeatureDiscoveryDurationReporterImplTest : public AshTestBase {
 public:
  FeatureDiscoveryDurationReporterImplTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  FeatureDiscoveryDurationReporterImplTest(
      const FeatureDiscoveryDurationReporterImplTest&) = delete;
  FeatureDiscoveryDurationReporterImplTest& operator=(
      const FeatureDiscoveryDurationReporterImplTest&) = delete;
  ~FeatureDiscoveryDurationReporterImplTest() override = default;

  bool IsReporterActive() {
    return GetFeatureDiscoveryDurationReporter()->is_active();
  }

  // Returns true if the feature discovery reporter has ongoing observations.
  bool IsMockFeatureUnderActiveObservation() {
    const auto& active_time_recordings =
        GetFeatureDiscoveryDurationReporter()->active_time_recordings_;
    return base::Contains(active_time_recordings,
                          feature_discovery::TrackableFeature::kMockFeature);
  }

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // Set up the primary account and the secondary account.
    GetSessionController()->ClearUserSessionsForTest();
    TestSessionControllerClient* session_client = GetSessionControllerClient();
    session_client->AddUserSession(kPrimaryUserEmail,
                                   user_manager::UserType::kRegular,
                                   /*provide_pref_service=*/false);
    session_client->AddUserSession(kSecondaryUserEmail,
                                   user_manager::UserType::kRegular,
                                   /*provide_pref_service=*/false);

    auto user_1_prefs = std::make_unique<TestingPrefServiceSimple>();
    RegisterUserProfilePrefs(user_1_prefs->registry(), /*country=*/"",
                             /*for_test=*/true);
    auto user_2_prefs = std::make_unique<TestingPrefServiceSimple>();
    RegisterUserProfilePrefs(user_2_prefs->registry(), /*country=*/"",
                             /*for_test=*/true);
    session_client->SetUserPrefService(primary_account_id_,
                                       std::move(user_1_prefs));
    session_client->SetUserPrefService(secondary_account_id_,
                                       std::move(user_2_prefs));

    // Switch to the primary account and lock the screen.
    session_client->SwitchActiveUser(primary_account_id_);
    GetSessionControllerClient()->SetSessionState(
        session_manager::SessionState::LOCKED);
  }

  AccountId primary_account_id_ = AccountId::FromUserEmail(kPrimaryUserEmail);
  AccountId secondary_account_id_ =
      AccountId::FromUserEmail(kSecondaryUserEmail);
};

// Verifies feature discovery duration is only recorded for primary accounts.
TEST_F(FeatureDiscoveryDurationReporterImplTest, OnlyRecordForNewPrimaryUser) {
  // Activate the primary user session then verify that the reporter is active.
  EXPECT_FALSE(IsReporterActive());
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  EXPECT_TRUE(IsReporterActive());

  // Switch to the secondary account. The session should still be active.
  TestSessionControllerClient* session_controller =
      GetSessionControllerClient();
  session_controller->SwitchActiveUser(secondary_account_id_);
  EXPECT_EQ(session_manager::SessionState::ACTIVE,
            GetSessionController()->GetSessionState());

  // The current user is not primary so the reporter is inactive.
  EXPECT_FALSE(IsReporterActive());

  // The metric data should not be recorded because the reporter is inactive.
  base::HistogramTester histogram_tester;
  FeatureDiscoveryDurationReporterImpl* reporter =
      GetFeatureDiscoveryDurationReporter();
  reporter->MaybeActivateObservation(
      feature_discovery::TrackableFeature::kMockFeature);
  task_environment()->FastForwardBy(base::Minutes(1));
  reporter->MaybeFinishObservation(
      feature_discovery::TrackableFeature::kMockFeature);
  histogram_tester.ExpectTotalCount(kMockHistogram, 0);
}

// Verifies that the feature discovery duration is recorded correctly in one
// active session.
TEST_F(FeatureDiscoveryDurationReporterImplTest, CountDurationInOneSession) {
  EXPECT_FALSE(IsReporterActive());
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  EXPECT_TRUE(IsReporterActive());

  // Start observation. Emulate to wait for one minute then stop observation.
  base::HistogramTester histogram_tester;

  // Finishing the observation that has not started should not record any data.
  FeatureDiscoveryDurationReporterImpl* reporter =
      GetFeatureDiscoveryDurationReporter();
  reporter->MaybeFinishObservation(
      feature_discovery::TrackableFeature::kMockFeature);
  histogram_tester.ExpectTotalCount(kMockHistogram, 0);

  reporter->MaybeActivateObservation(
      feature_discovery::TrackableFeature::kMockFeature);
  task_environment()->FastForwardBy(base::Minutes(1));
  reporter->MaybeFinishObservation(
      feature_discovery::TrackableFeature::kMockFeature);

  // Check that one-minute duration is recorded.
  histogram_tester.ExpectUniqueTimeSample(kMockHistogram, base::Minutes(1), 1);

  // Try to observe again. No additional data should be recorded for the same
  // histogram.
  reporter->MaybeActivateObservation(
      feature_discovery::TrackableFeature::kMockFeature);
  task_environment()->FastForwardBy(base::Minutes(1));
  reporter->MaybeFinishObservation(
      feature_discovery::TrackableFeature::kMockFeature);
  histogram_tester.ExpectTotalCount(kMockHistogram, 1);

  // Deactivate the reporter then reactivate it.
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  EXPECT_FALSE(IsReporterActive());
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  // Verify that the finished observation does not resume.
  EXPECT_TRUE(IsReporterActive());
  EXPECT_FALSE(IsMockFeatureUnderActiveObservation());
}

// Verifies that the feature discovery duration is recorded correctly across
// session states (i.e. deactivate a session then activate it before finishing
// the observation).
TEST_F(FeatureDiscoveryDurationReporterImplTest,
       CountDurationAcrossSessionStates) {
  EXPECT_FALSE(IsReporterActive());
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  EXPECT_TRUE(IsReporterActive());

  // Start observation. Emulate to wait for one minute then lock the screen.
  base::HistogramTester histogram_tester;
  FeatureDiscoveryDurationReporterImpl* reporter =
      GetFeatureDiscoveryDurationReporter();
  reporter->MaybeActivateObservation(
      feature_discovery::TrackableFeature::kMockFeature);
  constexpr base::TimeDelta delta1(base::Minutes(1));
  task_environment()->FastForwardBy(delta1);
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);

  // Because the session is inactive, the reporter should be inactive.
  EXPECT_FALSE(IsReporterActive());

  // Emulate to wait for three minutes before reactivating the session.
  task_environment()->FastForwardBy(base::Minutes(3));
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);

  // Emulate to wait for five minutes before finishing observation.
  constexpr base::TimeDelta delta2(base::Minutes(5));
  task_environment()->FastForwardBy(delta2);
  reporter->MaybeFinishObservation(
      feature_discovery::TrackableFeature::kMockFeature);

  // Check that only the time duration under the active session is recorded.
  histogram_tester.ExpectUniqueTimeSample(kMockHistogram, delta1 + delta2, 1);

  // Try to observe again. No additional data should be recorded for the same
  // histogram.
  reporter->MaybeActivateObservation(
      feature_discovery::TrackableFeature::kMockFeature);
  task_environment()->FastForwardBy(base::Minutes(1));
  reporter->MaybeFinishObservation(
      feature_discovery::TrackableFeature::kMockFeature);
  histogram_tester.ExpectTotalCount(kMockHistogram, 1);
}

// Verifies each feature that is supported by the feature discovery duration
// reporter has the unique feature name.
TEST_F(FeatureDiscoveryDurationReporterImplTest, VerifyFeatureNameIsUnique) {
  auto cmp = [](const char* a, const char* b) { return std::strcmp(a, b) > 0; };
  std::set<const char*, decltype(cmp)> feature_names(cmp);
  for (const auto& feature_info : feature_discovery::kTrackableFeatureArray) {
    bool success = feature_names.emplace(feature_info.name).second;
    EXPECT_TRUE(success) << " " << feature_info.name
                         << " is used more than once";
  }
}

// Used to verify that collected data can be split by tablet mode states.
class FeatureDiscoveryDurationReporterDataSplitTest
    : public FeatureDiscoveryDurationReporterImplTest,
      public testing::WithParamInterface</*is_tablet=*/bool> {
 public:
  FeatureDiscoveryDurationReporterDataSplitTest() = default;
  FeatureDiscoveryDurationReporterDataSplitTest(
      const FeatureDiscoveryDurationReporterDataSplitTest&) = delete;
  FeatureDiscoveryDurationReporterDataSplitTest& operator=(
      const FeatureDiscoveryDurationReporterDataSplitTest&) = delete;
  ~FeatureDiscoveryDurationReporterDataSplitTest() override = default;

  // FeatureDiscoveryDurationReporterImplTest:
  void SetUp() override {
    FeatureDiscoveryDurationReporterImplTest::SetUp();
    TabletMode::Get()->SetEnabledForTest(GetParam());
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         FeatureDiscoveryDurationReporterDataSplitTest,
                         testing::Bool());

TEST_P(FeatureDiscoveryDurationReporterDataSplitTest, Basics) {
  EXPECT_FALSE(IsReporterActive());
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  EXPECT_TRUE(IsReporterActive());

  base::HistogramTester histogram_tester;
  FeatureDiscoveryDurationReporterImpl* reporter =
      GetFeatureDiscoveryDurationReporter();
  reporter->MaybeActivateObservation(
      feature_discovery::TrackableFeature::kModeSeparateMockFeature);
  constexpr base::TimeDelta delta(base::Minutes(1));
  task_environment()->FastForwardBy(delta);
  reporter->MaybeFinishObservation(
      feature_discovery::TrackableFeature::kModeSeparateMockFeature);

  // Verify that data is recorded with the correct histogram.
  if (GetParam()) {
    histogram_tester.ExpectUniqueTimeSample(kMockFeatureTabletHistogram, delta,
                                            1);
    histogram_tester.ExpectTotalCount(kMockFeatureClamshellHistogram, 0);
  } else {
    histogram_tester.ExpectUniqueTimeSample(kMockFeatureClamshellHistogram,
                                            delta, 1);
    histogram_tester.ExpectTotalCount(kMockFeatureTabletHistogram, 0);
  }
}

// Verifies that the metric data is correctly reported when the tablet mode
// switches before the end of observation.
TEST_P(FeatureDiscoveryDurationReporterDataSplitTest,
       SwitchModeBeforeMetricReport) {
  EXPECT_FALSE(IsReporterActive());
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  EXPECT_TRUE(IsReporterActive());

  base::HistogramTester histogram_tester;
  FeatureDiscoveryDurationReporterImpl* reporter =
      GetFeatureDiscoveryDurationReporter();
  reporter->MaybeActivateObservation(
      feature_discovery::TrackableFeature::kModeSeparateMockFeature);
  constexpr base::TimeDelta delta1(base::Minutes(1));
  task_environment()->FastForwardBy(delta1);

  // Toggle the tablet mode. Wait for the time duration of `delta2`. Then finish
  // the observation.
  TabletMode::Get()->SetEnabledForTest(!GetParam());
  constexpr base::TimeDelta delta2(base::Minutes(2));
  task_environment()->FastForwardBy(delta2);
  reporter->MaybeFinishObservation(
      feature_discovery::TrackableFeature::kModeSeparateMockFeature);

  // Verify that data is recorded with the correct histogram.
  if (GetParam()) {
    histogram_tester.ExpectUniqueTimeSample(kMockFeatureTabletHistogram,
                                            delta1 + delta2, 1);
    histogram_tester.ExpectTotalCount(kMockFeatureClamshellHistogram, 0);
  } else {
    histogram_tester.ExpectUniqueTimeSample(kMockFeatureClamshellHistogram,
                                            delta1 + delta2, 1);
    histogram_tester.ExpectTotalCount(kMockFeatureTabletHistogram, 0);
  }
}

// Verifies the metric data that is not split by tablet mode should be recorded
// under both clamshell and tablet.
TEST_P(FeatureDiscoveryDurationReporterDataSplitTest, CheckUnsplitMetric) {
  EXPECT_FALSE(IsReporterActive());
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  EXPECT_TRUE(IsReporterActive());

  base::HistogramTester histogram_tester;
  FeatureDiscoveryDurationReporterImpl* reporter =
      GetFeatureDiscoveryDurationReporter();
  reporter->MaybeActivateObservation(
      feature_discovery::TrackableFeature::kMockFeature);
  constexpr base::TimeDelta delta(base::Minutes(1));
  task_environment()->FastForwardBy(delta);
  reporter->MaybeFinishObservation(
      feature_discovery::TrackableFeature::kMockFeature);
  histogram_tester.ExpectUniqueTimeSample(kMockHistogram, delta, 1);
}

}  // namespace ash
