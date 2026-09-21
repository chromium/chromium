// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/assistive_suggester_client_filter.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/window_properties.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/hash/hash.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/input_method/url_utils.h"
#include "components/exo/wm_helper.h"
#include "ui/base/ime/ash/text_input_method.h"
#include "ui/base/ime/text_input_type.h"
#include "url/gurl.h"

namespace ash {
namespace input_method {
namespace {

constexpr auto kAllowedDomainAndPathsForEmojiSuggester =
    std::to_array<std::array<std::string_view, 2>>({
        {{"discord.com", ""}},
        {{"messenger.com", ""}},
        {{"web.whatsapp.com", ""}},
        {{"web.skype.com", ""}},
        {{"duo.google.com", ""}},
        {{"hangouts.google.com", ""}},
        {{"messages.google.com", ""}},
        {{"web.telegram.org", ""}},
        {{"voice.google.com", ""}},
        {{"mail.google.com", "/chat"}},
    });

constexpr auto kTestUrls = std::to_array<std::string_view>({
    "e14s-test",
    "simple_textarea.html",
    "test_page.html",
});

// For some internal websites, we do not want to reveal their urls in plain
// text. See map between url and hash code in
// https://docs.google.com/spreadsheets/d/1VELTWiHrUTEyX4HQI5PL_jDVFreM-lRhThVOurUuOk4/edit#gid=0
constexpr auto kHashedInternalUrls = std::to_array<uint32_t>({
    1845308025U,
    153302869U,
});

// For ARC++ apps, use arc package name. For system apps, use app ID.
constexpr auto kAllowedAppsForEmojiSuggester = std::to_array<std::string_view>({
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
});

constexpr auto kDeniedUrlsForMultiwordSuggester =
    std::to_array<std::string_view>({
        "chrome-untrusted://crosh/",     // Crosh on Chrome browser
        "chrome-untrusted://terminal/",  // Terminal on Chrome browser
    });

constexpr auto kDeniedAppsForMultiwordSuggester =
    std::to_array<std::string_view>({
        "iodihamcpbpeioajjeobimgagajmlibd",  // SSH app
        "cgfnfgkafmcdkdgilmojlnaadileaach",  // Crosh app
        "fhicihalidkgcimdmhpohldehjmcabcf",  // Terminal app
        "mmfbcljfglbokpmkimbfghdkjmjhdgbg",  // System text
        "algkcnfjnajfhgimadimbjhmpaeohhln",  // SSH app (dev)
    });

constexpr auto kDeniedAppsForDiacritics = std::to_array<std::string_view>({
    "iodihamcpbpeioajjeobimgagajmlibd",  // SSH app
    "cgfnfgkafmcdkdgilmojlnaadileaach",  // Crosh app
    "fhicihalidkgcimdmhpohldehjmcabcf",  // Terminal app
    "mmfbcljfglbokpmkimbfghdkjmjhdgbg",  // System text
    "algkcnfjnajfhgimadimbjhmpaeohhln",  // SSH app (dev)
});

constexpr auto kDeniedUrlsForDiacritics = std::to_array<std::string_view>({
    "chrome-untrusted://crosh/",     // Crosh app
    "chrome-untrusted://terminal/",  // Terminal app
});

constexpr auto kDeniedDomainsForDiacritics = std::to_array<std::string_view>({
    "localhost",            // Lots of dev apps on localhost (e.g. code-server)
    "cider.corp.google",    // Cider
    "cider-v.corp.google",  // Cider-v
});

bool IsTestUrl(const std::optional<GURL>& url) {
  if (!url) {
    return false;
  }
  std::string filename = url->ExtractFileName();
  for (std::string_view test_url : kTestUrls) {
    if (base::CompareCaseInsensitiveASCII(filename, test_url) == 0) {
      return true;
    }
  }
  return false;
}

bool IsInternalWebsite(const std::optional<GURL>& url) {
  if (!url) {
    return false;
  }
  std::string host = url->GetHost();
  for (const size_t hash_code : kHashedInternalUrls) {
    if (hash_code == base::PersistentHash(host)) {
      return true;
    }
  }
  return false;
}

bool AtDomainWithPathPrefix(const std::optional<GURL>& url,
                            std::string_view domain,
                            std::string_view prefix) {
  if (!url) {
    return false;
  }
  return url->DomainIs(domain) && url->has_path() &&
         base::StartsWith(url->GetPath(), prefix);
}

bool IsMatchedUrlWithPathPrefix(
    base::span<const std::array<std::string_view, 2>>
        expected_domains_and_paths,
    const std::optional<GURL>& url) {
  if (!url) {
    return false;
  }
  for (const auto& [domain, path_prefix] : expected_domains_and_paths) {
    if (AtDomainWithPathPrefix(url, domain, path_prefix)) {
      return true;
    }
  }
  return false;
}

bool IsMatchedExactUrl(base::span<const std::string_view> expected_urls,
                       const std::optional<GURL>& url) {
  if (!url) {
    return false;
  }
  for (std::string_view expected_url : expected_urls) {
    if (base::CompareCaseInsensitiveASCII(url->spec(), expected_url) == 0) {
      return true;
    }
  }
  return false;
}

bool IsMatchedApp(
    base::span<const std::string_view> expected_app_ids_or_package_names,
    WindowProperties w) {
  if (!w.arc_package_name.empty() &&
      std::ranges::find(expected_app_ids_or_package_names,
                        w.arc_package_name) !=
          expected_app_ids_or_package_names.end()) {
    return true;
  }
  if (!w.app_id.empty() &&
      std::ranges::find(expected_app_ids_or_package_names, w.app_id) !=
          expected_app_ids_or_package_names.end()) {
    return true;
  }
  return false;
}

bool IsMatchedSubDomain(base::span<const std::string_view> expected_domains,
                        const std::optional<GURL>& url) {
  if (!url.has_value()) {
    return false;
  }
  for (std::string_view domain : expected_domains) {
    if (IsSubDomain(*url, domain)) {
      return true;
    }
  }
  return false;
}

}  // namespace

AssistiveSuggesterClientFilter::AssistiveSuggesterClientFilter(
    GetUrlCallback get_url,
    GetFocusedWindowPropertiesCallback get_window_properties)
    : get_url_(std::move(get_url)),
      get_window_properties_(std::move(get_window_properties)) {}

AssistiveSuggesterClientFilter::~AssistiveSuggesterClientFilter() = default;

void AssistiveSuggesterClientFilter::FetchEnabledSuggestionsThen(
    FetchEnabledSuggestionsCallback callback,
    const TextInputMethod::InputContext& context) {
  WindowProperties window_properties = get_window_properties_.Run();
  std::optional<GURL> current_url = get_url_.Run();

  // Deny-list (will block if matched, otherwise allow)
  bool diacritic_suggestions_allowed =
      !IsMatchedSubDomain(kDeniedDomainsForDiacritics, current_url) &&
      !IsMatchedApp(kDeniedAppsForDiacritics, window_properties) &&
      !IsMatchedExactUrl(kDeniedUrlsForDiacritics, current_url) &&
      // Disable in P/W and number fields
      !(context.type == ui::TEXT_INPUT_TYPE_PASSWORD ||
        context.type == ui::TEXT_INPUT_TYPE_NUMBER);

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

  // Deny-list (will block if matched, otherwise allow)
  bool multi_word_suggestions_allowed =
      !denylist_.Contains(*current_url) &&
      !IsMatchedApp(kDeniedAppsForMultiwordSuggester, window_properties) &&
      !IsMatchedExactUrl(kDeniedUrlsForMultiwordSuggester, current_url);

  std::move(callback).Run(AssistiveSuggesterSwitch::EnabledSuggestions{
      .emoji_suggestions = emoji_suggestions_allowed,
      .multi_word_suggestions = multi_word_suggestions_allowed,
      .diacritic_suggestions = diacritic_suggestions_allowed,
  });
}

}  // namespace input_method
}  // namespace ash
