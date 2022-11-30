// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"

#include <map>
#include <string>

#include "base/metrics/field_trial.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "base/win/windows_version.h"
#include "components/component_updater/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace safe_browsing {

namespace {

// Returns the expected path for the "stable" group on the current architecture.
std::string GetStablePath() {
  if (base::win::OSInfo::GetArchitecture() ==
      base::win::OSInfo::X86_ARCHITECTURE) {
    return "/dl/softwareremovaltool/win/x86/stable/chrome_cleanup_tool.exe";
  } else {
    return "/dl/softwareremovaltool/win/x64/stable/chrome_cleanup_tool.exe";
  }
}

class SRTDownloadURLTest : public ::testing::Test {
 protected:
  SRTDownloadURLTest() {
    test_prefs_.registry()->RegisterStringPref(prefs::kSwReporterCohort,
                                               "stable");
  }

  void CreateDownloadFeature(
      const absl::optional<std::string>& download_group_name) {
    base::FieldTrialParams params;
    if (download_group_name)
      params["cleaner_download_group"] = *download_group_name;
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kChromeCleanupDistributionFeature, params);
  }

  void DisableDownloadFeature() {
    scoped_feature_list_.InitAndDisableFeature(
        kChromeCleanupDistributionFeature);
  }

 protected:
  TestingPrefServiceSimple test_prefs_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// The feature should override the pref. The string "experiment" should be
// allowed for the feature, even though it's not allowed from a pref.
TEST_F(SRTDownloadURLTest, Experiment) {
  CreateDownloadFeature("experiment");
  const std::string expected_path =
      (base::win::OSInfo::GetArchitecture() ==
       base::win::OSInfo::X86_ARCHITECTURE)
          ? "/dl/softwareremovaltool/win/x86/experiment/chrome_cleanup_tool.exe"
          : "/dl/softwareremovaltool/win/x64/experiment/"
            "chrome_cleanup_tool.exe";
  EXPECT_EQ(expected_path, GetSRTDownloadURL(&test_prefs_).path());
}

// Default to "stable" because the pref is set to that in the test constructor.
TEST_F(SRTDownloadURLTest, DefaultsToStable) {
  DisableDownloadFeature();
  EXPECT_EQ(GetStablePath(), GetSRTDownloadURL(&test_prefs_).path());
}

// Default to "stable" because the pref is set to that in the test constructor.
TEST_F(SRTDownloadURLTest, EmptyParamIsStable) {
  CreateDownloadFeature("");
  EXPECT_EQ(GetStablePath(), GetSRTDownloadURL(&test_prefs_).path());
}

// Default to "stable" because the pref is set to that in the test constructor.
TEST_F(SRTDownloadURLTest, MissingParamIsStable) {
  CreateDownloadFeature(absl::nullopt);
  EXPECT_EQ(GetStablePath(), GetSRTDownloadURL(&test_prefs_).path());
}

// "canary" is also a valid value for the pref.
TEST_F(SRTDownloadURLTest, CanaryFromPrefs) {
  DisableDownloadFeature();
  test_prefs_.SetUserPref(prefs::kSwReporterCohort, base::Value("canary"));
  const std::string expected_path =
      (base::win::OSInfo::GetArchitecture() ==
       base::win::OSInfo::X86_ARCHITECTURE)
          ? "/dl/softwareremovaltool/win/x86/canary/chrome_cleanup_tool.exe"
          : "/dl/softwareremovaltool/win/x64/canary/"
            "chrome_cleanup_tool.exe";
  EXPECT_EQ(expected_path, GetSRTDownloadURL(&test_prefs_).path());
}

}  // namespace

}  // namespace safe_browsing
