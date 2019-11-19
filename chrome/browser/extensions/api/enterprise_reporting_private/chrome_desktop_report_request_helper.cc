// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/chrome_desktop_report_request_helper.h"

#include <string>

#include "base/base_paths.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/enterprise_reporting_private/prefs.h"
#include "chrome/browser/policy/browser_dm_token_storage.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/policy_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"

namespace em = enterprise_management;

namespace extensions {
namespace {

// JSON keys in the extension arguments.
const char kBrowserReport[] = "browserReport";
const char kChromeUserProfileReport[] = "chromeUserProfileReport";
const char kChromeSignInUser[] = "chromeSignInUser";
const char kExtensionData[] = "extensionData";
const char kPlugins[] = "plugins";
const char kSafeBrowsingWarnings[] = "safeBrowsingWarnings";
const char kSafeBrowsingWarningsClickThrough[] =
    "safeBrowsingWarningsClickThrough";

// JSON keys in the os_info field.
const char kOS[] = "os";
const char kOSArch[] = "arch";
const char kOSVersion[] = "os_version";

const char kDefaultDictionary[] = "{}";
const char kDefaultList[] = "[]";

enum Type {
  LIST,
  DICTIONARY,
};

std::string GetChromePath() {
  base::FilePath path;
  base::PathService::Get(base::DIR_EXE, &path);
  return path.AsUTF8Unsafe();
}

std::string GetProfileId(const Profile* profile) {
  return profile->GetOriginalProfile()->GetPath().AsUTF8Unsafe();
}

// Returns last policy fetch timestamp of machine level user cloud policy if
// it exists. Otherwise, returns zero.
int64_t GetMachineLevelUserCloudPolicyFetchTimestamp() {
  policy::MachineLevelUserCloudPolicyManager* manager =
      g_browser_process->browser_policy_connector()
          ->machine_level_user_cloud_policy_manager();
  if (!manager || !manager->IsClientRegistered())
    return 0;
  return manager->core()->client()->last_policy_timestamp().ToJavaTime();
}

void AppendAdditionalBrowserInformation(em::ChromeDesktopReportRequest* request,
                                        Profile* profile) {
  const PrefService* prefs = profile->GetPrefs();

  // Set Chrome version number
  request->mutable_browser_report()->set_browser_version(
      version_info::GetVersionNumber());
  // Set Chrome channel
  request->mutable_browser_report()->set_channel(
      policy::ConvertToProtoChannel(chrome::GetChannel()));

  // Add a new profile report if extension doesn't report any profile.
  if (request->browser_report().chrome_user_profile_reports_size() == 0)
    request->mutable_browser_report()->add_chrome_user_profile_reports();

  DCHECK_EQ(1, request->browser_report().chrome_user_profile_reports_size());

  // Set Chrome executable path
  request->mutable_browser_report()->set_executable_path(GetChromePath());

  // Set profile ID for the first profile.
  request->mutable_browser_report()
      ->mutable_chrome_user_profile_reports(0)
      ->set_id(GetProfileId(profile));

  // Set the profile name
  request->mutable_browser_report()
      ->mutable_chrome_user_profile_reports(0)
      ->set_name(prefs->GetString(prefs::kProfileName));

  if (prefs->GetBoolean(enterprise_reporting::kReportPolicyData)) {
    // Set policy data of the first profile. Extension will report this data in
    // the future.
    request->mutable_browser_report()
        ->mutable_chrome_user_profile_reports(0)
        ->set_policy_data(policy::DictionaryPolicyConversions()
                              .WithBrowserContext(profile)
                              .EnablePrettyPrint(false)
                              .ToJSON());

    int64_t timestamp = GetMachineLevelUserCloudPolicyFetchTimestamp();
    if (timestamp > 0) {
      request->mutable_browser_report()
          ->mutable_chrome_user_profile_reports(0)
          ->set_policy_fetched_timestamp(timestamp);
    }
  }
}

bool UpdateJSONEncodedStringEntry(const base::Value& dict_value,
                                  const char key[],
                                  std::string* entry,
                                  const Type type) {
  if (const base::Value* value = dict_value.FindKey(key)) {
    if ((type == DICTIONARY && !value->is_dict()) ||
        (type == LIST && !value->is_list())) {
      return false;
    }
    base::JSONWriter::Write(*value, entry);
  } else {
    if (type == DICTIONARY)
      *entry = kDefaultDictionary;
    else if (type == LIST)
      *entry = kDefaultList;
  }

  return true;
}

void AppendPlatformInformation(em::ChromeDesktopReportRequest* request,
                               const PrefService* prefs) {
  base::Value os_info = base::Value(base::Value::Type::DICTIONARY);
  os_info.SetKey(kOS, base::Value(policy::GetOSPlatform()));
  os_info.SetKey(kOSVersion, base::Value(policy::GetOSVersion()));
  os_info.SetKey(kOSArch, base::Value(policy::GetOSArchitecture()));
  base::JSONWriter::Write(os_info, request->mutable_os_info());

  const char kComputerName[] = "computername";
  base::Value machine_name = base::Value(base::Value::Type::DICTIONARY);
  machine_name.SetKey(kComputerName, base::Value(policy::GetMachineName()));
  base::JSONWriter::Write(machine_name, request->mutable_machine_name());

  const char kUsername[] = "username";
  base::Value os_user = base::Value(base::Value::Type::DICTIONARY);
  os_user.SetKey(kUsername, base::Value(policy::GetOSUsername()));
  base::JSONWriter::Write(os_user, request->mutable_os_user());

#if defined(OS_WIN)
  request->set_serial_number(
      policy::BrowserDMTokenStorage::Get()->RetrieveSerialNumber());
#endif
}

std::unique_ptr<em::ChromeUserProfileReport>
GenerateChromeUserProfileReportRequest(const base::Value& profile_report,
                                       const PrefService* prefs) {
  if (!profile_report.is_dict())
    return nullptr;

  std::unique_ptr<em::ChromeUserProfileReport> request =
      std::make_unique<em::ChromeUserProfileReport>();

  if (!UpdateJSONEncodedStringEntry(profile_report, kChromeSignInUser,
                                    request->mutable_chrome_signed_in_user(),
                                    DICTIONARY)) {
    return nullptr;
  }

  if (prefs->GetBoolean(
          enterprise_reporting::kReportExtensionsAndPluginsData)) {
    if (!UpdateJSONEncodedStringEntry(profile_report, kExtensionData,
                                      request->mutable_extension_data(),
                                      LIST) ||
        !UpdateJSONEncodedStringEntry(profile_report, kPlugins,
                                      request->mutable_plugins(), LIST)) {
      return nullptr;
    }
  }

  if (prefs->GetBoolean(enterprise_reporting::kReportSafeBrowsingData)) {
    if (const base::Value* count =
            profile_report.FindKey(kSafeBrowsingWarnings)) {
      if (!count->is_int())
        return nullptr;
      request->set_safe_browsing_warnings(count->GetInt());
    }

    if (const base::Value* count =
            profile_report.FindKey(kSafeBrowsingWarningsClickThrough)) {
      if (!count->is_int())
        return nullptr;
      request->set_safe_browsing_warnings_click_through(count->GetInt());
    }
  }

  return request;
}

}  // namespace

std::unique_ptr<em::ChromeDesktopReportRequest>
GenerateChromeDesktopReportRequest(const base::DictionaryValue& report,
                                   Profile* profile) {
  std::unique_ptr<em::ChromeDesktopReportRequest> request =
      std::make_unique<em::ChromeDesktopReportRequest>();

  const PrefService* prefs = profile->GetPrefs();

  AppendPlatformInformation(request.get(), prefs);

  if (const base::Value* browser_report =
          report.FindKeyOfType(kBrowserReport, base::Value::Type::DICTIONARY)) {
    if (const base::Value* profile_reports = browser_report->FindKeyOfType(
            kChromeUserProfileReport, base::Value::Type::LIST)) {
      if (!profile_reports->GetList().empty()) {
        DCHECK_EQ(1u, profile_reports->GetList().size());
        // Currently, profile send their browser reports individually.
        std::unique_ptr<em::ChromeUserProfileReport> profile_report_request =
            GenerateChromeUserProfileReportRequest(
                profile_reports->GetList()[0], prefs);
        if (!profile_report_request)
          return nullptr;
        request->mutable_browser_report()
            ->mutable_chrome_user_profile_reports()
            ->AddAllocated(profile_report_request.release());
      }
    }
  }

  AppendAdditionalBrowserInformation(request.get(), profile);

  return request;
}

}  // namespace extensions
