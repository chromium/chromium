// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_CLICK_TO_CALL_PHONE_NUMBER_REGEX_H_
#define CHROME_BROWSER_SHARING_CLICK_TO_CALL_PHONE_NUMBER_REGEX_H_

namespace re2 {
class RE2;
}  // namespace re2

// Returns an RE2 instance to detect phone numbers.
const re2::RE2& GetPhoneNumberRegex();

// Precompile regexes on a best effort task 15 seconds after startup.
void PrecompilePhoneNumberRegexesAsync();

#endif  // CHROME_BROWSER_SHARING_CLICK_TO_CALL_PHONE_NUMBER_REGEX_H_
