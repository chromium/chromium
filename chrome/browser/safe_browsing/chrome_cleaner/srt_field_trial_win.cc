// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"

#include <string>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/win/windows_version.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

// The download links of the Software Removal Tool.
constexpr char kDownloadRootPath[] =
    "https://dl.google.com/dl/softwareremovaltool/win/";

}  // namespace

namespace safe_browsing {

const base::Feature kChromeCleanupDistributionFeature{
    "ChromeCleanupDistribution", base::FEATURE_ENABLED_BY_DEFAULT};

const base::FeatureParam<std::string> kReporterDistributionTagParam{
    &kChromeCleanupDistributionFeature, "reporter_omaha_tag", ""};

const base::FeatureParam<std::string> kCleanerDownloadGroupParam{
    &kChromeCleanupDistributionFeature, "cleaner_download_group", ""};

GURL GetSRTDownloadURL() {
  std::string download_group = kCleanerDownloadGroupParam.Get();
  if (download_group.empty()) {
    // TODO(crbug.com/1305048): If the download group isn't assigned by the
    // server, randomly assign the user to canary or stable.
    download_group = "stable";
  }
  const std::string architecture = base::win::OSInfo::GetArchitecture() ==
                                           base::win::OSInfo::X86_ARCHITECTURE
                                       ? "x86"
                                       : "x64";

  // Construct download URL using the following pattern:
  // https://dl.google.com/.../win/{arch}/{group}/chrome_cleanup_tool.exe
  GURL download_url(base::StrCat({kDownloadRootPath, architecture, "/",
                                  download_group, "/chrome_cleanup_tool.exe"}));

  // Ensure URL construction didn't change origin. The only part of the url
  // that doesn't come from a fixed list is `download_group`, which comes from
  // a trusted server, so this should be impossible.
  CHECK(url::IsSameOriginWith(download_url, GURL(kDownloadRootPath)));

  return download_url;
}

void RecordPromptNotShownWithReasonHistogram(
    NoPromptReasonHistogramValue value) {
  UMA_HISTOGRAM_ENUMERATION("SoftwareReporter.NoPromptReason", value,
                            NO_PROMPT_REASON_MAX);
}

}  // namespace safe_browsing
