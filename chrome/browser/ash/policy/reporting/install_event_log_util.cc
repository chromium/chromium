// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/install_event_log_util.h"

#include <set>

#include "base/hash/md5.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/system/statistics_provider.h"

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
bool GetHash(const base::Value::Dict& event,
             const base::Value::Dict& context,
             std::string* hash) {
  if (hash == nullptr) {
    return false;
  }

  std::string serialized_string;
  JSONStringValueSerializer serializer(&serialized_string);
  if (!serializer.Serialize(event)) {
    return false;
  }

  base::MD5Context ctx;
  base::MD5Init(&ctx);
  base::MD5Update(&ctx, serialized_string);

  if (!serializer.Serialize(context)) {
    return false;
  }
  base::MD5Update(&ctx, serialized_string);

  base::MD5Digest digest;
  base::MD5Final(&digest, &ctx);
  *hash = base::MD5DigestToBase16(digest);
  return true;
}

}  // namespace

std::string GetSerialNumber() {
  return std::string(
      ash::system::StatisticsProvider::GetInstance()->GetMachineID().value_or(
          ""));
}

base::Value::List ConvertExtensionProtoToValue(
    const em::ExtensionInstallReportRequest* extension_install_report_request,
    const base::Value::Dict& context) {
  DCHECK(extension_install_report_request);

  base::Value::List event_list;
  std::set<extensions::ExtensionId> seen_ids;

  for (const em::ExtensionInstallReport& extension_install_report :
       extension_install_report_request->extension_install_reports()) {
    for (const em::ExtensionInstallReportLogEvent&
             extension_install_report_log_event :
         extension_install_report.logs()) {
      base::Value::Dict wrapper = ConvertExtensionEventToValue(
          extension_install_report.has_extension_id()
              ? extension_install_report.extension_id()
              : "",
          extension_install_report_log_event, context);
      auto* id = wrapper.FindString(kEventId);
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

base::Value::Dict ConvertExtensionEventToValue(
    const extensions::ExtensionId& extension_id,
    const em::ExtensionInstallReportLogEvent&
        extension_install_report_log_event,
    const base::Value::Dict& context) {
  base::Value::Dict event;
  if (!extension_id.empty()) {
    event.Set(kExtensionId, extension_id);
  }

  if (extension_install_report_log_event.has_event_type()) {
    event.Set(kEventType, extension_install_report_log_event.event_type());
  }

  if (extension_install_report_log_event.has_stateful_total()) {
    // 64-bit ints aren't supported by JSON - must be stored as strings
    event.Set(kStatefulTotal,
              base::NumberToString(
                  extension_install_report_log_event.stateful_total()));
  }

  if (extension_install_report_log_event.has_stateful_free()) {
    // 64-bit ints aren't supported by JSON - must be stored as strings
    event.Set(kStatefulFree,
              base::NumberToString(
                  extension_install_report_log_event.stateful_free()));
  }

  if (extension_install_report_log_event.has_online()) {
    event.Set(kOnline, extension_install_report_log_event.online());
  }

  if (extension_install_report_log_event.has_session_state_change_type()) {
    event.Set(kSessionStateChangeType,
              extension_install_report_log_event.session_state_change_type());
  }

  if (extension_install_report_log_event.has_downloading_stage()) {
    event.Set(kDownloadingStage,
              extension_install_report_log_event.downloading_stage());
  }

  event.Set(kSerialNumber, GetSerialNumber());

  if (extension_install_report_log_event.has_failure_reason()) {
    event.Set(kFailureReason,
              extension_install_report_log_event.failure_reason());
  }

  if (extension_install_report_log_event.has_user_type()) {
    event.Set(kUserType, extension_install_report_log_event.user_type());
    DCHECK(extension_install_report_log_event.has_is_new_user());
    event.Set(kIsNewUser, extension_install_report_log_event.is_new_user());
  }

  if (extension_install_report_log_event.has_installation_stage()) {
    event.Set(kInstallationStage,
              extension_install_report_log_event.installation_stage());
  }

  if (extension_install_report_log_event.has_extension_type()) {
    event.Set(kExtensionType,
              extension_install_report_log_event.extension_type());
  }

  if (extension_install_report_log_event.has_is_misconfiguration_failure()) {
    event.Set(kIsMisconfigurationFailure,
              extension_install_report_log_event.is_misconfiguration_failure());
  }

  if (extension_install_report_log_event.has_install_creation_stage()) {
    event.Set(kInstallCreationStage,
              extension_install_report_log_event.install_creation_stage());
  }

  if (extension_install_report_log_event.has_download_cache_status()) {
    event.Set(kDownloadCacheStatus,
              extension_install_report_log_event.download_cache_status());
  }

  if (extension_install_report_log_event.has_unpacker_failure_reason()) {
    event.Set(kUnpackerFailureReason,
              extension_install_report_log_event.unpacker_failure_reason());
  }

  if (extension_install_report_log_event.has_manifest_invalid_error()) {
    event.Set(kManifestInvalidError,
              extension_install_report_log_event.manifest_invalid_error());
  }

  if (extension_install_report_log_event.has_fetch_error_code()) {
    event.Set(kFetchErrorCode,
              extension_install_report_log_event.fetch_error_code());
  }

  if (extension_install_report_log_event.has_fetch_tries()) {
    event.Set(kFetchTries, extension_install_report_log_event.fetch_tries());
  }

  if (extension_install_report_log_event.has_crx_install_error_detail()) {
    event.Set(kCrxInstallErrorDetail,
              extension_install_report_log_event.crx_install_error_detail());
  }

  auto wrapper =
      base::Value::Dict().Set(kExtensionInstallEvent, std::move(event));

  if (extension_install_report_log_event.has_timestamp()) {
    // Format the current time (UTC) in RFC3339 format
    base::Time timestamp =
        base::Time::UnixEpoch() +
        base::Microseconds(extension_install_report_log_event.timestamp());
    wrapper.Set(kTime, base::TimeFormatAsIso8601(timestamp));
  }

  std::string event_id;
  if (GetHash(wrapper, context, &event_id)) {
    wrapper.Set(kEventId, event_id);
  }

  return wrapper;
}

base::Value::List ConvertArcAppProtoToValue(
    const em::AppInstallReportRequest* app_install_report_request,
    const base::Value::Dict& context) {
  DCHECK(app_install_report_request);

  base::Value::List event_list;
  std::set<std::string> seen_ids;

  for (const em::AppInstallReport& app_install_report :
       app_install_report_request->app_install_reports()) {
    for (const em::AppInstallReportLogEvent& app_install_report_log_event :
         app_install_report.logs()) {
      base::Value::Dict wrapper = ConvertArcAppEventToValue(
          app_install_report.has_package() ? app_install_report.package() : "",
          app_install_report_log_event, context);
      auto* id = wrapper.FindString(kEventId);
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

base::Value::Dict ConvertArcAppEventToValue(
    const std::string& package,
    const em::AppInstallReportLogEvent& app_install_report_log_event,
    const base::Value::Dict& context) {
  base::Value::Dict event;

  if (!package.empty()) {
    event.Set(kAppPackage, package);
  }

  if (app_install_report_log_event.has_event_type()) {
    event.Set(kEventType, app_install_report_log_event.event_type());
  }

  if (app_install_report_log_event.has_stateful_total()) {
    // 64-bit ints aren't supported by JSON - must be stored as strings
    event.Set(
        kStatefulTotal,
        base::NumberToString(app_install_report_log_event.stateful_total()));
  }

  if (app_install_report_log_event.has_stateful_free()) {
    // 64-bit ints aren't supported by JSON - must be stored as strings
    event.Set(kStatefulFree, base::NumberToString(
                                 app_install_report_log_event.stateful_free()));
  }

  if (app_install_report_log_event.has_clouddps_response()) {
    event.Set(kCloudDpsResponse,
              app_install_report_log_event.clouddps_response());
  }

  if (app_install_report_log_event.has_online()) {
    event.Set(kOnline, app_install_report_log_event.online());
  }

  if (app_install_report_log_event.has_session_state_change_type()) {
    event.Set(kSessionStateChangeType,
              app_install_report_log_event.session_state_change_type());
  }

  if (app_install_report_log_event.has_android_id()) {
    // 64-bit ints aren't supporetd by JSON - must be stored as strings
    event.Set(kAndroidId,
              base::NumberToString(app_install_report_log_event.android_id()));
  }

  event.Set(kSerialNumber, GetSerialNumber());

  auto wrapper =
      base::Value::Dict().Set(kAndroidAppInstallEvent, std::move(event));

  if (app_install_report_log_event.has_timestamp()) {
    // Format the current time (UTC) in RFC3339 format
    base::Time timestamp =
        base::Time::UnixEpoch() +
        base::Microseconds(app_install_report_log_event.timestamp());
    wrapper.Set(kTime, base::TimeFormatAsIso8601(timestamp));
  }

  std::string event_id;
  if (GetHash(wrapper, context, &event_id)) {
    wrapper.Set(kEventId, event_id);
  }

  return wrapper;
}

reporting::AndroidAppInstallEvent CreateAndroidAppInstallEvent(
    const std::string& package,
    const enterprise_management::AppInstallReportLogEvent& event) {
  auto result = reporting::AndroidAppInstallEvent();
  result.set_app_package(package);
  result.set_serial_number(GetSerialNumber());
  result.set_event_type(event.event_type());
  result.set_stateful_total(event.stateful_total());
  result.set_stateful_free(event.stateful_free());
  result.set_clouddps_response(event.clouddps_response());
  result.set_online(event.online());
  result.set_session_state_change_type(event.session_state_change_type());
  result.set_android_id(event.android_id());
  return result;
}

}  // namespace policy
