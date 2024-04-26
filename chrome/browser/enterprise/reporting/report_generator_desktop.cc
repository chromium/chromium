// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/report_generator_desktop.h"

#include <utility>

#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/reporting/android_app_info_generator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace em = enterprise_management;

namespace enterprise_reporting {

// TODO(crbug.com/40703888): Split up Chrome OS reporting code into its own
// delegates, then move this method's implementation to ReportGeneratorChromeOS.
void ReportGeneratorDesktop::SetAndroidAppInfos(ReportRequest* basic_request) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  DCHECK(basic_request);
  basic_request->GetDeviceReportRequest().clear_android_app_infos();

  // Android application is only supported for primary profile.
  Profile* primary_profile =
      g_browser_process->profile_manager()->GetPrimaryUserProfile();

  if (!arc::IsArcPlayStoreEnabledForProfile(primary_profile))
    return;

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(primary_profile);

  if (!prefs) {
    LOG(ERROR) << base::StringPrintf(
        "Failed to generate ArcAppListPrefs instance for primary user profile "
        "(debug name: %s).",
        primary_profile->GetDebugName().c_str());
    return;
  }

  AndroidAppInfoGenerator generator;
  for (std::string app_id : prefs->GetAppIds()) {
    basic_request->GetDeviceReportRequest()
        .mutable_android_app_infos()
        ->AddAllocated(generator.Generate(prefs, app_id).release());
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace enterprise_reporting
