// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/util/deep_link_util.h"

#include <array>
#include <set>

#include "base/i18n/rtl.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "net/base/escape.h"
#include "url/gurl.h"

namespace ash {
namespace assistant {
namespace util {

namespace {

// Supported deep link param keys. These values must be kept in sync with the
// server. See more details at go/cros-assistant-deeplink.
constexpr char kQueryParamKey[] = "q";
constexpr char kPageParamKey[] = "page";
constexpr char kRelaunchParamKey[] = "relaunch";

// Supported deep link prefixes. These values must be kept in sync with the
// server. See more details at go/cros-assistant-deeplink.
constexpr char kChromeSettingsPrefix[] = "googleassistant://chrome-settings";
constexpr char kAssistantFeedbackPrefix[] = "googleassistant://send-feedback";
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
      {DeepLinkParam::kPage, kPageParamKey},
      {DeepLinkParam::kQuery, kQueryParamKey},
      {DeepLinkParam::kRelaunch, kRelaunchParamKey}};

  const std::string& key = kDeepLinkParamKeys.at(param);
  const auto it = params.find(key);
  return it != params.end()
             ? base::Optional<std::string>(net::UnescapeURLComponent(
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

DeepLinkType GetDeepLinkType(const GURL& url) {
  // Map of supported deep link types to their prefixes.
  static const std::map<DeepLinkType, std::string> kSupportedDeepLinks = {
      {DeepLinkType::kChromeSettings, kChromeSettingsPrefix},
      {DeepLinkType::kFeedback, kAssistantFeedbackPrefix},
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

bool IsDeepLinkType(const GURL& url, DeepLinkType type) {
  return GetDeepLinkType(url) == type;
}

bool IsDeepLinkUrl(const GURL& url) {
  return GetDeepLinkType(url) != DeepLinkType::kUnsupported;
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
  return GetWebUrl(GetDeepLinkType(deep_link));
}

base::Optional<GURL> GetWebUrl(DeepLinkType type) {
  // TODO(b/113357196): Make these URLs configurable for development purposes.
  static constexpr char kAssistantRemindersWebUrl[] =
      "https://assistant.google.com/reminders/mainview?hl=";
  static constexpr char kAssistantSettingsWebUrl[] =
      "https://assistant.google.com/settings/mainpage?hl=";

  if (!IsWebDeepLinkType(type))
    return base::nullopt;

  switch (type) {
    case DeepLinkType::kReminders:
      return GURL(kAssistantRemindersWebUrl +
                  base::i18n::GetConfiguredLocale());
    case DeepLinkType::kSettings:
      return GURL(kAssistantSettingsWebUrl + base::i18n::GetConfiguredLocale());
    case DeepLinkType::kUnsupported:
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
  return IsWebDeepLinkType(GetDeepLinkType(deep_link));
}

bool IsWebDeepLinkType(DeepLinkType type) {
  // Set of deep link types which open web contents in the Assistant UI.
  static const std::set<DeepLinkType> kWebDeepLinks = {DeepLinkType::kReminders,
                                                       DeepLinkType::kSettings};

  return base::ContainsKey(kWebDeepLinks, type);
}

}  // namespace util
}  // namespace assistant
}  // namespace ash
