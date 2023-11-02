// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <vector>

#include "ash/public/cpp/window_properties.h"
#include "base/callback.h"
#include "base/hash/hash.h"
#include "chrome/browser/ash/input_method/assistive_suggester_client_filter.h"
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

const char* kDeniedDomainAndPathsForDiacritics[][2] = {
    // Google Slides: delete on insert does not work
    {"docs.google.com", "/presentation"},
    // Google Docs: delete on insert does not work
    {"docs.google.com", "/document"},
};

const char* kDeniedAppsForDiacritics[] = {
    "iodihamcpbpeioajjeobimgagajmlibd",  // SSH app
    "cgfnfgkafmcdkdgilmojlnaadileaach",  // Crosh app
    "fhicihalidkgcimdmhpohldehjmcabcf",  // Terminal app
    "mmfbcljfglbokpmkimbfghdkjmjhdgbg",  // System text
    "algkcnfjnajfhgimadimbjhmpaeohhln",  // SSH app (dev)
};

const char* kDeniedUrlsForDiacritics[] = {
    "chrome-untrusted://crosh/",     // Crosh app
    "chrome-untrusted://terminal/",  // Terminal app
};

bool IsTestUrl(const absl::optional<GURL>& url) {
  if (!url)
    return false;
  std::string filename = url->ExtractFileName();
  for (const char* test_url : kTestUrls) {
    if (base::CompareCaseInsensitiveASCII(filename, test_url) == 0) {
      return true;
    }
  }
  return false;
}

bool IsInternalWebsite(const absl::optional<GURL>& url) {
  if (!url)
    return false;
  std::string host = url->host();
  for (const size_t hash_code : kHashedInternalUrls) {
    if (hash_code == base::PersistentHash(host)) {
      return true;
    }
  }
  return false;
}

bool AtDomainWithPathPrefix(const absl::optional<GURL>& url,
                            const std::string& domain,
                            const std::string& prefix) {
  if (!url)
    return false;
  return url->DomainIs(domain) && url->has_path() &&
         base::StartsWith(url->path(), prefix);
}

template <size_t N>
bool IsMatchedUrlWithPathPrefix(const char* (&expected_domains_and_paths)[N][2],
                                const absl::optional<GURL>& url) {
  if (!url)
    return false;
  for (size_t i = 0; i < N; i++) {
    auto domain = expected_domains_and_paths[i][0];
    auto path_prefix = expected_domains_and_paths[i][1];
    if (AtDomainWithPathPrefix(url, domain, path_prefix)) {
      return true;
    }
  }
  return false;
}

template <size_t N>
bool IsMatchedExactUrl(const char* (&expected_urls)[N],
                       const absl::optional<GURL>& url) {
  if (!url)
    return false;
  for (size_t i = 0; i < N; i++) {
    auto expected_url = expected_urls[i];
    if (base::CompareCaseInsensitiveASCII(url->spec(), expected_url) == 0) {
      return true;
    }
  }
  return false;
}

template <size_t N>
bool IsMatchedApp(const char* (&expected_app_ids_or_package_names)[N],
                  WindowProperties w) {
  if (!w.arc_package_name.empty() &&
      std::find(expected_app_ids_or_package_names,
                expected_app_ids_or_package_names + N,
                w.arc_package_name) != expected_app_ids_or_package_names + N) {
    return true;
  }
  if (!w.app_id.empty() &&
      std::find(expected_app_ids_or_package_names,
                expected_app_ids_or_package_names + N,
                w.app_id) != expected_app_ids_or_package_names + N) {
    return true;
  }
  return false;
}

void ReturnEnabledSuggestions(
    AssistiveSuggesterSwitch::FetchEnabledSuggestionsCallback callback,
    WindowProperties window_properties,
    const absl::optional<GURL>& current_url) {
  // Deny-list (will block if matched, otherwise allow)
  bool diacritic_suggestions_allowed =
      !IsMatchedApp(kDeniedAppsForDiacritics, window_properties) &&
      !IsMatchedUrlWithPathPrefix(kDeniedDomainAndPathsForDiacritics,
                                  current_url) &&
      !IsMatchedExactUrl(kDeniedUrlsForDiacritics, current_url);

  // TODO(b/245469813): Investigate if denied is intentional for suggesters
  // below is intentional.
  if (!current_url.has_value()) {
    std::move(callback).Run(AssistiveSuggesterSwitch::EnabledSuggestions{
        .diacritic_suggestions = diacritic_suggestions_allowed});
    return;
  }

  // Allow-list (will only allow if matched)
  bool emoji_suggestions_allowed =
      IsTestUrl(current_url) || IsInternalWebsite(current_url) ||
      IsMatchedUrlWithPathPrefix(kAllowedDomainAndPathsForEmojiSuggester,
                                 current_url) ||
      IsMatchedApp(kAllowedAppsForEmojiSuggester, window_properties);

  // Allow-list (will only allow if matched)
  bool multi_word_suggestions_allowed =
      IsTestUrl(current_url) || IsInternalWebsite(current_url) ||
      IsMatchedUrlWithPathPrefix(kAllowedDomainAndPathsForMultiWordSuggester,
                                 current_url) ||
      IsMatchedApp(kAllowedAppsForMultiWordSuggester, window_properties);

  // Allow-list (will only allow if matched)
  bool personal_info_suggestions_allowed =
      IsTestUrl(current_url) || IsInternalWebsite(current_url) ||
      IsMatchedUrlWithPathPrefix(kAllowedDomainAndPathsForPersonalInfoSuggester,
                                 current_url) ||
      IsMatchedApp(kAllowedAppsForPersonalInfoSuggester, window_properties);

  std::move(callback).Run(AssistiveSuggesterSwitch::EnabledSuggestions{
      .emoji_suggestions = emoji_suggestions_allowed,
      .multi_word_suggestions = multi_word_suggestions_allowed,
      .personal_info_suggestions = personal_info_suggestions_allowed,
      .diacritic_suggestions = diacritic_suggestions_allowed,
  });
}

}  // namespace

AssistiveSuggesterClientFilter::AssistiveSuggesterClientFilter(
    GetUrlCallback get_url,
    GetFocusedWindowPropertiesCallback get_window_properties)
    : get_url_(std::move(get_url)),
      get_window_properties_(std::move(get_window_properties)) {}

AssistiveSuggesterClientFilter::~AssistiveSuggesterClientFilter() = default;

void AssistiveSuggesterClientFilter::FetchEnabledSuggestionsThen(
    FetchEnabledSuggestionsCallback callback) {
  WindowProperties window_properties = get_window_properties_.Run();
  get_url_.Run(base::BindOnce(ReturnEnabledSuggestions, std::move(callback),
                              window_properties));
}

}  // namespace input_method
}  // namespace ash
