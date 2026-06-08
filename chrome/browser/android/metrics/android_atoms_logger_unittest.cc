// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/metrics/android_atoms_logger.h"

#include "base/containers/fixed_flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome::android::westworld {

namespace {

struct LoggedAtom {
  int atom_id;
  MetricType type;
  base::HistogramBase::Sample32 sample;
};

class TestAndroidAtomsLogger : public AndroidAtomsLogger {
 public:
  explicit TestAndroidAtomsLogger(base::span<const HistogramInfo> allowlist)
      : AndroidAtomsLogger(allowlist) {}

  ~TestAndroidAtomsLogger() override = default;

  void LogAtom(int atom_id,
               MetricType type,
               base::HistogramBase::Sample32 sample) override {
    logged_atoms_.push_back({atom_id, type, sample});
  }

  std::vector<LoggedAtom> logged_atoms_;
};

}  // namespace

class AndroidAtomsLoggerTest : public testing::Test {
 public:
  AndroidAtomsLoggerTest() = default;
  ~AndroidAtomsLoggerTest() override = default;

 protected:
  static constexpr HistogramInfo kTestAllowlist[] = {
      {"TEST_HISTOGRAM", 12345, MetricType::kInt},
  };

  void SetUp() override {
    statistics_recorder_ =
        base::StatisticsRecorder::CreateTemporaryForTesting();

    TestingPrefServiceSimple* local_state =
        TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
    DCHECK(local_state);
    if (!local_state->FindPreference(
            metrics::prefs::kMetricsReportingEnabled)) {
      local_state->registry()->RegisterBooleanPref(
          metrics::prefs::kMetricsReportingEnabled, false);
    }
  }

  void TearDown() override { statistics_recorder_.reset(); }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<base::StatisticsRecorder> statistics_recorder_;
};

TEST_F(AndroidAtomsLoggerTest, FeatureDisabled_DoesNotInitialize) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kAndroidAtomsLogging);

  TestAndroidAtomsLogger logger(kTestAllowlist);

  EXPECT_TRUE(logger.observers_.empty());
}

TEST_F(AndroidAtomsLoggerTest, FeatureEnabled_InitializesOnDesktop) {
  if (!AndroidAtomsLogger::IsDesktop()) {
    // Unit test is not built for Desktop. Skipping this unit test.
    return;
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kAndroidAtomsLogging);

  TestAndroidAtomsLogger logger(kTestAllowlist);

  EXPECT_EQ(logger.observers_.size(), std::size(kTestAllowlist));
}

TEST_F(AndroidAtomsLoggerTest, FeatureEnabled_DoesNotInitializeOnNonDesktop) {
  if (AndroidAtomsLogger::IsDesktop()) {
    // Unit test is built for Desktop. Skipping this unit test.
    return;
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kAndroidAtomsLogging);

  TestAndroidAtomsLogger logger(kTestAllowlist);

  EXPECT_TRUE(logger.observers_.empty());
}

TEST_F(AndroidAtomsLoggerTest, LogAtomGuardedByMetricsConsentOnDesktop) {
  if (!AndroidAtomsLogger::IsDesktop()) {
    // Unit test is not built for Android Desktop. Skipping this unit test.
    return;
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      chrome::android::kAndroidAtomsLogging);

  TestAndroidAtomsLogger logger(kTestAllowlist);

  auto* local_state =
      TestingBrowserProcess::GetGlobal()->GetTestingLocalState();
  local_state->SetBoolean(metrics::prefs::kMetricsReportingEnabled, true);
  // Trigger a sample for a histogram in the allowlist.
  base::UmaHistogramCounts100("TEST_HISTOGRAM", 5);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(logger.logged_atoms_.size(), 1u);
  EXPECT_EQ(logger.logged_atoms_[0].atom_id, 12345);
  EXPECT_EQ(logger.logged_atoms_[0].sample, 5);

  logger.logged_atoms_.clear();

  // Disable metrics reporting.
  local_state->SetBoolean(metrics::prefs::kMetricsReportingEnabled, false);
  // Trigger another sample.
  base::UmaHistogramCounts100("TEST_HISTOGRAM", 10);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(logger.logged_atoms_.size(), 0u);
}

}  // namespace chrome::android::westworld
