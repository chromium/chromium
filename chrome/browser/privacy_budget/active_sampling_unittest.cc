// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/active_sampling.h"

#include "base/barrier_closure.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/common/privacy_budget/privacy_budget_settings_provider.h"
#include "chrome/common/privacy_budget/scoped_privacy_budget_config.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
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
  parameters.actively_sampled_fonts = {"Arial", "Helvetica"};
  test::ScopedPrivacyBudgetConfig config(parameters);
  ScopedIdentifiabilityStudySettings scoped_settings;

  base::RunLoop run_loop;
  ukm_recorder().SetOnAddEntryCallback(
      ukm::builders::Identifiability::kEntryName,
      BarrierClosure(2u, run_loop.QuitClosure()));
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
  EXPECT_THAT(reported_surface_keys,
              testing::IsSupersetOf({
                  18009598079355128088u,  // model
                  9223784233214641190u,   // Arial
                  10735872651981970214u,  // Helvetica
              }));
}
