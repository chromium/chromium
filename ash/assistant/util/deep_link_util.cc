// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/util/deep_link_util.h"

#include <set>

#include "ash/assistant/util/i18n_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace ash {
namespace assistant {
namespace util {

namespace {

using chromeos::assistant::AssistantEntryPoint;
using chromeos::assistant::AssistantQuerySource;

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

// Supported proactive suggestions action deep link param values.
constexpr char kCardClick[] = "cardClick";
constexpr char kEntryPointClick[] = "entryPointClick";
constexpr char kEntryPointClose[] = "entryPointClose";
constexpr char kViewImpression[] = "viewImpression";

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
constexpr char kAssistantProactiveSuggestionsPrefix[] =
    "googleassistant://proactive-suggestions";
constexpr char kAssistantQueryPrefix[] = "googleassistant://send-query";
constexpr char kAssistantRemindersPrefix[] = "googleassistant://reminders";
constexpr char kAssistantScreenshotPrefix[] =
    "googleassistant://take-screenshot";
constexpr char kAssistantSettingsPrefix[] = "googleassistant://settings";
constexpr char kAssistantTaskManagerPrefix[] = "googleassistant://task-manager";
constexpr char kAssistantWhatsOnMyScreenPrefix[] =
    "googleassistant://whats-on-my-screen";

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
  return std::string();
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
  return std::string();
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

base::Optional<GURL> CreateAlarmTimerDeepLink(
    AlarmTimerAction action,
    base::Optional<std::string> alarm_timer_id,
    base::Optional<base::TimeDelta> duration) {
  switch (action) {
    case assistant::util::AlarmTimerAction::kAddTimeToTimer:
      DCHECK(alarm_timer_id.has_value() && duration.has_value());
      if (!alarm_timer_id.has_value() || !duration.has_value())
        return base::nullopt;
      break;
    case assistant::util::AlarmTimerAction::kPauseTimer:
    case assistant::util::AlarmTimerAction::kRemoveAlarmOrTimer:
    case assistant::util::AlarmTimerAction::kResumeTimer:
      DCHECK(alarm_timer_id.has_value() && !duration.has_value());
      if (!alarm_timer_id.has_value() || duration.has_value())
        return base::nullopt;
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
  const std::string key = GetDeepLinkParamKey(param);
  const auto it = params.find(key);
  return it != params.end()
             ? base::Optional<std::string>(net::UnescapeBinaryURLComponent(
                   it->second, net::UnescapeRule::REPLACE_PLUS_WITH_SPACE))
             : base::nullopt;
}

base::Optional<AlarmTimerAction> GetDeepLinkParamAsAlarmTimerAction(
    const std::map<std::string, std::string>& params) {
  const base::Optional<std::string>& action_string_value =
      GetDeepLinkParam(params, DeepLinkParam::kAction);
  if (!action_string_value.has_value())
    return base::nullopt;

  if (action_string_value.value() == kAddTimeToTimer)
    return AlarmTimerAction::kAddTimeToTimer;

  if (action_string_value.value() == kPauseTimer)
    return AlarmTimerAction::kPauseTimer;

  if (action_string_value.value() == kRemoveAlarmOrTimer)
    return AlarmTimerAction::kRemoveAlarmOrTimer;

  if (action_string_value.value() == kResumeTimer)
    return AlarmTimerAction::kResumeTimer;

  return base::nullopt;
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

base::Optional<AssistantEntryPoint> GetDeepLinkParamAsEntryPoint(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param) {
  const base::Optional<int> value = GetDeepLinkParamAsInt(params, param);
  if (!value.has_value() || (value.value() < 0) ||
      (value.value() > static_cast<int>(AssistantEntryPoint::kMaxValue))) {
    return base::nullopt;
  }
  return static_cast<AssistantEntryPoint>(value.value());
}

base::Optional<GURL> GetDeepLinkParamAsGURL(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param) {
  const base::Optional<std::string>& spec = GetDeepLinkParam(params, param);
  return spec.has_value() ? base::Optional<GURL>(spec.value()) : base::nullopt;
}

base::Optional<int> GetDeepLinkParamAsInt(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param) {
  const base::Optional<std::string>& value = GetDeepLinkParam(params, param);
  if (value.has_value()) {
    int result;
    if (base::StringToInt(value.value(), &result))
      return result;
  }

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

base::Optional<ProactiveSuggestionsAction>
GetDeepLinkParamAsProactiveSuggestionsAction(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param) {
  const base::Optional<std::string>& value = GetDeepLinkParam(params, param);
  if (value == kCardClick)
    return ProactiveSuggestionsAction::kCardClick;
  if (value == kEntryPointClick)
    return ProactiveSuggestionsAction::kEntryPointClick;
  if (value == kEntryPointClose)
    return ProactiveSuggestionsAction::kEntryPointClose;
  if (value == kViewImpression)
    return ProactiveSuggestionsAction::kViewImpression;
  return base::nullopt;
}

base::Optional<AssistantQuerySource> GetDeepLinkParamAsQuerySource(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param) {
  const base::Optional<int> value = GetDeepLinkParamAsInt(params, param);
  if (!value.has_value() || (value.value() < 0) ||
      (value.value() > static_cast<int>(AssistantQuerySource::kMaxValue))) {
    return base::nullopt;
  }
  return static_cast<AssistantQuerySource>(value.value());
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

DeepLinkType GetDeepLinkType(const GURL& url) {
  // Map of supported deep link types to their prefixes.
  static const std::map<DeepLinkType, std::string> kSupportedDeepLinks = {
      {DeepLinkType::kAlarmTimer, kAssistantAlarmTimerPrefix},
      {DeepLinkType::kChromeSettings, kChromeSettingsPrefix},
      {DeepLinkType::kFeedback, kAssistantFeedbackPrefix},
      {DeepLinkType::kLists, kAssistantListsPrefix},
      {DeepLinkType::kNotes, kAssistantNotesPrefix},
      {DeepLinkType::kOnboarding, kAssistantOnboardingPrefix},
      {DeepLinkType::kProactiveSuggestions,
       kAssistantProactiveSuggestionsPrefix},
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

bool IsDeepLinkType(const GURL& url, DeepLinkType type) {
  return GetDeepLinkType(url) == type;
}

bool IsDeepLinkUrl(const GURL& url) {
  return GetDeepLinkType(url) != DeepLinkType::kUnsupported;
}

base::Optional<GURL> GetAssistantUrl(
    DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  std::string top_level_url;
  std::string by_id_url;

  switch (type) {
    case DeepLinkType::kLists: {
      const auto& type = GetDeepLinkParam(params, DeepLinkParam::kType);
      top_level_url =
          std::string("https://assistant.google.com/lists/mainview");
      by_id_url = (type && type.value().compare("shopping") == 0)
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
      return base::nullopt;
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

GURL GetChromeSettingsUrl(const base::Optional<std::string>& page) {
  static constexpr char kChromeOsSettingsUrl[] = "chrome://os-settings/";

  // Note that we only allow deep linking to a subset of pages. If a deep link
  // requests a page not contained in this map, we fallback gracefully to
  // top-level Chrome OS Settings. We may wish to allow deep linking into
  // Browser Settings at some point in the future at which point we will define
  // an analogous collection of |kAllowedBrowserPages|.
  // These values are copied from
  // chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.
  // We can not reuse the generated defines as we can not depend on //chrome.
  // TODO(b/168138594): use generated defines once that header has been moved to
  // chromeos.
  static const std::map<std::string, std::string> kAllowedOsPages = {
      {/*page=*/"googleAssistant", /*os_page=*/"googleAssistant"},
      {/*page=*/"languages", /*os_page=*/"osLanguages/details"}};

  return page && base::Contains(kAllowedOsPages, page.value())
             ? GURL(kChromeOsSettingsUrl + kAllowedOsPages.at(page.value()))
             : GURL(kChromeOsSettingsUrl);
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
    case DeepLinkType::kReminders:
      return GetAssistantUrl(type, params);
    case DeepLinkType::kSettings:
      return CreateLocalizedGURL(kAssistantSettingsWebUrl);
    case DeepLinkType::kUnsupported:
    case DeepLinkType::kAlarmTimer:
    case DeepLinkType::kChromeSettings:
    case DeepLinkType::kFeedback:
    case DeepLinkType::kOnboarding:
    case DeepLinkType::kProactiveSuggestions:
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
