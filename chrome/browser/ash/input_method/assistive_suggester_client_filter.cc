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
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/exo/wm_helper.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace ash {
namespace input_method {
namespace {

const char* kAllowedDomainsForPersonalInfoSuggester[] = {
    "discord.com",      "messenger.com",       "web.whatsapp.com",
    "web.skype.com",    "duo.google.com",      "hangouts.google.com",
    "chat.google.com",  "messages.google.com", "web.telegram.org",
    "voice.google.com",
};

const char* kAllowedDomainsForEmojiSuggester[] = {
    "discord.com",      "messenger.com",       "web.whatsapp.com",
    "web.skype.com",    "duo.google.com",      "hangouts.google.com",
    "chat.google.com",  "messages.google.com", "web.telegram.org",
    "voice.google.com",
};

// TODO(b/3339115): Add web.skype.com back to the list after compatibility
//    issues are solved.
const char* kAllowedDomainsForMultiWordSuggester[] = {
    "discord.com",         "messenger.com",       "web.whatsapp.com",
    "duo.google.com",      "hangouts.google.com", "chat.google.com",
    "messages.google.com", "web.telegram.org",    "voice.google.com",
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

template <size_t N>
bool IsAllowedUrl(const char* (&allowedDomains)[N], GURL url) {
  if (IsTestUrl(url) || IsInternalWebsite(url))
    return true;
  for (size_t i = 0; i < N; i++) {
    if (url.DomainIs(allowedDomains[i])) {
      return true;
    }
  }
  return false;
}

template <size_t N>
bool IsAllowedUrlLegacy(const char* (&allowedDomains)[N]) {
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
      if (url.DomainIs(allowedDomains[i])) {
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
  return IsAllowedUrlLegacy(kAllowedDomainsForPersonalInfoSuggester) ||
         IsAllowedApp(kAllowedAppsForPersonalInfoSuggester);
}

bool IsAllowedUrlOrAppForEmojiSuggestion() {
  return IsAllowedUrlLegacy(kAllowedDomainsForEmojiSuggester) ||
         IsAllowedApp(kAllowedAppsForEmojiSuggester);
}

bool IsAllowedUrlOrAppForMultiWordSuggestion() {
  return IsAllowedUrlLegacy(kAllowedDomainsForMultiWordSuggester) ||
         IsAllowedApp(kAllowedAppsForMultiWordSuggester);
}

absl::optional<GURL> GetAshChromeUrl() {
  Browser* browser = chrome::FindLastActive();
  // Ash chrome will return true for browser->window()->IsActive() if the
  // user is currently typing in an ash browser tab. IsActive() will return
  // false if the user is currently typing a lacros browser tab.
  if (browser && browser->window() && browser->window()->IsActive() &&
      browser->tab_strip_model() &&
      browser->tab_strip_model()->GetActiveWebContents()) {
    return browser->tab_strip_model()
        ->GetActiveWebContents()
        ->GetLastCommittedURL();
  }

  return absl::nullopt;
}

using GetBrowserUrlCallback =
    base::OnceCallback<void(const absl::optional<GURL>& url)>;

void OnLacrosChromeUrlReturned(GetBrowserUrlCallback callback,
                               const absl::optional<GURL>& url) {
  std::move(callback).Run(std::move(url));
}

void GetLacrosChromeUrl(GetBrowserUrlCallback callback) {
  crosapi::BrowserManager* browser_manager = crosapi::BrowserManager::Get();
  // browser_manager will exist whenever there is a lacros browser running.
  // GetActiveTabUrlSupported() will only return true if the current lacros
  // browser is being used by the user.
  if (browser_manager && browser_manager->IsRunning() &&
      browser_manager->GetActiveTabUrlSupported()) {
    browser_manager->GetActiveTabUrl(
        base::BindOnce(&OnLacrosChromeUrlReturned, std::move(callback)));
    return;
  }

  std::move(callback).Run(absl::nullopt);
}

void GetBrowserUrl(GetBrowserUrlCallback callback) {
  absl::optional<GURL> ash_url = GetAshChromeUrl();
  if (ash_url.has_value()) {
    std::move(callback).Run(ash_url);
    return;
  }

  GetLacrosChromeUrl(std::move(callback));
}

void ReturnEnabledSuggestions(
    AssistiveSuggesterSwitch::GetEnabledSuggestionsCallback callback,
    const absl::optional<GURL>& current_url) {
  if (!current_url.has_value()) {
    std::move(callback).Run(AssistiveSuggesterSwitch::EnabledSuggestions{});
    return;
  }

  bool emoji_suggestions_allowed =
      IsAllowedUrl(kAllowedDomainsForEmojiSuggester, *current_url) ||
      IsAllowedApp(kAllowedAppsForEmojiSuggester);

  bool multi_word_suggestions_allowed =
      IsAllowedUrl(kAllowedDomainsForMultiWordSuggester, *current_url) ||
      IsAllowedApp(kAllowedAppsForMultiWordSuggester);

  bool personal_info_suggestions_allowed =
      IsAllowedUrl(kAllowedDomainsForPersonalInfoSuggester, *current_url) ||
      IsAllowedApp(kAllowedAppsForPersonalInfoSuggester);

  std::move(callback).Run(AssistiveSuggesterSwitch::EnabledSuggestions{
      .emoji_suggestions = emoji_suggestions_allowed,
      .multi_word_suggestions = multi_word_suggestions_allowed,
      .personal_info_suggestions = personal_info_suggestions_allowed,
  });
}

}  // namespace

AssistiveSuggesterClientFilter::AssistiveSuggesterClientFilter() = default;
AssistiveSuggesterClientFilter::~AssistiveSuggesterClientFilter() = default;

bool AssistiveSuggesterClientFilter::IsEmojiSuggestionAllowed() {
  return IsAllowedUrlOrAppForEmojiSuggestion();
}

bool AssistiveSuggesterClientFilter::IsMultiWordSuggestionAllowed() {
  return IsAllowedUrlOrAppForMultiWordSuggestion();
}

bool AssistiveSuggesterClientFilter::IsPersonalInfoSuggestionAllowed() {
  return IsAllowedUrlOrAppForPersonalInfoSuggestion();
}

void AssistiveSuggesterClientFilter::GetEnabledSuggestions(
    GetEnabledSuggestionsCallback callback) {
  GetBrowserUrl(base::BindOnce(ReturnEnabledSuggestions, std::move(callback)));
}

}  // namespace input_method
}  // namespace ash
