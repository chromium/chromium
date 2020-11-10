// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/install_event_log_util.h"

#include <set>

#include "base/hash/md5.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromeos/system/statistics_provider.h"

namespace em = enterprise_management;

namespace policy {

namespace {
// Common Key names used when building the dictionary to pass to the Chrome
// Reporting API.
constexpr char kEventId[] = "eventId";
constexpr char kEventType[] = "eventType";
constexpr char kOnline[] = "online";
constexpr char kSerialNumber[] = "serialNumber";
constexpr char kSessionStateChangeType[] = "sessionStateChangeType";
constexpr char kStatefulTotal[] = "statefulTotal";
constexpr char kStatefulFree[] = "statefulFree";
constexpr char kTime[] = "time";

// Key names used for ARC++ apps when building the dictionary to pass to the
// Chrome Reporting API.
constexpr char kAndroidId[] = "androidId";
constexpr char kAppPackage[] = "appPackage";
constexpr char kCloudDpsResponse[] = "clouddpsResponse";
constexpr char kAndroidAppInstallEvent[] = "androidAppInstallEvent";

// Key names used for extensions when building the dictionary to pass to the
// Chrome Reporting API.
constexpr char kExtensionId[] = "extensionId";
constexpr char kExtensionInstallEvent[] = "extensionAppInstallEvent";
constexpr char kDownloadingStage[] = "downloadingStage";
constexpr char kFailureReason[] = "failureReason";
constexpr char kInstallationStage[] = "installationStage";
constexpr char kExtensionType[] = "extensionType";
constexpr char kUserType[] = "userType";
constexpr char kIsNewUser[] = "isNewUser";
constexpr char kIsMisconfigurationFailure[] = "isMisconfigurationFailure";
constexpr char kInstallCreationStage[] = "installCreationStage";
constexpr char kDownloadCacheStatus[] = "downloadCacheStatus";
constexpr char kUnpackerFailureReason[] = "unpackerFailureReason";
constexpr char kManifestInvalidError[] = "manifestInvalidError";
constexpr char kCrxInstallErrorDetail[] = "crxInstallErrorDetail";
constexpr char kFetchErrorCode[] = "fetchErrorCode";
constexpr char kFetchTries[] = "fetchTries";

// Calculates hash for the given |event| and |context|, and stores the hash in
// |hash|. Returns true if |event| and |context| are json serializable and
// |hash| is not nullptr, otherwise return false.
bool GetHash(const base::Value& event,
             const base::Value& context,
             std::string* hash) {
  if (hash == nullptr)
    return false;

  std::string serialized_string;
  JSONStringValueSerializer serializer(&serialized_string);
  if (!serializer.Serialize(event))
    return false;

  base::MD5Context ctx;
  base::MD5Init(&ctx);
  base::MD5Update(&ctx, serialized_string);

  if (!serializer.Serialize(context))
    return false;
  base::MD5Update(&ctx, serialized_string);

  base::MD5Digest digest;
  base::MD5Final(&digest, &ctx);
  *hash = base::MD5DigestToBase16(digest);
  return true;
}

std::string GetTimeString(const base::Time& timestamp) {
  base::Time::Exploded time_exploded;
  timestamp.UTCExplode(&time_exploded);
  std::string time_str = base::StringPrintf(
      "%d-%02d-%02dT%02d:%02d:%02d.%03dZ", time_exploded.year,
      time_exploded.month, time_exploded.day_of_month, time_exploded.hour,
      time_exploded.minute, time_exploded.second, time_exploded.millisecond);
  return time_str;
}

}  // namespace

std::string GetSerialNumber() {
  return chromeos::system::StatisticsProvider::GetInstance()
      ->GetEnterpriseMachineID();
}

base::Value ConvertExtensionProtoToValue(
    const em::ExtensionInstallReportRequest* extension_install_report_request,
    const base::Value& context) {
  DCHECK(extension_install_report_request);

  base::Value event_list(base::Value::Type::LIST);
  std::set<extensions::ExtensionId> seen_ids;

  for (const em::ExtensionInstallReport& extension_install_report :
       extension_install_report_request->extension_install_reports()) {
    for (const em::ExtensionInstallReportLogEvent&
             extension_install_report_log_event :
         extension_install_report.logs()) {
      base::Value wrapper;
      wrapper = ConvertExtensionEventToValue(
          extension_install_report.has_extension_id()
              ? extension_install_report.extension_id()
              : "",
          extension_install_report_log_event, context);
      auto* id = wrapper.FindStringKey(kEventId);
      if (id) {
        if (seen_ids.find(*id) != seen_ids.end()) {
          LOG(WARNING) << "Skipping duplicate event (" << *id
                       << "): " << wrapper;
          continue;
        }
        seen_ids.insert(*id);
      }
      event_list.Append(std::move(wrapper));
    }
  }

  return event_list;
}

base::Value ConvertExtensionEventToValue(
    const extensions::ExtensionId& extension_id,
    const em::ExtensionInstallReportLogEvent&
        extension_install_report_log_event,
    const base::Value& context) {
  base::Value event(base::Value::Type::DICTIONARY);
  if (!extension_id.empty())
    event.SetStringKey(kExtensionId, extension_id);

  if (extension_install_report_log_event.has_event_type()) {
    event.SetIntKey(kEventType,
                    extension_install_report_log_event.event_type());
  }

  if (extension_install_report_log_event.has_stateful_total()) {
    // 64-bit ints aren't supported by JSON - must be stored as strings
    std::ostringstream str;
    str << extension_install_report_log_event.stateful_total();
    event.SetStringKey(kStatefulTotal, str.str());
  }

  if (extension_install_report_log_event.has_stateful_free()) {
    // 64-bit ints aren't supported by JSON - must be stored as strings
    std::ostringstream str;
    str << extension_install_report_log_event.stateful_free();
    event.SetStringKey(kStatefulFree, str.str());
  }

  if (extension_install_report_log_event.has_online())
    event.SetBoolKey(kOnline, extension_install_report_log_event.online());

  if (extension_install_report_log_event.has_session_state_change_type()) {
    event.SetIntKey(
        kSessionStateChangeType,
        extension_install_report_log_event.session_state_change_type());
  }

  if (extension_install_report_log_event.has_downloading_stage()) {
    event.SetIntKey(kDownloadingStage,
                    extension_install_report_log_event.downloading_stage());
  }

  event.SetStringKey(kSerialNumber, GetSerialNumber());

  if (extension_install_report_log_event.has_failure_reason()) {
    event.SetIntKey(kFailureReason,
                    extension_install_report_log_event.failure_reason());
  }

  if (extension_install_report_log_event.has_user_type()) {
    event.SetIntKey(kUserType, extension_install_report_log_event.user_type());
    DCHECK(extension_install_report_log_event.has_is_new_user());
    event.SetBoolKey(kIsNewUser,
                     extension_install_report_log_event.is_new_user());
  }

  if (extension_install_report_log_event.has_installation_stage()) {
    event.SetIntKey(kInstallationStage,
                    extension_install_report_log_event.installation_stage());
  }

  if (extension_install_report_log_event.has_extension_type()) {
    event.SetIntKey(kExtensionType,
                    extension_install_report_log_event.extension_type());
  }

  if (extension_install_report_log_event.has_is_misconfiguration_failure()) {
    event.SetBoolKey(
        kIsMisconfigurationFailure,
        extension_install_report_log_event.is_misconfiguration_failure());
  }

  if (extension_install_report_log_event.has_install_creation_stage()) {
    event.SetIntKey(
        kInstallCreationStage,
        extension_install_report_log_event.install_creation_stage());
  }

  if (extension_install_report_log_event.has_download_cache_status()) {
    event.SetIntKey(kDownloadCacheStatus,
                    extension_install_report_log_event.download_cache_status());
  }

  if (extension_install_report_log_event.has_unpacker_failure_reason()) {
    event.SetIntKey(
        kUnpackerFailureReason,
        extension_install_report_log_event.unpacker_failure_reason());
  }

  if (extension_install_report_log_event.has_manifest_invalid_error()) {
    event.SetIntKey(
        kManifestInvalidError,
        extension_install_report_log_event.manifest_invalid_error());
  }

  if (extension_install_report_log_event.has_fetch_error_code()) {
    event.SetIntKey(kFetchErrorCode,
                    extension_install_report_log_event.fetch_error_code());
  }

  if (extension_install_report_log_event.has_fetch_tries()) {
    event.SetIntKey(kFetchTries,
                    extension_install_report_log_event.fetch_tries());
  }

  if (extension_install_report_log_event.has_crx_install_error_detail()) {
    event.SetIntKey(
        kCrxInstallErrorDetail,
        extension_install_report_log_event.crx_install_error_detail());
  }

  base::Value wrapper(base::Value::Type::DICTIONARY);
  wrapper.SetKey(kExtensionInstallEvent, std::move(event));

  if (extension_install_report_log_event.has_timestamp()) {
    // Format the current time (UTC) in RFC3339 format
    base::Time timestamp = base::Time::UnixEpoch() +
                           base::TimeDelta::FromMicroseconds(
                               extension_install_report_log_event.timestamp());
    wrapper.SetStringKey(kTime, GetTimeString(timestamp));
  }

  std::string event_id;
  if (GetHash(wrapper, context, &event_id)) {
    wrapper.SetStringKey(kEventId, event_id);
  }

  return wrapper;
}

base::Value ConvertArcAppProtoToValue(
    const em::AppInstallReportRequest* app_install_report_request,
    const base::Value& context) {
  DCHECK(app_install_report_request);

  base::Value event_list(base::Value::Type::LIST);
  std::set<std::string> seen_ids;

  for (const em::AppInstallReport& app_install_report :
       app_install_report_request->app_install_reports()) {
    for (const em::AppInstallReportLogEvent& app_install_report_log_event :
         app_install_report.logs()) {
      base::Value wrapper;
      wrapper = ConvertArcAppEventToValue(
          app_install_report.has_package() ? app_install_report.package() : "",
          app_install_report_log_event, context);
      auto* id = wrapper.FindStringKey(kEventId);
      if (id) {
        if (seen_ids.find(*id) != seen_ids.end()) {
          LOG(WARNING) << "Skipping duplicate event (" << *id
                       << "): " << wrapper;
          continue;
        }
        seen_ids.insert(*id);
      }
      event_list.Append(std::move(wrapper));
    }
  }

  return event_list;
}

base::Value ConvertArcAppEventToValue(
    const std::string& package,
    const em::AppInstallReportLogEvent& app_install_report_log_event,
    const base::Value& context) {
  base::Value event(base::Value::Type::DICTIONARY);

  if (!package.empty())
    event.SetStringKey(kAppPackage, package);

  if (app_install_report_log_event.has_event_type()) {
    event.SetIntKey(kEventType, app_install_report_log_event.event_type());
  }

  if (app_install_report_log_event.has_stateful_total()) {
    // 64-bit ints aren't supported by JSON - must be stored as strings
    std::ostringstream str;
    str << app_install_report_log_event.stateful_total();
    event.SetStringKey(kStatefulTotal, str.str());
  }

  if (app_install_report_log_event.has_stateful_free()) {
    // 64-bit ints aren't supported by JSON - must be stored as strings
    std::ostringstream str;
    str << app_install_report_log_event.stateful_free();
    event.SetStringKey(kStatefulFree, str.str());
  }

  if (app_install_report_log_event.has_clouddps_response()) {
    event.SetIntKey(kCloudDpsResponse,
                    app_install_report_log_event.clouddps_response());
  }

  if (app_install_report_log_event.has_online())
    event.SetBoolKey(kOnline, app_install_report_log_event.online());

  if (app_install_report_log_event.has_session_state_change_type()) {
    event.SetIntKey(kSessionStateChangeType,
                    app_install_report_log_event.session_state_change_type());
  }

  if (app_install_report_log_event.has_android_id()) {
    // 64-bit ints aren't supporetd by JSON - must be stored as strings
    std::ostringstream str;
    str << app_install_report_log_event.android_id();
    event.SetStringKey(kAndroidId, str.str());
  }

  event.SetStringKey(kSerialNumber, GetSerialNumber());

  base::Value wrapper(base::Value::Type::DICTIONARY);
  wrapper.SetKey(kAndroidAppInstallEvent, std::move(event));

  if (app_install_report_log_event.has_timestamp()) {
    // Format the current time (UTC) in RFC3339 format
    base::Time timestamp =
        base::Time::UnixEpoch() + base::TimeDelta::FromMicroseconds(
                                      app_install_report_log_event.timestamp());
    wrapper.SetStringKey(kTime, GetTimeString(timestamp));
  }

  std::string event_id;
  if (GetHash(wrapper, context, &event_id)) {
    wrapper.SetStringKey(kEventId, event_id);
  }

  return wrapper;
}

}  // namespace policy
