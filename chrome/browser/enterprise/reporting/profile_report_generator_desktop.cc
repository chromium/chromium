// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/profile_report_generator_desktop.h"

#include <utility>

#include "base/json/values_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/reporting/extension_info.h"
#include "chrome/browser/enterprise/reporting/extension_request/extension_request_report_generator.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/enterprise/browser/reporting/policy_info.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "extensions/common/extension_urls.h"

namespace em = enterprise_management;

namespace enterprise_reporting {
namespace {

const int kMaxNumberOfExtensionRequest = 1000;

}  // namespace

ProfileReportGeneratorDesktop::ProfileReportGeneratorDesktop() = default;

ProfileReportGeneratorDesktop::~ProfileReportGeneratorDesktop() = default;

void ProfileReportGeneratorDesktop::GetExtensionInfo(
    enterprise_management::ChromeUserProfileInfo* report) {
  AppendExtensionInfoIntoProfileReport(profile_, report);
}

void ProfileReportGeneratorDesktop::GetExtensionRequest(
    enterprise_management::ChromeUserProfileInfo* report) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (!profile_->GetPrefs()->GetBoolean(prefs::kCloudExtensionRequestEnabled))
    return;
  const base::Value::Dict& pending_requests =
      profile_->GetPrefs()->GetDict(prefs::kCloudExtensionRequestIds);

  // In case a corrupted profile prefs causing |pending_requests| to be null.

  extensions::ExtensionManagement* extension_management =
      extensions::ExtensionManagementFactory::GetForBrowserContext(profile_);
  std::string webstore_update_url =
      extension_urls::GetDefaultWebstoreUpdateUrl().spec();

  int number_of_requests = 0;
  for (auto [extension_id, request_data] : pending_requests) {
    if (!ExtensionRequestReportGenerator::ShouldUploadExtensionRequest(
            extension_id, webstore_update_url, extension_management)) {
      continue;
    }

    // Use a hard limitation to prevent users adding too many requests. 1000
    // requests should use less than 50 kb report space.
    number_of_requests += 1;
    if (number_of_requests > kMaxNumberOfExtensionRequest)
      break;

    auto* request = report->add_extension_requests();
    request->set_id(extension_id);

    const auto& request_data_dict = request_data.GetDict();
    std::optional<base::Time> timestamp = ::base::ValueToTime(
        request_data_dict.Find(extension_misc::kExtensionRequestTimestamp));
    if (timestamp)
      request->set_request_timestamp(timestamp->InMillisecondsSinceUnixEpoch());

    const std::string* justification = request_data_dict.FindString(
        extension_misc::kExtensionWorkflowJustification);
    if (justification) {
      request->set_justification(*justification);
    }
  }
#endif
}

}  // namespace enterprise_reporting
