// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/experiment/experiment_manager.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/tpcd/experiment/tpcd_pref_names.h"
#include "chrome/browser/tpcd/experiment/tpcd_utils.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/common/content_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tpcd::experiment {

namespace {

class TestingExperimentManager : public ExperimentManager {};

}  // namespace

class ExperimentManagerTest : public testing::Test {
 public:
  ExperimentManagerTest() : local_state_(TestingBrowserProcess::GetGlobal()) {}

  PrefService& prefs() { return *local_state_.Get(); }

 protected:
  ScopedTestingLocalState local_state_;
};

TEST_F(ExperimentManagerTest, Version) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kCookieDeprecationFacilitatedTesting, {{"version", "2"}});

  const struct {
    const char* desc;
    absl::optional<int> initial_version;
    absl::optional<utils::ExperimentState> initial_state;
    int expected_version;
    utils::ExperimentState expected_state;
  } kTestCases[] = {
      {
          .desc = "first-run",
          .expected_version = 2,
          .expected_state = utils::ExperimentState::kUnknownEligiblity,
      },
      {
          .desc = "new-version",
          .initial_version = 1,
          .initial_state = utils::ExperimentState::kEligible,
          .expected_version = 2,
          .expected_state = utils::ExperimentState::kUnknownEligiblity,
      },
      {
          .desc = "same-version",
          .initial_version = 2,
          .initial_state = utils::ExperimentState::kEligible,
          .expected_version = 2,
          .expected_state = utils::ExperimentState::kEligible,
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    if (test_case.initial_version.has_value()) {
      prefs().SetInteger(prefs::kTPCDExperimentClientStateVersion,
                         *test_case.initial_version);
    } else {
      prefs().ClearPref(prefs::kTPCDExperimentClientStateVersion);
    }
    if (test_case.initial_state.has_value()) {
      prefs().SetInteger(prefs::kTPCDExperimentClientState,
                         static_cast<int>(*test_case.initial_state));
    } else {
      prefs().ClearPref(prefs::kTPCDExperimentClientState);
    }

    TestingExperimentManager experiment_manager;

    EXPECT_EQ(prefs().GetInteger(prefs::kTPCDExperimentClientStateVersion),
              test_case.expected_version);
    EXPECT_EQ(prefs().GetInteger(prefs::kTPCDExperimentClientState),
              static_cast<int>(test_case.expected_state));
  }
}

}  // namespace tpcd::experiment
