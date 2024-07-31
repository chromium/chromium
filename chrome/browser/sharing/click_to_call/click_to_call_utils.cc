// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/click_to_call_utils.h"

#include <optional>

#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing/click_to_call/phone_number_regex.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "components/prefs/pref_service.h"
#include "components/sharing_message/features.h"
#include "components/sharing_message/pref_names.h"
#include "components/sharing_message/sharing_service.h"
#include "content/public/browser/browser_context.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/url_constants.h"
#include "url/url_util.h"

namespace {

// Upper bound on length of selection text to avoid performance issues. Take
// into account numbers with country code and spaces '+99 0 999 999 9999' when
// reducing the max length.
constexpr int kSelectionTextMaxLength = 30;

// Upper bound on digits in selected text to reduce false positives. This
// matches the maximum number of digits in phone numbers according to E.164 and
// showed a good tradeoff between false negatives vs. false positives.
constexpr int kSelectionTextMaxDigits = 15;

bool IsClickToCallEnabled(content::BrowserContext* browser_context) {
#if BUILDFLAG(IS_ANDROID)
  // We don't support sending phone numbers from Android.
  return false;
#else   // BUILDFLAG(IS_ANDROID)
  if (!base::FeatureList::IsEnabled(kClickToCall)) {
    return false;
  }

  // Check Chrome enterprise policy for Click to Call.
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (profile && !profile->GetPrefs()->GetBoolean(prefs::kClickToCallEnabled))
    return false;

  SharingService* sharing_service =
      SharingServiceFactory::GetForBrowserContext(browser_context);
  return sharing_service != nullptr;
#endif  // BUILDFLAG(IS_ANDROID)
}

// Returns the first possible phone number in |selection_text| given the
// |regex_variant| to be used or std::nullopt if the regex did not match.
std::optional<std::string> ExtractPhoneNumber(
    const std::string& selection_text) {
  std::string parsed_number;

  const re2::RE2& regex = GetPhoneNumberRegex();
  if (!re2::RE2::PartialMatch(selection_text, regex, &parsed_number))
    return std::nullopt;

  return base::UTF16ToUTF8(
      base::TrimWhitespace(base::UTF8ToUTF16(parsed_number), base::TRIM_ALL));
}

// Unescapes and returns the URL contents.
std::string GetUnescapedURLContent(const GURL& url) {
  std::string content_string(url.GetContent());
  url::RawCanonOutputT<char16_t> unescaped_content;
  url::DecodeURLEscapeSequences(content_string,
                                url::DecodeURLMode::kUTF8OrIsomorphic,
                                &unescaped_content);
  return base::UTF16ToUTF8(unescaped_content.view());
}

}  // namespace

bool ShouldOfferClickToCallForURL(content::BrowserContext* browser_context,
                                  const GURL& url) {
  return !url.is_empty() && url.SchemeIs(url::kTelScheme) &&
         IsUrlSafeForClickToCall(url) && IsClickToCallEnabled(browser_context);
}

std::optional<std::string> ExtractPhoneNumberForClickToCall(
    content::BrowserContext* browser_context,
    const std::string& selection_text) {
  DCHECK(!selection_text.empty());

  if (selection_text.size() > kSelectionTextMaxLength)
    return std::nullopt;

  // See https://en.cppreference.com/w/cpp/string/byte/isdigit for why this uses
  // unsigned char.
  int digits = base::ranges::count_if(
      selection_text, [](unsigned char c) { return absl::ascii_isdigit(c); });
  if (digits > kSelectionTextMaxDigits)
    return std::nullopt;

  if (!IsClickToCallEnabled(browser_context))
    return std::nullopt;

  return ExtractPhoneNumber(selection_text);
}

bool IsUrlSafeForClickToCall(const GURL& url) {
  // Get the unescaped content as this is what we'll end up sending to the
  // Android dialer.
  std::string unescaped = GetUnescapedURLContent(url);
  // We don't allow any number that contains any of these characters as they
  // might be used to create USSD codes.
  return !unescaped.empty() && base::ranges::none_of(unescaped, [](char c) {
    return c == '#' || c == '*' || c == '%';
  });
}
