// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"

#include <map>
#include <string>

#include "base/metrics/field_trial.h"
#include "base/test/scoped_feature_list.h"
#include "base/win/windows_version.h"
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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SRTDownloadURLTest, Experiment) {
  CreateDownloadFeature("experiment");
  const std::string expected_path =
      (base::win::OSInfo::GetArchitecture() ==
       base::win::OSInfo::X86_ARCHITECTURE)
          ? "/dl/softwareremovaltool/win/x86/experiment/chrome_cleanup_tool.exe"
          : "/dl/softwareremovaltool/win/x64/experiment/"
            "chrome_cleanup_tool.exe";
  EXPECT_EQ(expected_path, GetSRTDownloadURL().path());
}

TEST_F(SRTDownloadURLTest, DefaultsToStable) {
  DisableDownloadFeature();
  EXPECT_EQ(GetStablePath(), GetSRTDownloadURL().path());
}

TEST_F(SRTDownloadURLTest, EmptyParamIsStable) {
  CreateDownloadFeature("");
  EXPECT_EQ(GetStablePath(), GetSRTDownloadURL().path());
}

TEST_F(SRTDownloadURLTest, MissingParamIsStable) {
  CreateDownloadFeature(absl::nullopt);
  EXPECT_EQ(GetStablePath(), GetSRTDownloadURL().path());
}

}  // namespace

}  // namespace safe_browsing
