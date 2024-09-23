// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/util/deep_link_util.h"

#include <set>

#include "ash/assistant/util/i18n_util.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/containers/contains.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace ash::assistant::util {

namespace {

// Supported deep link param keys. These values must be kept in sync with the
// server. See more details at go/cros-assistant-deeplink.
constexpr char kActionParamKey[] = "action";
constexpr char kCategoryParamKey[] = "category";
constexpr char kClientIdParamKey[] = "clientId";
constexpr char kDurationMsParamKey[] = "durationMs";
constexpr char kEidParamKey[] = "eid";
constexpr char kEntryPointParamKey[] = "entryPoint";
constexpr char kHrefParamKey[] = "href";
constexpr char kIdParamKey[] = "id";
constexpr char kIndexParamKey[] = "index";
constexpr char kQueryParamKey[] = "q";
constexpr char kQuerySourceParamKey[] = "querySource";
constexpr char kPageParamKey[] = "page";
constexpr char kRelaunchParamKey[] = "relaunch";
constexpr char kSourceParamKey[] = "source";
constexpr char kTypeParamKey[] = "type";
constexpr char kVeIdParamKey[] = "veId";

// Supported alarm/timer action deep link param values.
constexpr char kAddTimeToTimer[] = "addTimeToTimer";
constexpr char kPauseTimer[] = "pauseTimer";
constexpr char kRemoveAlarmOrTimer[] = "removeAlarmOrTimer";
constexpr char kResumeTimer[] = "resumeTimer";

// Supported reminder action deep link param values.
constexpr char kCreateReminder[] = "create";
constexpr char kEditReminder[] = "edit";

// Supported deep link prefixes. These values must be kept in sync with the
// server. See more details at go/cros-assistant-deeplink.
constexpr char kAssistantAlarmTimerPrefix[] = "googleassistant://alarm-timer";
constexpr char kChromeSettingsPrefix[] = "googleassistant://chrome-settings";
constexpr char kAssistantFeedbackPrefix[] = "googleassistant://send-feedback";
constexpr char kAssistantListsPrefix[] = "googleassistant://lists";
constexpr char kAssistantNotesPrefix[] = "googleassistant://notes";
constexpr char kAssistantOnboardingPrefix[] = "googleassistant://onboarding";
constexpr char kAssistantQueryPrefix[] = "googleassistant://send-query";
constexpr char kAssistantRemindersPrefix[] = "googleassistant://reminders";
constexpr char kAssistantScreenshotPrefix[] =
    "googleassistant://take-screenshot";
constexpr char kAssistantSettingsPrefix[] = "googleassistant://settings";
constexpr char kAssistantTaskManagerPrefix[] = "googleassistant://task-manager";

// Helpers ---------------------------------------------------------------------

std::string GetAlarmTimerActionParamValue(AlarmTimerAction action) {
  switch (action) {
    case AlarmTimerAction::kAddTimeToTimer:
      return kAddTimeToTimer;
    case AlarmTimerAction::kPauseTimer:
      return kPauseTimer;
    case AlarmTimerAction::kRemoveAlarmOrTimer:
      return kRemoveAlarmOrTimer;
    case AlarmTimerAction::kResumeTimer:
      return kResumeTimer;
  }
  NOTREACHED();
}

std::string GetDeepLinkParamKey(DeepLinkParam param) {
  switch (param) {
    case DeepLinkParam::kAction:
      return kActionParamKey;
    case DeepLinkParam::kCategory:
      return kCategoryParamKey;
    case DeepLinkParam::kClientId:
      return kClientIdParamKey;
    case DeepLinkParam::kDurationMs:
      return kDurationMsParamKey;
    case DeepLinkParam::kEid:
      return kEidParamKey;
    case DeepLinkParam::kEntryPoint:
      return kEntryPointParamKey;
    case DeepLinkParam::kHref:
      return kHrefParamKey;
    case DeepLinkParam::kId:
      return kIdParamKey;
    case DeepLinkParam::kIndex:
      return kIndexParamKey;
    case DeepLinkParam::kPage:
      return kPageParamKey;
    case DeepLinkParam::kQuery:
      return kQueryParamKey;
    case DeepLinkParam::kQuerySource:
      return kQuerySourceParamKey;
    case DeepLinkParam::kRelaunch:
      return kRelaunchParamKey;
    case DeepLinkParam::kType:
      return kTypeParamKey;
    case DeepLinkParam::kVeId:
      return kVeIdParamKey;
  }
  NOTREACHED();
}

GURL AppendOrReplaceDeepLinkParam(const GURL& deep_link,
                                  DeepLinkParam param,
                                  const std::string& value) {
  DCHECK(IsDeepLinkUrl(deep_link));
  const std::string key = GetDeepLinkParamKey(param);
  return net::AppendOrReplaceQueryParameter(deep_link, key, value);
}

}  // namespace

