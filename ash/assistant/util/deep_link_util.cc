// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/util/deep_link_util.h"

#include <array>
#include <set>

#include "ash/assistant/util/i18n_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace ash {
namespace assistant {
namespace util {

namespace {

// Supported deep link param keys. These values must be kept in sync with the
// server. See more details at go/cros-assistant-deeplink.
constexpr char kActionParamKey[] = "action";
constexpr char kClientIdParamKey[] = "clientId";
constexpr char kDurationMsParamKey[] = "durationMs";
constexpr char kIdParamKey[] = "id";
constexpr char kQueryParamKey[] = "q";
constexpr char kPageParamKey[] = "page";
constexpr char kRelaunchParamKey[] = "relaunch";
constexpr char kSourceParamKey[] = "source";

// Supported alarm/timer action deep link param values.
constexpr char kAddTimeToTimer[] = "addTimeToTimer";
constexpr char kStopAlarmTimerRinging[] = "stopAlarmTimerRinging";

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
constexpr char kAssistantWhatsOnMyScreenPrefix[] =
    "googleassistant://whats-on-my-screen";

}  // namespace

// Utilities -------------------------------------------------------------------

base::Optional<GURL> CreateAlarmTimerDeepLink(
    AlarmTimerAction action,
    base::Optional<std::string> alarm_timer_id,
    base::Optional<base::TimeDelta> duration) {
  GURL url = GURL(kAssistantAlarmTimerPrefix);

  switch (action) {
    case assistant::util::AlarmTimerAction::kAddTimeToTimer:
      DCHECK(alarm_timer_id.has_value() && duration.has_value());
      if (!alarm_timer_id.has_value() || !duration.has_value())
        return base::nullopt;
      url = net::AppendOrReplaceQueryParameter(url, kActionParamKey,
                                               kAddTimeToTimer);
      break;
    case assistant::util::AlarmTimerAction::kStopRinging:
      DCHECK(!alarm_timer_id.has_value() && !duration.has_value());
      if (alarm_timer_id.has_value() || duration.has_value())
        return base::nullopt;
      url = net::AppendOrReplaceQueryParameter(url, kActionParamKey,
                                               kStopAlarmTimerRinging);
      break;
  }

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

GURL CreateWhatsOnMyScreenDeepLink() {
  return GURL(kAssistantWhatsOnMyScreenPrefix);
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

base::Optional<std::string> GetDeepLinkParam(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param) {
  // Map of supported deep link params to their keys.
  static const std::map<DeepLinkParam, std::string> kDeepLinkParamKeys = {
      {DeepLinkParam::kAction, kActionParamKey},
      {DeepLinkParam::kClientId, kClientIdParamKey},
      {DeepLinkParam::kDurationMs, kDurationMsParamKey},
      {DeepLinkParam::kId, kIdParamKey},
      {DeepLinkParam::kPage, kPageParamKey},
      {DeepLinkParam::kQuery, kQueryParamKey},
      {DeepLinkParam::kRelaunch, kRelaunchParamKey}};

  const std::string& key = kDeepLinkParamKeys.at(param);
  const auto it = params.find(key);
  return it != params.end()
             ? base::Optional<std::string>(net::UnescapeBinaryURLComponent(
                   it->second, net::UnescapeRule::REPLACE_PLUS_WITH_SPACE))
             : base::nullopt;
}

base::Optional<bool> GetDeepLinkParamAsBool(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param) {
  const base::Optional<std::string>& value = GetDeepLinkParam(params, param);
  if (value == "true")
    return true;

  if (value == "false")
    return false;

  return base::nullopt;
}

base::Optional<ReminderAction> GetDeepLinkParamAsRemindersAction(
    const std::map<std::string, std::string> params,
    DeepLinkParam param) {
  const base::Optional<std::string>& value = GetDeepLinkParam(params, param);
  if (value == kCreateReminder)
    return ReminderAction::kCreate;

  if (value == kEditReminder)
    return ReminderAction::kEdit;

  return base::nullopt;
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
      {DeepLinkType::kWhatsOnMyScreen, kAssistantWhatsOnMyScreenPrefix}};

  for (const auto& supported_deep_link : kSupportedDeepLinks) {
    if (base::StartsWith(url.spec(), supported_deep_link.second,
                         base::CompareCase::SENSITIVE)) {
      return supported_deep_link.first;
    }
  }
  return DeepLinkType::kUnsupported;
}

base::Optional<AlarmTimerAction> GetDeepLinkParamAsAlarmTimerAction(
    const std::map<std::string, std::string>& params) {
  const base::Optional<std::string>& action_string_value =
      GetDeepLinkParam(params, DeepLinkParam::kAction);
  if (!action_string_value.has_value())
    return base::nullopt;

  if (action_string_value.value() == kAddTimeToTimer)
    return AlarmTimerAction::kAddTimeToTimer;

  if (action_string_value.value() == kStopAlarmTimerRinging)
    return AlarmTimerAction::kStopRinging;

  return base::nullopt;
}

base::Optional<int64_t> GetDeepLinkParamAsInt64(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param) {
  const base::Optional<std::string>& value = GetDeepLinkParam(params, param);
  if (value.has_value()) {
    int64_t result;
    if (base::StringToInt64(value.value(), &result))
      return result;
  }

  return base::nullopt;
}

base::Optional<base::TimeDelta> GetDeepLinkParamAsTimeDelta(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param) {
  if (param != DeepLinkParam::kDurationMs)
    return base::nullopt;

  const base::Optional<int64_t>& duration_ms =
      GetDeepLinkParamAsInt64(params, DeepLinkParam::kDurationMs);
  if (!duration_ms.has_value())
    return base::nullopt;

  return base::TimeDelta::FromMilliseconds(duration_ms.value());
}

bool IsDeepLinkType(const GURL& url, DeepLinkType type) {
  return GetDeepLinkType(url) == type;
}

bool IsDeepLinkUrl(const GURL& url) {
  return GetDeepLinkType(url) != DeepLinkType::kUnsupported;
}

base::Optional<GURL> GetAssistantUrl(DeepLinkType type,
                                     const base::Optional<std::string>& id) {
  std::string top_level_url;
  std::string by_id_url;

  switch (type) {
    case DeepLinkType::kLists:
      top_level_url =
          std::string("https://assistant.google.com/lists/mainview");
      by_id_url = std::string("https://assistant.google.com/lists/list/");
      break;
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
      return base::nullopt;
  }

  const std::string url =
      (id && !id.value().empty()) ? (by_id_url + id.value()) : top_level_url;

  // Source is currently assumed to be |Assistant|. If need be, we can make
  // |source| a deep link parameter in the future.
  constexpr char kDefaultSource[] = "Assistant";
  return net::AppendOrReplaceQueryParameter(CreateLocalizedGURL(url),
                                            kSourceParamKey, kDefaultSource);
}

GURL GetChromeSettingsUrl(const base::Optional<std::string>& page) {
  static constexpr char kChromeSettingsUrl[] = "chrome://settings/";

  // Note that we only allow deep linking to a subset of pages. If a deep link
  // requests a page not contained in this array, we fallback gracefully to
  // top-level Chrome Settings.
  static constexpr std::array<char[16], 2> kAllowedPages = {"googleAssistant",
                                                            "languages"};

  return page && std::find(kAllowedPages.begin(), kAllowedPages.end(),
                           page.value()) != kAllowedPages.end()
             ? GURL(kChromeSettingsUrl + page.value())
             : GURL(kChromeSettingsUrl);
}

base::Optional<GURL> GetWebUrl(const GURL& deep_link) {
  return GetWebUrl(GetDeepLinkType(deep_link), GetDeepLinkParams(deep_link));
}

base::Optional<GURL> GetWebUrl(
    DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  static constexpr char kAssistantSettingsWebUrl[] =
      "https://assistant.google.com/settings/mainpage";

  if (!IsWebDeepLinkType(type, params))
    return base::nullopt;

  switch (type) {
    case DeepLinkType::kLists:
    case DeepLinkType::kNotes:
    case DeepLinkType::kReminders: {
      const auto id = GetDeepLinkParam(params, DeepLinkParam::kId);
      return GetAssistantUrl(type, id);
    }
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
    case DeepLinkType::kWhatsOnMyScreen:
      NOTREACHED();
      return base::nullopt;
  }

  NOTREACHED();
  return base::nullopt;
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

}  // namespace util
}  // namespace assistant
}  // namespace ash
