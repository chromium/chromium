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

namespace safe_browsing {

class SRTDownloadURLTest : public ::testing::Test {
 protected:
  void CreateDownloadFeature(const std::string& download_group_name) {
    base::FieldTrialParams params;
    params["cleaner_download_group"] = download_group_name;
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kChromeCleanupDistributionFeature, params);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SRTDownloadURLTest, Experiment) {
  CreateDownloadFeature("experiment");
  std::string expected_path;
  if (base::win::OSInfo::GetArchitecture() ==
      base::win::OSInfo::X86_ARCHITECTURE) {
    expected_path =
        "/dl/softwareremovaltool/win/x86/experiment/chrome_cleanup_tool.exe";
  } else {
    expected_path =
        "/dl/softwareremovaltool/win/x64/experiment/chrome_cleanup_tool.exe";
  }
  EXPECT_EQ(expected_path, GetSRTDownloadURL().path());
}

TEST_F(SRTDownloadURLTest, DefaultsToStable) {
  std::string expected_path;
  if (base::win::OSInfo::GetArchitecture() ==
      base::win::OSInfo::X86_ARCHITECTURE) {
    expected_path =
        "/dl/softwareremovaltool/win/x86/stable/chrome_cleanup_tool.exe";
  } else {
    expected_path =
        "/dl/softwareremovaltool/win/x64/stable/chrome_cleanup_tool.exe";
  }
  EXPECT_EQ(expected_path, GetSRTDownloadURL().path());
}

}  // namespace safe_browsing