// Utilities -------------------------------------------------------------------

GURL AppendOrReplaceEntryPointParam(const GURL& deep_link,
                                    AssistantEntryPoint entry_point) {
  return AppendOrReplaceDeepLinkParam(
      deep_link, DeepLinkParam::kEntryPoint,
      base::NumberToString(static_cast<int>(entry_point)));
}

GURL AppendOrReplaceQuerySourceParam(const GURL& deep_link,
                                     AssistantQuerySource query_source) {
  return AppendOrReplaceDeepLinkParam(
      deep_link, DeepLinkParam::kQuerySource,
      base::NumberToString(static_cast<int>(query_source)));
}

std::optional<GURL> CreateAlarmTimerDeepLink(
    AlarmTimerAction action,
    std::optional<std::string> alarm_timer_id,
    std::optional<base::TimeDelta> duration) {
  switch (action) {
    case assistant::util::AlarmTimerAction::kAddTimeToTimer:
      DCHECK(alarm_timer_id.has_value() && duration.has_value());
      if (!alarm_timer_id.has_value() || !duration.has_value())
        return std::nullopt;
      break;
    case assistant::util::AlarmTimerAction::kPauseTimer:
    case assistant::util::AlarmTimerAction::kRemoveAlarmOrTimer:
    case assistant::util::AlarmTimerAction::kResumeTimer:
      DCHECK(alarm_timer_id.has_value() && !duration.has_value());
      if (!alarm_timer_id.has_value() || duration.has_value())
        return std::nullopt;
      break;
  }

  GURL url = net::AppendOrReplaceQueryParameter(
      GURL(kAssistantAlarmTimerPrefix), kActionParamKey,
      GetAlarmTimerActionParamValue(action));

  if (alarm_timer_id.has_value()) {
    url = net::AppendOrReplaceQueryParameter(url, kIdParamKey,
                                             alarm_timer_id.value());
  }

  if (duration.has_value()) {
    url = net::AppendOrReplaceQueryParameter(
        url, kDurationMsParamKey,
        base::NumberToString(duration->InMilliseconds()));
  }

  return url;
}

GURL CreateAssistantQueryDeepLink(const std::string& query) {
  return net::AppendOrReplaceQueryParameter(GURL(kAssistantQueryPrefix),
                                            kQueryParamKey, query);
}

GURL CreateAssistantSettingsDeepLink() {
  return GURL(kAssistantSettingsPrefix);
}

std::map<std::string, std::string> GetDeepLinkParams(const GURL& deep_link) {
  std::map<std::string, std::string> params;

  if (!IsDeepLinkUrl(deep_link))
    return params;

  if (!deep_link.has_query())
    return params;

  // Key-value pairs are '&' delimited and the keys/values are '=' delimited.
  // Example: "googleassistant://onboarding?k1=v1&k2=v2".
  base::StringPairs pairs;
  if (!base::SplitStringIntoKeyValuePairs(deep_link.query(), '=', '&',
                                          &pairs)) {
    return params;
  }

  for (const auto& pair : pairs)
    params[pair.first] = pair.second;

  return params;
}

std::optional<std::string> GetDeepLinkParam(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param) {
  const std::string key = GetDeepLinkParamKey(param);
  const auto it = params.find(key);
  return it != params.end()
             ? std::optional<std::string>(base::UnescapeBinaryURLComponent(
                   it->second, base::UnescapeRule::REPLACE_PLUS_WITH_SPACE))
             : std::nullopt;
}

std::optional<AlarmTimerAction> GetDeepLinkParamAsAlarmTimerAction(
    const std::map<std::string, std::string>& params) {
  const std::optional<std::string>& action_string_value =
      GetDeepLinkParam(params, DeepLinkParam::kAction);
  if (!action_string_value.has_value())
    return std::nullopt;

  if (action_string_value.value() == kAddTimeToTimer)
    return AlarmTimerAction::kAddTimeToTimer;

  if (action_string_value.value() == kPauseTimer)
    return AlarmTimerAction::kPauseTimer;

  if (action_string_value.value() == kRemoveAlarmOrTimer)
    return AlarmTimerAction::kRemoveAlarmOrTimer;

  if (action_string_value.value() == kResumeTimer)
    return AlarmTimerAction::kResumeTimer;

  return std::nullopt;
}

