// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/click_to_call_utils.h"

#include <algorithm>
#include <cctype>

#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing/click_to_call/phone_number_regex.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
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
  // Check Chrome enterprise policy for Click to Call.
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (profile && !profile->GetPrefs()->GetBoolean(prefs::kClickToCallEnabled))
    return false;

  SharingService* sharing_service =
      SharingServiceFactory::GetForBrowserContext(browser_context);
  return sharing_service != nullptr;
}

}  // namespace

bool ShouldOfferClickToCallForURL(content::BrowserContext* browser_context,
                                  const GURL& url) {
  return !url.is_empty() && url.SchemeIs(url::kTelScheme) &&
         !url.GetContent().empty() && IsClickToCallEnabled(browser_context);
}

base::Optional<std::string> ExtractPhoneNumberForClickToCall(
    content::BrowserContext* browser_context,
    const std::string& selection_text) {
  DCHECK(!selection_text.empty());

  if (selection_text.size() > kSelectionTextMaxLength)
    return base::nullopt;

  int digits = std::count_if(selection_text.begin(), selection_text.end(),
                             [](char c) { return std::isdigit(c); });
  if (digits > kSelectionTextMaxDigits)
    return base::nullopt;

  if (!IsClickToCallEnabled(browser_context))
    return base::nullopt;

  return ExtractPhoneNumber(selection_text);
}

base::Optional<std::string> ExtractPhoneNumber(
    const std::string& selection_text) {
  std::string parsed_number;

  const re2::RE2& regex = GetPhoneNumberRegex();
  if (!re2::RE2::PartialMatch(selection_text, regex, &parsed_number))
    return base::nullopt;

  return base::UTF16ToUTF8(
      base::TrimWhitespace(base::UTF8ToUTF16(parsed_number), base::TRIM_ALL));
}

std::string GetUnescapedURLContent(const GURL& url) {
  std::string content_string(url.GetContent());
  url::RawCanonOutputT<char16_t> unescaped_content;
  url::DecodeURLEscapeSequences(content_string.data(), content_string.size(),
                                url::DecodeURLMode::kUTF8OrIsomorphic,
                                &unescaped_content);
  return base::UTF16ToUTF8(
      std::u16string(unescaped_content.data(), unescaped_content.length()));
}
