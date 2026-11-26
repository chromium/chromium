// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/browser_report_generator_desktop.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/upgrade_detector/build_state.h"
#include "chrome/common/channel_info.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/version_info/version_info.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/enterprise/signals/app_bound_encryption_state_win.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/common/signals_features.h"
#include "components/enterprise/browser/reporting/report_util.h"
#endif  // BUILDFLAG(IS_WIN)

namespace em = ::enterprise_management;

namespace enterprise_reporting {

BrowserReportGeneratorDesktop::BrowserReportGeneratorDesktop() = default;

BrowserReportGeneratorDesktop::~BrowserReportGeneratorDesktop() = default;

std::string BrowserReportGeneratorDesktop::GetExecutablePath() {
  base::FilePath path;
  return base::PathService::Get(base::DIR_EXE, &path) ? path.AsUTF8Unsafe()
                                                      : std::string();
}

version_info::Channel BrowserReportGeneratorDesktop::GetChannel() {
  return chrome::GetChannel();
}

std::vector<BrowserReportGenerator::ReportedProfileData>
BrowserReportGeneratorDesktop::GetReportedProfiles() {
  std::vector<BrowserReportGenerator::ReportedProfileData> reportedProfileData;

  for (const auto* entry : g_browser_process->profile_manager()
                               ->GetProfileAttributesStorage()
                               .GetAllProfilesAttributes()) {
#if BUILDFLAG(IS_CHROMEOS)
    // Skip sign-in and lock screen app profile on Chrome OS.
    if (!ash::ProfileHelper::IsUserProfilePath(entry->GetPath().BaseName())) {
      continue;
    }
#endif  // BUILDFLAG(IS_CHROMEOS)

    base::FilePath profile_path = entry->GetPath();

    reportedProfileData.push_back(
        {profile_path.AsUTF8Unsafe(), base::UTF16ToUTF8(entry->GetName())});
  }

  return reportedProfileData;
}

bool BrowserReportGeneratorDesktop::IsExtendedStableChannel() {
  return chrome::IsExtendedStableChannel();
}

void BrowserReportGeneratorDesktop::GenerateBuildStateInfo(
    em::BrowserReport* report) {
  const auto* const build_state = g_browser_process->GetBuildState();
  if (build_state->update_type() != BuildState::UpdateType::kNone) {
    const auto& installed_version = build_state->installed_version();
    if (installed_version) {
      report->set_installed_browser_version(installed_version->GetString());
    }
  }
}

#if BUILDFLAG(IS_WIN)
void BrowserReportGeneratorDesktop::GenerateAppBoundEncryptionInfo(
    std::unique_ptr<em::BrowserReport> report,
    BrowserReportGenerator::ReportCallback callback) {
  if (enterprise_signals::features::IsReportAppBoundEncryptionStateEnabled()) {
    device_signals::GetAppBoundEncryptionState(
        g_browser_process->os_crypt_async(), g_browser_process->local_state(),
        base::BindOnce(
            [](std::unique_ptr<em::BrowserReport> report,
               BrowserReportGenerator::ReportCallback callback,
               device_signals::AppBoundEncryptionState state) {
              report->set_app_bound_encryption_state(
                  TranslateAppBoundEncryptionState(state));
              std::move(callback).Run(std::move(report));
            },
            std::move(report), std::move(callback)));
    return;
  }
  std::move(callback).Run(std::move(report));
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace enterprise_reporting