std::optional<bool> GetDeepLinkParamAsBool(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param) {
  const std::optional<std::string>& value = GetDeepLinkParam(params, param);
  if (value == "true")
    return true;

  if (value == "false")
    return false;

  return std::nullopt;
}

std::optional<AssistantEntryPoint> GetDeepLinkParamAsEntryPoint(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param) {
  const std::optional<int> value = GetDeepLinkParamAsInt(params, param);
  if (!value.has_value() || (value.value() < 0) ||
      (value.value() > static_cast<int>(AssistantEntryPoint::kMaxValue))) {
    return std::nullopt;
  }
  return static_cast<AssistantEntryPoint>(value.value());
}

std::optional<GURL> GetDeepLinkParamAsGURL(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param) {
  const std::optional<std::string>& spec = GetDeepLinkParam(params, param);
  return spec.has_value() ? std::optional<GURL>(spec.value()) : std::nullopt;
}

std::optional<int> GetDeepLinkParamAsInt(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param) {
  const std::optional<std::string>& value = GetDeepLinkParam(params, param);
  if (value.has_value()) {
    int result;
    if (base::StringToInt(value.value(), &result))
      return result;
  }

  return std::nullopt;
}

std::optional<int64_t> GetDeepLinkParamAsInt64(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param) {
  const std::optional<std::string>& value = GetDeepLinkParam(params, param);
  if (value.has_value()) {
    int64_t result;
    if (base::StringToInt64(value.value(), &result))
      return result;
  }

  return std::nullopt;
}

std::optional<AssistantQuerySource> GetDeepLinkParamAsQuerySource(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param) {
  const std::optional<int> value = GetDeepLinkParamAsInt(params, param);
  if (!value.has_value() || (value.value() < 0) ||
      (value.value() > static_cast<int>(AssistantQuerySource::kMaxValue))) {
    return std::nullopt;
  }
  return static_cast<AssistantQuerySource>(value.value());
}

std::optional<ReminderAction> GetDeepLinkParamAsRemindersAction(
    const std::map<std::string, std::string> params,
    DeepLinkParam param) {
  const std::optional<std::string>& value = GetDeepLinkParam(params, param);
  if (value == kCreateReminder)
    return ReminderAction::kCreate;

  if (value == kEditReminder)
    return ReminderAction::kEdit;

  return std::nullopt;
}

std::optional<base::TimeDelta> GetDeepLinkParamAsTimeDelta(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param) {
  if (param != DeepLinkParam::kDurationMs)
    return std::nullopt;

  const std::optional<int64_t>& duration_ms =
      GetDeepLinkParamAsInt64(params, DeepLinkParam::kDurationMs);
  if (!duration_ms.has_value())
    return std::nullopt;

  return base::Milliseconds(duration_ms.value());
}

DeepLinkType GetDeepLinkType(const GURL& url) {
  // Map of supported deep link types to their prefixes.
  static const std::map<DeepLinkType, std::string> kSupportedDeepLinks = {
      {DeepLinkType::kAlarmTimer, kAssistantAlarmTimerPrefix},
      {DeepLinkType::kChromeSettings, kChromeSettingsPrefix},
      {DeepLinkType::kFeedback, kAssistantFeedbackPrefix},
      {DeepLinkType::kLists, kAssistantListsPrefix},
      {DeepLinkType::kNotes, kAssistantNotesPrefix},
      {DeepLinkType::kOnboarding, kAssistantOnboardingPrefix},
      {DeepLinkType::kQuery, kAssistantQueryPrefix},
      {DeepLinkType::kReminders, kAssistantRemindersPrefix},
      {DeepLinkType::kScreenshot, kAssistantScreenshotPrefix},
      {DeepLinkType::kSettings, kAssistantSettingsPrefix},
      {DeepLinkType::kTaskManager, kAssistantTaskManagerPrefix},
  };

  for (const auto& supported_deep_link : kSupportedDeepLinks) {
    if (base::StartsWith(url.spec(), supported_deep_link.second,
                         base::CompareCase::SENSITIVE)) {
      return supported_deep_link.first;
    }
  }
  return DeepLinkType::kUnsupported;
}

bool IsDeepLinkType(const GURL& url, DeepLinkType type) {
  return GetDeepLinkType(url) == type;
}

bool IsDeepLinkUrl(const GURL& url) {
  return GetDeepLinkType(url) != DeepLinkType::kUnsupported;
}

