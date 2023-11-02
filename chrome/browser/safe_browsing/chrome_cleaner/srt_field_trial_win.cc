// Copyright 2015 The Chromium Authors
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
#include "components/component_updater/pref_names.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

// The download links of the Software Removal Tool.
constexpr char kDownloadRootPath[] =
    "https://dl.google.com/dl/softwareremovaltool/win/";

}  // namespace

namespace safe_browsing {

// Do not remove this feature, even though it's enabled by default. It's used
// in testing to override `kReporterDistributionTagParam` and
// `kCleanerDownloadGroupParam` to fetch pre-release versions of the tool.
BASE_FEATURE(kChromeCleanupDistributionFeature,
             "ChromeCleanupDistribution",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kReporterDistributionTagParam{
    &kChromeCleanupDistributionFeature, "reporter_omaha_tag", ""};

const base::FeatureParam<std::string> kCleanerDownloadGroupParam{
    &kChromeCleanupDistributionFeature, "cleaner_download_group", ""};

GURL GetSRTDownloadURL(PrefService* prefs) {
  std::string download_group = kCleanerDownloadGroupParam.Get();
  if (download_group.empty()) {
    // Use the same string from prefs that was used for the reporter tag. Since
    // the tag and download group aren't coming from
    // kChromeCleanupDistributionFeature, the pref will always be set to a valid
    // value by SwReporterInstallerPolicy when the reporter was fetched.
    DCHECK(kReporterDistributionTagParam.Get().empty());
    download_group = prefs->GetString(prefs::kSwReporterCohort);
    DCHECK(download_group == "canary" || download_group == "stable");
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
