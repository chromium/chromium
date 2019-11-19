// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/app_install_event_log_util.h"

#include "base/hash/md5.h"
#include "base/json/json_string_value_serializer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/system/statistics_provider.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"

namespace em = enterprise_management;

namespace policy {

namespace {
// Key names used when building the dictionary to pass to the Chrome Reporting
// API.
constexpr char kAppPackage[] = "appPackage";
constexpr char kEventType[] = "eventType";
constexpr char kStatefulTotal[] = "statefulTotal";
constexpr char kStatefulFree[] = "statefulFree";
constexpr char kCloudDpsResponse[] = "clouddpsResponse";
constexpr char kOnline[] = "online";
constexpr char kSessionStateChangeType[] = "sessionStateChangeType";
constexpr char kSerialNumber[] = "serialNumber";
constexpr char kGaiaId[] = "gaiaId";
constexpr char kAndroidAppInstallEvent[] = "androidAppInstallEvent";
constexpr char kTime[] = "time";
constexpr char kEventId[] = "eventId";

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

}  // namespace

bool GetGaiaId(Profile* profile, int* gaia_id) {
  if (!profile)
    return false;
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return false;
  if (!base::StringToInt(user->GetAccountId().GetGaiaId(), gaia_id))
    return false;
  return true;
}

std::string GetSerialNumber() {
  return chromeos::system::StatisticsProvider::GetInstance()
      ->GetEnterpriseMachineID();
}

base::Value ConvertProtoToValue(
    const em::AppInstallReportRequest* app_install_report_request,
    Profile* profile) {
  DCHECK(app_install_report_request);

  base::Value event_list(base::Value::Type::LIST);

  for (const em::AppInstallReport& app_install_report :
       app_install_report_request->app_install_reports()) {
    for (const em::AppInstallReportLogEvent& app_install_report_log_event :
         app_install_report.logs()) {
      base::Value wrapper;
      wrapper = ConvertEventToValue(
          app_install_report.has_package() ? app_install_report.package() : "",
          app_install_report_log_event, profile);
      event_list.Append(std::move(wrapper));
    }
  }

  return event_list;
}

base::Value ConvertEventToValue(
    const std::string& package,
    const em::AppInstallReportLogEvent& app_install_report_log_event,
    Profile* profile) {
  base::Value event(base::Value::Type::DICTIONARY);

  if (!package.empty())
    event.SetStringKey(kAppPackage, package);

  if (app_install_report_log_event.has_event_type()) {
    event.SetIntKey(kEventType, app_install_report_log_event.event_type());
  }

  if (app_install_report_log_event.has_stateful_total()) {
    event.SetIntKey(kStatefulTotal,
                    app_install_report_log_event.stateful_total());
  }

  if (app_install_report_log_event.has_stateful_free()) {
    event.SetIntKey(kStatefulFree,
                    app_install_report_log_event.stateful_free());
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

  event.SetStringKey(kSerialNumber, GetSerialNumber());

  int gaia_id;
  if (GetGaiaId(profile, &gaia_id))
    event.SetIntKey(kGaiaId, gaia_id);

  base::Value wrapper(base::Value::Type::DICTIONARY);
  wrapper.SetKey(kAndroidAppInstallEvent, std::move(event));

  if (app_install_report_log_event.has_timestamp()) {
    // Format the current time (UTC) in RFC3339 format
    base::Time timestamp =
        base::Time::UnixEpoch() + base::TimeDelta::FromMicroseconds(
                                      app_install_report_log_event.timestamp());
    base::Time::Exploded time_exploded;
    timestamp.UTCExplode(&time_exploded);
    std::string time_str = base::StringPrintf(
        "%d-%02d-%02dT%02d:%02d:%02d.%03dZ", time_exploded.year,
        time_exploded.month, time_exploded.day_of_month, time_exploded.hour,
        time_exploded.minute, time_exploded.second, time_exploded.millisecond);
    wrapper.SetStringKey(kTime, time_str);
  }

  return wrapper;
}

void AppendEventId(base::Value* event_list, const base::Value& context) {
  DCHECK(event_list);

  DCHECK(event_list->is_list());
  for (auto& event : event_list->GetList()) {
    std::string event_id;
    if (GetHash(event, context, &event_id))
      event.SetStringKey(kEventId, event_id);
  }
}

}  // namespace policy
