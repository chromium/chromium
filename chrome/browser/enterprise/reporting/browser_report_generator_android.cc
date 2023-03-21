// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/browser_report_generator_android.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/channel_info.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/version_info/version_info.h"

namespace em = ::enterprise_management;

namespace enterprise_reporting {

BrowserReportGeneratorAndroid::BrowserReportGeneratorAndroid() = default;

BrowserReportGeneratorAndroid::~BrowserReportGeneratorAndroid() = default;

std::string BrowserReportGeneratorAndroid::GetExecutablePath() {
  base::FilePath path;
  return base::PathService::Get(base::DIR_EXE, &path) ? path.AsUTF8Unsafe()
                                                      : std::string();
}

version_info::Channel BrowserReportGeneratorAndroid::GetChannel() {
  return chrome::GetChannel();
}

std::vector<BrowserReportGenerator::ReportedProfileData>
BrowserReportGeneratorAndroid::GetReportedProfiles() {
  std::vector<BrowserReportGenerator::ReportedProfileData> reportedProfileData;
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  for (const auto* entry : profile_manager->GetProfileAttributesStorage()
                               .GetAllProfilesAttributes()) {
    reportedProfileData.push_back(
        {entry->GetPath().AsUTF8Unsafe(), base::UTF16ToUTF8(entry->GetName())});
  }

  return reportedProfileData;
}

bool BrowserReportGeneratorAndroid::IsExtendedStableChannel() {
  return chrome::IsExtendedStableChannel();
}

void BrowserReportGeneratorAndroid::GenerateBuildStateInfo(
    em::BrowserReport* report) {
  // Not used on Android because there is no in-app auto-update.
}

}  // namespace enterprise_reporting
