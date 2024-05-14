// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/time_limits/app_time_policy_helpers.h"

#include <utility>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_types.h"

namespace ash {
namespace app_time {
namespace policy {

const char kUrlList[] = "url_list";
const char kAppList[] = "app_list";
const char kAppId[] = "app_id";
const char kAppType[] = "app_type";
const char kAppLimitsArray[] = "app_limits";
const char kAppInfoDict[] = "app_info";
const char kRestrictionEnum[] = "restriction";
const char kDailyLimitInt[] = "daily_limit_mins";
const char kLastUpdatedString[] = "last_updated_millis";
const char kResetAtDict[] = "reset_at";
const char kHourInt[] = "hour";
const char kMinInt[] = "minute";
const char kActivityReportingEnabled[] = "activity_reporting_enabled";

apps::AppType PolicyStringToAppType(const std::string& app_type) {
  if (app_type == "ARC")
    return apps::AppType::kArc;
  if (app_type == "BOREALIS")
    return apps::AppType::kBorealis;
  // After the splitting of kChromeApp from the kExtension type in which
  // the original kExtension was renamed to kChromeApp (crrev.com/c/3314469),
  // the subsequent kExtension type refers to Chrome browser extensions only.
  // The legacy kChromeApp policy string remains unchanged.
  if (app_type == "BROWSER-EXTENSION")
    return apps::AppType::kExtension;
  if (app_type == "BRUSCHETTA")
    return apps::AppType::kBruschetta;
  if (app_type == "BUILT-IN")
    return apps::AppType::kBuiltIn;
  if (app_type == "CROSTINI")
    return apps::AppType::kCrostini;
  if (app_type == "EXTENSION")
    return apps::AppType::kChromeApp;
  if (app_type == "LACROS-BROWSER")
    return apps::AppType::kStandaloneBrowser;
  if (app_type == "LACROS-CHROME-APP")
    return apps::AppType::kStandaloneBrowserChromeApp;
  if (app_type == "LACROS-EXTENSION")
    return apps::AppType::kStandaloneBrowserExtension;
  if (app_type == "PLUGIN-VM")
    return apps::AppType::kPluginVm;
  if (app_type == "REMOTE")
    return apps::AppType::kRemote;
  if (app_type == "SYSTEM-WEB")
    return apps::AppType::kSystemWeb;
  if (app_type == "WEB")
    return apps::AppType::kWeb;
  if (app_type == "UNKNOWN")
    return apps::AppType::kUnknown;

  NOTREACHED_IN_MIGRATION();
  return apps::AppType::kUnknown;
}

std::string AppTypeToPolicyString(apps::AppType app_type) {
  switch (app_type) {
    case apps::AppType::kArc:
      return "ARC";
    case apps::AppType::kBorealis:
      return "BOREALIS";
    case apps::AppType::kExtension:
      return "BROWSER-EXTENSION";
    case apps::AppType::kBruschetta:
      return "BRUSCHETTA";
    case apps::AppType::kBuiltIn:
      return "BUILT-IN";
    case apps::AppType::kCrostini:
      return "CROSTINI";
    case apps::AppType::kChromeApp:
      return "EXTENSION";
    case apps::AppType::kStandaloneBrowser:
      return "LACROS-BROWSER";
    case apps::AppType::kStandaloneBrowserChromeApp:
      return "LACROS-CHROME-APP";
    case apps::AppType::kStandaloneBrowserExtension:
      return "LACROS-EXTENSION";
    case apps::AppType::kPluginVm:
      return "PLUGIN-VM";
    case apps::AppType::kRemote:
      return "REMOTE";
    case apps::AppType::kSystemWeb:
      return "SYSTEM-WEB";
    case apps::AppType::kWeb:
      return "WEB";
    case apps::AppType::kUnknown:
      return "UNKNOWN";
  }
  NOTREACHED_IN_MIGRATION();
}

AppRestriction PolicyStringToAppRestriction(const std::string& restriction) {
  if (restriction == "BLOCK")
    return AppRestriction::kBlocked;
  if (restriction == "TIME_LIMIT")
    return AppRestriction::kTimeLimit;

  NOTREACHED_IN_MIGRATION();
  return AppRestriction::kUnknown;
}

std::string AppRestrictionToPolicyString(const AppRestriction& restriction) {
  switch (restriction) {
    case AppRestriction::kBlocked:
      return "BLOCK";
    case AppRestriction::kTimeLimit:
      return "TIME_LIMIT";
    default:
      NOTREACHED_IN_MIGRATION();
      return "";
  }
}

std::optional<AppId> AppIdFromDict(const base::Value::Dict* dict) {
  if (!dict) {
    return std::nullopt;
  }

  const std::string* id = dict->FindString(kAppId);
  if (!id || id->empty()) {
    DLOG(ERROR) << "Invalid id.";
    return std::nullopt;
  }

  const std::string* type_string = dict->FindString(kAppType);
  if (!type_string || type_string->empty()) {
    DLOG(ERROR) << "Invalid type.";
    return std::nullopt;
  }

  return AppId(PolicyStringToAppType(*type_string), *id);
}

base::Value::Dict AppIdToDict(const AppId& app_id) {
  base::Value::Dict dict;
  dict.Set(kAppId, base::Value(app_id.app_id()));
  dict.Set(kAppType, base::Value(AppTypeToPolicyString(app_id.app_type())));

  return dict;
}

std::optional<AppId> AppIdFromAppInfoDict(const base::Value::Dict* dict) {
  if (!dict) {
    return std::nullopt;
  }

  const base::Value::Dict* app_info = dict->FindDict(kAppInfoDict);
  if (!app_info) {
    DLOG(ERROR) << "Invalid app info dictionary.";
    return std::nullopt;
  }
  return AppIdFromDict(app_info);
}

std::optional<AppLimit> AppLimitFromDict(const base::Value::Dict& dict) {
  const std::string* restriction_string = dict.FindString(kRestrictionEnum);
  if (!restriction_string || restriction_string->empty()) {
    DLOG(ERROR) << "Invalid restriction.";
    return std::nullopt;
  }
  const AppRestriction restriction =
      PolicyStringToAppRestriction(*restriction_string);

  std::optional<int> daily_limit_mins = dict.FindInt(kDailyLimitInt);
  if ((restriction == AppRestriction::kTimeLimit && !daily_limit_mins) ||
      (restriction == AppRestriction::kBlocked && daily_limit_mins)) {
    DLOG(ERROR) << "Invalid restriction.";
    return std::nullopt;
  }

  std::optional<base::TimeDelta> daily_limit;
  if (daily_limit_mins) {
    daily_limit = base::Minutes(*daily_limit_mins);
    if (daily_limit &&
        (*daily_limit < base::Hours(0) || *daily_limit > base::Hours(24))) {
      DLOG(ERROR) << "Invalid daily limit.";
      return std::nullopt;
    }
  }

  const std::string* last_updated_string = dict.FindString(kLastUpdatedString);
  int64_t last_updated_millis;
  if (!last_updated_string || last_updated_string->empty() ||
      !base::StringToInt64(*last_updated_string, &last_updated_millis)) {
    DLOG(ERROR) << "Invalid last updated time.";
    return std::nullopt;
  }

  const base::Time last_updated =
      base::Time::UnixEpoch() + base::Milliseconds(last_updated_millis);

  return AppLimit(restriction, daily_limit, last_updated);
}

base::Value::Dict AppLimitToDict(const AppLimit& limit) {
  base::Value::Dict dict;
  dict.Set(kRestrictionEnum,
           base::Value(AppRestrictionToPolicyString(limit.restriction())));
  if (limit.daily_limit())
    dict.Set(kDailyLimitInt, base::Value(limit.daily_limit()->InMinutes()));
  const std::string last_updated_string = base::NumberToString(
      (limit.last_updated() - base::Time::UnixEpoch()).InMilliseconds());
  dict.Set(kLastUpdatedString, base::Value(last_updated_string));

  return dict;
}

std::optional<base::TimeDelta> ResetTimeFromDict(
    const base::Value::Dict& dict) {
  const base::Value* reset_dict = dict.Find(kResetAtDict);
  if (!reset_dict || !reset_dict->is_dict()) {
    DLOG(ERROR) << "Invalid reset time dictionary.";
    return std::nullopt;
  }

  std::optional<int> hour = reset_dict->GetDict().FindInt(kHourInt);
  if (!hour) {
    DLOG(ERROR) << "Invalid reset hour.";
    return std::nullopt;
  }

  std::optional<int> minutes = reset_dict->GetDict().FindInt(kMinInt);
  if (!minutes) {
    DLOG(ERROR) << "Invalid reset minutes.";
    return std::nullopt;
  }

  const int hour_in_mins = base::Hours(1).InMinutes();
  return base::Minutes(hour.value() * hour_in_mins + minutes.value());
}

base::Value::Dict ResetTimeToDict(int hour, int minutes) {
  base::Value::Dict dict;
  dict.Set(kHourInt, base::Value(hour));
  dict.Set(kMinInt, base::Value(minutes));

  return dict;
}

std::optional<bool> ActivityReportingEnabledFromDict(
    const base::Value::Dict& dict) {
  return dict.FindBool(kActivityReportingEnabled);
}

std::map<AppId, AppLimit> AppLimitsFromDict(const base::Value::Dict& dict) {
  std::map<AppId, AppLimit> app_limits;

  const base::Value::List* limits_array = dict.FindList(kAppLimitsArray);
  if (!limits_array) {
    DLOG(ERROR) << "Invalid app limits list.";
    return app_limits;
  }

  for (const base::Value& app_limits_dict : *limits_array) {
    if (!app_limits_dict.is_dict()) {
      DLOG(ERROR) << "Invalid app limits entry. ";
      continue;
    }

    std::optional<AppId> app_id =
        AppIdFromAppInfoDict(&app_limits_dict.GetDict());
    if (!app_id) {
      DLOG(ERROR) << "Invalid app id.";
      continue;
    }

    std::optional<AppLimit> app_limit =
        AppLimitFromDict(app_limits_dict.GetDict());
    if (!app_limit) {
      DLOG(ERROR) << "Invalid app limit.";
      continue;
    }

    app_limits.emplace(*app_id, *app_limit);
  }

  return app_limits;
}

}  // namespace policy
}  // namespace app_time
}  // namespace ash
