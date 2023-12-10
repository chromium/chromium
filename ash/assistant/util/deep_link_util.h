// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UTIL_DEEP_LINK_UTIL_H_
#define ASH_ASSISTANT_UTIL_DEEP_LINK_UTIL_H_

#include <map>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/timer/timer.h"

class GURL;

namespace ash {
namespace assistant {

enum class AssistantEntryPoint;
enum class AssistantQuerySource;

namespace util {

// Enumeration of deep link types.
enum class DeepLinkType {
  kUnsupported,
  kAlarmTimer,
  kChromeSettings,
  kFeedback,
  kLists,
  kNotes,
  kOnboarding,
  kQuery,
  kReminders,
  kScreenshot,
  kSettings,
  kTaskManager,
};

// Enumeration of deep link parameters.
// Examples of usage in comments. Note that actual Assistant deeplinks are
// prefixed w/ "googleassistant"; "ga" is only used here to avoid line wrapping.
enum class DeepLinkParam {
  kAction,      // ga://proactive-suggestions?action=cardClick
  kCategory,    // ga://proactive-suggestions?category=1
  kClientId,    // ga://reminders?action=edit&clientId=1
  kDurationMs,  // ga://alarm-timer?action=addTimeToTimer&durationMs=60000
  kEid,         // ga://lists?eid=1
  kEntryPoint,  // ga://send-query?q=weather&entryPoint=11
  kHref,   // ga://proactive-suggestions?action=cardClick&href=https://g.co/
  kIndex,  // ga://proactive-suggestions?action=cardClick&index=1
  kId,     // ga://alarm-timer?action=addTimeToTimer&id=1
  kPage,   // ga://settings?page=googleAssistant
  kQuery,  // ga://send-query?q=weather
  kQuerySource,  // ga://send-query?q=weather&querySource=12
  kRelaunch,     // ga://onboarding?relaunch=true
  kType,         // ga://lists?id=1&type=shopping
  kVeId,         // ga://proactive-suggestions?action=cardClick&veId=1
};

// Enumeration of alarm/timer deep link actions.
enum class AlarmTimerAction {
  kAddTimeToTimer,
  kPauseTimer,
  kRemoveAlarmOrTimer,
  kResumeTimer,
};

// Enumeration of reminder deep link actions.
enum class ReminderAction {
  kCreate,
  kEdit,
};

// Returns a new deep link, having appended or replaced the entry point param
// from the original |deep_link| with |entry_point|.
COMPONENT_EXPORT(ASSISTANT_UTIL)
GURL AppendOrReplaceEntryPointParam(const GURL& deep_link,
                                    AssistantEntryPoint entry_point);

// Returns a new deep link, having appended or replaced the query source param
// from the original |deep_link| with |query_source|.
COMPONENT_EXPORT(ASSISTANT_UTIL)
GURL AppendOrReplaceQuerySourceParam(const GURL& deep_link,
                                     AssistantQuerySource query_source);

// Returns a deep link to perform an alarm/timer action.
COMPONENT_EXPORT(ASSISTANT_UTIL)
std::optional<GURL> CreateAlarmTimerDeepLink(
    AlarmTimerAction action,
    std::optional<std::string> alarm_timer_id,
    std::optional<base::TimeDelta> duration = std::nullopt);

// Returns a deep link to send an Assistant query.
COMPONENT_EXPORT(ASSISTANT_UTIL)
GURL CreateAssistantQueryDeepLink(const std::string& query);

// Returns a deep link to top level Assistant Settings.
COMPONENT_EXPORT(ASSISTANT_UTIL) GURL CreateAssistantSettingsDeepLink();

// Returns the parsed parameters for the specified |deep_link|. If the supplied
// argument is not a supported deep link or if no parameters are found, an empty
// map is returned.
COMPONENT_EXPORT(ASSISTANT_UTIL)
std::map<std::string, std::string> GetDeepLinkParams(const GURL& deep_link);

// Returns a specific string |param| from the given parameters. If the desired
// parameter is not found, and empty value is returned.
COMPONENT_EXPORT(ASSISTANT_UTIL)
std::optional<std::string> GetDeepLinkParam(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param);

// Returns AlarmTimerAction from the given parameters. If the desired
// parameter is not found or is not an AlarmTimerAction, an empty value is
// returned.
COMPONENT_EXPORT(ASSISTANT_UTIL)
std::optional<AlarmTimerAction> GetDeepLinkParamAsAlarmTimerAction(
    const std::map<std::string, std::string>& params);

// Returns a specific bool |param| from the given parameters. If the desired
// parameter is not found or is not a bool, an empty value is returned.
COMPONENT_EXPORT(ASSISTANT_UTIL)
std::optional<bool> GetDeepLinkParamAsBool(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param);

// Returns a specific entry point |param| from the given parameters. If the
// desired parameter is not found or is not mappable to an Assistant entry
// point, an empty value is returned.
COMPONENT_EXPORT(ASSISTANT_UTIL)
std::optional<AssistantEntryPoint> GetDeepLinkParamAsEntryPoint(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param);

// Returns a specific GURL |param| from the given parameters. If the desired
// parameter is not found, an absent value is returned.
COMPONENT_EXPORT(ASSISTANT_UTIL)
std::optional<GURL> GetDeepLinkParamAsGURL(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param);

// Returns a specific int |param| from the given parameters. If the desired
// parameter is not found or is not an int, an empty value is returned.
COMPONENT_EXPORT(ASSISTANT_UTIL)
std::optional<int32_t> GetDeepLinkParamAsInt(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param);

// Returns a specific int64 |param| from the given parameters. If the desired
// parameter is not found or is not an int64, an empty value is returned.
COMPONENT_EXPORT(ASSISTANT_UTIL)
std::optional<int64_t> GetDeepLinkParamAsInt64(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param);

// Returns a specific query source |param| from the given parameters. If the
// desired parameter is not found or is not mappable to an Assistant query
// source, an empty value is returned.
COMPONENT_EXPORT(ASSISTANT_UTIL)
std::optional<AssistantQuerySource> GetDeepLinkParamAsQuerySource(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param);

// Returns a specific ReminderAction |param| from the given parameters. If the
// desired parameter is not found, an empty value is returned.
COMPONENT_EXPORT(ASSISTANT_UTIL)
std::optional<ReminderAction> GetDeepLinkParamAsRemindersAction(
    const std::map<std::string, std::string> params,
    DeepLinkParam param);

// Returns TimeDelta from the given parameters. If the desired parameter is not
// found, can't convert to TimeDelta or not a time type parameter, an empty
// value is returned.
COMPONENT_EXPORT(ASSISTANT_UTIL)
std::optional<base::TimeDelta> GetDeepLinkParamAsTimeDelta(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param);

// Returns the deep link type of the specified |url|. If the specified url is
// not a supported deep link, DeepLinkType::kUnsupported is returned.
COMPONENT_EXPORT(ASSISTANT_UTIL) DeepLinkType GetDeepLinkType(const GURL& url);

// Returns true if the specified |url| is a deep link of the given |type|.
COMPONENT_EXPORT(ASSISTANT_UTIL)
bool IsDeepLinkType(const GURL& url, DeepLinkType type);

// Returns true if the specified |url| is a deep link, false otherwise.
COMPONENT_EXPORT(ASSISTANT_UTIL) bool IsDeepLinkUrl(const GURL& url);

// Returns the Assistant URL for the deep link of the specified |type|. A return
// value will only be present if the deep link type is one of {kLists, kNotes,
// or kReminders}. If |id| is not contained in |params|, the returned URL will
// be for the top-level Assistant URL. Otherwise, the URL will correspond to
// the resource identified by |id|.
COMPONENT_EXPORT(ASSISTANT_UTIL)
std::optional<GURL> GetAssistantUrl(
    DeepLinkType type,
    const std::map<std::string, std::string>& params);

// Returns the URL for the specified Chrome Settings |page|. If page is absent
// or not allowed, the URL will be for top-level Chrome Settings.
COMPONENT_EXPORT(ASSISTANT_UTIL)
GURL GetChromeSettingsUrl(const std::optional<std::string>& page);

// Returns the web URL for the specified |deep_link|. A return value will only
// be present if |deep_link| is a web deep link as identified by the
// IsWebDeepLink(GURL) API.
COMPONENT_EXPORT(ASSISTANT_UTIL)
std::optional<GURL> GetWebUrl(const GURL& deep_link);

// Returns the web URL for a deep link of the specified |type| with the given
// |params|. A return value will only be present if the deep link type is a web
// deep link type as identified by the IsWebDeepLinkType(DeepLinkType) API.
COMPONENT_EXPORT(ASSISTANT_UTIL)
std::optional<GURL> GetWebUrl(DeepLinkType type,
                              const std::map<std::string, std::string>& params);

// Returns true if the specified |deep_link| is a web deep link.
COMPONENT_EXPORT(ASSISTANT_UTIL) bool IsWebDeepLink(const GURL& deep_link);

// Returns true if the specified deep link |type| is a web deep link.
COMPONENT_EXPORT(ASSISTANT_UTIL)
bool IsWebDeepLinkType(DeepLinkType type,
                       const std::map<std::string, std::string>& params);

}  // namespace util
}  // namespace assistant
}  // namespace ash

#endif  // ASH_ASSISTANT_UTIL_DEEP_LINK_UTIL_H_
