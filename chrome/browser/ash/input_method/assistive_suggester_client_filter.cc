// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/assistive_suggester_client_filter.h"

#include <algorithm>
#include <string>
#include <vector>

#include "ash/public/cpp/window_properties.h"
#include "base/callback.h"
#include "base/hash/hash.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/exo/wm_helper.h"
#include "url/gurl.h"

namespace ash {
namespace input_method {
namespace {

const char* kAllowedDomainAndPathsForPersonalInfoSuggester[][2] = {
    {"discord.com", ""},         {"messenger.com", ""},
    {"web.whatsapp.com", ""},    {"web.skype.com", ""},
    {"duo.google.com", ""},      {"hangouts.google.com", ""},
    {"messages.google.com", ""}, {"web.telegram.org", ""},
    {"voice.google.com", ""},    {"mail.google.com", "/chat"},
};

const char* kAllowedDomainAndPathsForEmojiSuggester[][2] = {
    {"discord.com", ""},         {"messenger.com", ""},
    {"web.whatsapp.com", ""},    {"web.skype.com", ""},
    {"duo.google.com", ""},      {"hangouts.google.com", ""},
    {"messages.google.com", ""}, {"web.telegram.org", ""},
    {"voice.google.com", ""},    {"mail.google.com", "/chat"},
};

// TODO(b/3339115): Add web.skype.com back to the list after compatibility
//    issues are solved.
const char* kAllowedDomainAndPathsForMultiWordSuggester[][2] = {
    {"discord.com", ""},          {"messenger.com", ""},
    {"web.whatsapp.com", ""},     {"duo.google.com", ""},
    {"hangouts.google.com", ""},  {"messages.google.com", ""},
    {"web.telegram.org", ""},     {"voice.google.com", ""},
    {"mail.google.com", "/chat"},
};

const char* kTestUrls[] = {
    "e14s-test",
    "simple_textarea.html",
    "test_page.html",
};

// For some internal websites, we do not want to reveal their urls in plain
// text. See map between url and hash code in
// https://docs.google.com/spreadsheets/d/1VELTWiHrUTEyX4HQI5PL_jDVFreM-lRhThVOurUuOk4/edit#gid=0
const uint32_t kHashedInternalUrls[] = {
    1845308025U,
    153302869U,
};

// For ARC++ apps, use arc package name. For system apps, use app ID.
const char* kAllowedAppsForPersonalInfoSuggester[] = {
    "com.discord",
    "com.facebook.orca",
    "com.whatsapp",
    "com.skype.raider",
    "com.google.android.apps.tachyon",
    "com.google.android.talk",
    "org.telegram.messenger",
    "com.enflick.android.TextNow",
    "com.facebook.mlite",
    "com.viber.voip",
    "com.skype.m2",
    "com.imo.android.imoim",
    "com.google.android.apps.googlevoice",
    "com.playstation.mobilemessenger",
    "kik.android",
    "com.link.messages.sms",
    "jp.naver.line.android",
    "com.skype.m2",
    "co.happybits.marcopolo",
    "com.imo.android.imous",
    "mmfbcljfglbokpmkimbfghdkjmjhdgbg",  // System text
};

// For ARC++ apps, use arc package name. For system apps, use app ID.
const char* kAllowedAppsForEmojiSuggester[] = {
    "com.discord",
    "com.facebook.orca",
    "com.whatsapp",
    "com.skype.raider",
    "com.google.android.apps.tachyon",
    "com.google.android.talk",
    "org.telegram.messenger",
    "com.enflick.android.TextNow",
    "com.facebook.mlite",
    "com.viber.voip",
    "com.skype.m2",
    "com.imo.android.imoim",
    "com.google.android.apps.googlevoice",
    "com.playstation.mobilemessenger",
    "kik.android",
    "com.link.messages.sms",
    "jp.naver.line.android",
    "com.skype.m2",
    "co.happybits.marcopolo",
    "com.imo.android.imous",
    "mmfbcljfglbokpmkimbfghdkjmjhdgbg",  // System text
};

// For ARC++ apps, use arc package name. For system apps, use app ID.
const char* kAllowedAppsForMultiWordSuggester[] = {
    "com.discord",
    "com.facebook.orca",
    "com.whatsapp",
    "com.skype.raider",
    "com.google.android.apps.tachyon",
    "com.google.android.talk",
    "org.telegram.messenger",
    "com.enflick.android.TextNow",
    "com.facebook.mlite",
    "com.viber.voip",
    "com.skype.m2",
    "com.imo.android.imoim",
    "com.google.android.apps.googlevoice",
    "com.playstation.mobilemessenger",
    "kik.android",
    "com.link.messages.sms",
    "jp.naver.line.android",
    "com.skype.m2",
    "co.happybits.marcopolo",
    "com.imo.android.imous",
    "mmfbcljfglbokpmkimbfghdkjmjhdgbg",  // System text
};

bool IsTestUrl(GURL url) {
  std::string filename = url.ExtractFileName();
  for (const char* test_url : kTestUrls) {
    if (base::CompareCaseInsensitiveASCII(filename, test_url) == 0) {
      return true;
    }
  }
  return false;
}

bool IsInternalWebsite(GURL url) {
  std::string host = url.host();
  for (const size_t hash_code : kHashedInternalUrls) {
    if (hash_code == base::PersistentHash(host)) {
      return true;
    }
  }
  return false;
}

bool AtDomainWithPathPrefix(GURL url,
                            const std::string& domain,
                            const std::string& prefix) {
  return url.DomainIs(domain) && url.has_path() &&
         base::StartsWith(url.path(), prefix);
}

template <size_t N>
bool IsAllowedUrlWithPathPrefix(const char* (&allowedDomainAndPaths)[N][2],
                                GURL url) {
  if (IsTestUrl(url) || IsInternalWebsite(url))
    return true;
  for (size_t i = 0; i < N; i++) {
    auto domain = allowedDomainAndPaths[i][0];
    auto path_prefix = allowedDomainAndPaths[i][1];
    if (AtDomainWithPathPrefix(url, domain, path_prefix)) {
      return true;
    }
  }
  return false;
}

template <size_t N>
bool IsAllowedUrlLegacy(const char* (&allowedDomainAndPaths)[N][2]) {
  Browser* browser = chrome::FindLastActive();
  if (browser && browser->window() && browser->window()->IsActive() &&
      browser->tab_strip_model() &&
      browser->tab_strip_model()->GetActiveWebContents()) {
    GURL url = browser->tab_strip_model()
                   ->GetActiveWebContents()
                   ->GetLastCommittedURL();
    if (IsTestUrl(url) || IsInternalWebsite(url))
      return true;
    for (size_t i = 0; i < N; i++) {
      auto domain = allowedDomainAndPaths[i][0];
      auto path_prefix = allowedDomainAndPaths[i][1];
      if (AtDomainWithPathPrefix(url, domain, path_prefix)) {
        return true;
      }
    }
  }
  return false;
}

template <size_t N>
bool IsAllowedApp(const char* (&allowedApps)[N]) {
  // WMHelper is not available in Chrome on Linux.
  if (!exo::WMHelper::HasInstance())
    return false;

  auto* wm_helper = exo::WMHelper::GetInstance();
  auto* window = wm_helper ? wm_helper->GetActiveWindow() : nullptr;
  if (!window)
    return false;

  // TODO(crbug/1094113): improve to cover more scenarios such as chat heads.
  const std::string* arc_package_name =
      window->GetProperty(ash::kArcPackageNameKey);
  if (arc_package_name && std::find(allowedApps, allowedApps + N,
                                    *arc_package_name) != allowedApps + N) {
    return true;
  }
  const std::string* app_id = window->GetProperty(ash::kAppIDKey);
  if (app_id &&
      std::find(allowedApps, allowedApps + N, *app_id) != allowedApps + N) {
    return true;
  }
  return false;
}

bool IsAllowedUrlOrAppForPersonalInfoSuggestion() {
  return IsAllowedUrlLegacy(kAllowedDomainAndPathsForPersonalInfoSuggester) ||
         IsAllowedApp(kAllowedAppsForPersonalInfoSuggester);
}

bool IsAllowedUrlOrAppForEmojiSuggestion() {
  return IsAllowedUrlLegacy(kAllowedDomainAndPathsForEmojiSuggester) ||
         IsAllowedApp(kAllowedAppsForEmojiSuggester);
}

bool IsAllowedUrlOrAppForMultiWordSuggestion() {
  return IsAllowedUrlLegacy(kAllowedDomainAndPathsForMultiWordSuggester) ||
         IsAllowedApp(kAllowedAppsForMultiWordSuggester);
}

void ReturnEnabledSuggestions(
    AssistiveSuggesterSwitch::FetchEnabledSuggestionsCallback callback,
    const absl::optional<GURL>& current_url) {
  if (!current_url.has_value()) {
    std::move(callback).Run(AssistiveSuggesterSwitch::EnabledSuggestions{});
    return;
  }

  bool emoji_suggestions_allowed =
      IsAllowedUrlWithPathPrefix(kAllowedDomainAndPathsForEmojiSuggester,
                                 *current_url) ||
      IsAllowedApp(kAllowedAppsForEmojiSuggester);

  bool multi_word_suggestions_allowed =
      IsAllowedUrlWithPathPrefix(kAllowedDomainAndPathsForMultiWordSuggester,
                                 *current_url) ||
      IsAllowedApp(kAllowedAppsForMultiWordSuggester);

  bool personal_info_suggestions_allowed =
      IsAllowedUrlWithPathPrefix(kAllowedDomainAndPathsForPersonalInfoSuggester,
                                 *current_url) ||
      IsAllowedApp(kAllowedAppsForPersonalInfoSuggester);

  std::move(callback).Run(AssistiveSuggesterSwitch::EnabledSuggestions{
      .emoji_suggestions = emoji_suggestions_allowed,
      .multi_word_suggestions = multi_word_suggestions_allowed,
      .personal_info_suggestions = personal_info_suggestions_allowed,
  });
}

}  // namespace

AssistiveSuggesterClientFilter::AssistiveSuggesterClientFilter(
    GetUrlCallback get_url)
    : get_url_(std::move(get_url)) {}

AssistiveSuggesterClientFilter::~AssistiveSuggesterClientFilter() = default;

void AssistiveSuggesterClientFilter::FetchEnabledSuggestionsThen(
    FetchEnabledSuggestionsCallback callback) {
  get_url_.Run(base::BindOnce(ReturnEnabledSuggestions, std::move(callback)));
}

}  // namespace input_method
}  // namespace ash