std::optional<GURL> GetAssistantUrl(
    DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  std::string top_level_url;
  std::string by_id_url;

  switch (type) {
    case DeepLinkType::kLists: {
      const auto& type_param = GetDeepLinkParam(params, DeepLinkParam::kType);
      top_level_url =
          std::string("https://assistant.google.com/lists/mainview");
      by_id_url = (type_param && type_param.value().compare("shopping") == 0)
                      ? std::string("https://shoppinglist.google.com/lists/")
                      : std::string("https://assistant.google.com/lists/list/");
      break;
    }
    case DeepLinkType::kNotes:
      top_level_url = std::string(
          "https://assistant.google.com/lists/mainview?note_tap=true");
      by_id_url = std::string("https://assistant.google.com/lists/note/");
      break;
    case DeepLinkType::kReminders:
      top_level_url =
          std::string("https://assistant.google.com/reminders/mainview");
      by_id_url = std::string("https://assistant.google.com/reminders/id/");
      break;
    default:
      NOTREACHED();
  }

  const auto& id = GetDeepLinkParam(params, DeepLinkParam::kId);
  const auto& eid = GetDeepLinkParam(params, DeepLinkParam::kEid);
  GURL url = (id && !id.value().empty()) ? GURL(by_id_url + id.value())
                                         : GURL(top_level_url);
  if (eid && !eid.value().empty())
    url = net::AppendOrReplaceQueryParameter(url, kEidParamKey, eid.value());

  // Source is currently assumed to be |Assistant|. If need be, we can make
  // |source| a deep link parameter in the future.
  constexpr char kDefaultSource[] = "Assistant";
  return net::AppendOrReplaceQueryParameter(CreateLocalizedGURL(url.spec()),
                                            kSourceParamKey, kDefaultSource);
}

GURL GetChromeSettingsUrl(const std::optional<std::string>& page) {
  static constexpr char kChromeOsSettingsUrl[] = "chrome://os-settings/";

  // Note that we only allow deep linking to a subset of pages. If a deep link
  // requests a page not contained in this map, we fallback gracefully to
  // top-level Chrome OS Settings. We may wish to allow deep linking into
  // Browser Settings at some point in the future at which point we will define
  // an analogous collection of |kAllowedBrowserPages|.
  static const std::map<std::string, std::string> kAllowedOsPages = {
      {/*page=*/"googleAssistant",
       /*os_page=*/chromeos::settings::mojom::kAssistantSubpagePath},
      {/*page=*/"languages",
       /*os_page=*/chromeos::settings::mojom::kLanguagesSubpagePath}};

  return page && base::Contains(kAllowedOsPages, page.value())
             ? GURL(kChromeOsSettingsUrl + kAllowedOsPages.at(page.value()))
             : GURL(kChromeOsSettingsUrl);
}

std::optional<GURL> GetWebUrl(const GURL& deep_link) {
  return GetWebUrl(GetDeepLinkType(deep_link), GetDeepLinkParams(deep_link));
}

std::optional<GURL> GetWebUrl(
    DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  static constexpr char kAssistantSettingsWebUrl[] =
      "https://assistant.google.com/settings/mainpage";

  if (!IsWebDeepLinkType(type, params))
    return std::nullopt;

  switch (type) {
    case DeepLinkType::kLists:
    case DeepLinkType::kNotes:
    case DeepLinkType::kReminders:
      return GetAssistantUrl(type, params);
    case DeepLinkType::kSettings:
      return CreateLocalizedGURL(kAssistantSettingsWebUrl);
    case DeepLinkType::kUnsupported:
    case DeepLinkType::kAlarmTimer:
    case DeepLinkType::kChromeSettings:
    case DeepLinkType::kFeedback:
    case DeepLinkType::kOnboarding:
    case DeepLinkType::kQuery:
    case DeepLinkType::kScreenshot:
    case DeepLinkType::kTaskManager:
      NOTREACHED();
  }

  NOTREACHED();
}

bool IsWebDeepLink(const GURL& deep_link) {
  return IsWebDeepLinkType(GetDeepLinkType(deep_link),
                           GetDeepLinkParams(deep_link));
}

bool IsWebDeepLinkType(DeepLinkType type,
                       const std::map<std::string, std::string>& params) {
  // Create/edit reminder deeplink will trigger Assistant conversation flow.
  if (type == DeepLinkType::kReminders &&
      GetDeepLinkParamAsRemindersAction(params, DeepLinkParam::kAction)) {
    return false;
  }
  // Set of deep link types which open web contents in the Assistant UI.
  static const std::set<DeepLinkType> kWebDeepLinks = {
      DeepLinkType::kLists, DeepLinkType::kNotes, DeepLinkType::kReminders,
      DeepLinkType::kSettings};

  return base::Contains(kWebDeepLinks, type);
}

}  // namespace ash::assistant::util
