// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/active_sampling.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/common/privacy_budget/privacy_budget_settings_provider.h"
#include "chrome/common/privacy_budget/scoped_privacy_budget_config.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"

namespace {

class ScopedIdentifiabilityStudySettings {
 public:
  ScopedIdentifiabilityStudySettings() {
    // Reload the config in the global study settings.
    blink::IdentifiabilityStudySettings::SetGlobalProvider(
        std::make_unique<PrivacyBudgetSettingsProvider>());
  }

  ~ScopedIdentifiabilityStudySettings() {
    blink::IdentifiabilityStudySettings::ResetStateForTesting();
  }
};

}  // namespace

class PrivacyBudgetActiveSamplingTest : public ::testing::Test {
 public:
  ukm::TestAutoSetUkmRecorder& ukm_recorder() { return ukm_recorder_; }

 private:
  base::test::TaskEnvironment task_environment_;
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
};

TEST_F(PrivacyBudgetActiveSamplingTest, ActivelySampledSurfaces) {
  test::ScopedPrivacyBudgetConfig::Parameters parameters;
  parameters.enable_active_sampling = true;
  test::ScopedPrivacyBudgetConfig config(parameters);
  ScopedIdentifiabilityStudySettings scoped_settings;

  base::RunLoop run_loop;
  ukm_recorder().SetOnAddEntryCallback(
      ukm::builders::Identifiability::kEntryName, run_loop.QuitClosure());
  ActivelySampleIdentifiableSurfaces();

  // Wait for the metrics to come down the pipe.
  run_loop.Run();

  auto merged_entries = ukm_recorder().GetMergedEntriesByName(
      ukm::builders::Identifiability::kEntryName);

  std::vector<uint64_t> reported_surface_keys;
  for (const auto& entry : merged_entries) {
    for (const auto& metric : entry.second->metrics) {
      reported_surface_keys.push_back(metric.first);
    }
  }
  EXPECT_EQ(reported_surface_keys,
            std::vector<uint64_t>({18009598079355128088u}));
}
