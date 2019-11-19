// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_CLICK_TO_CALL_PHONE_NUMBER_REGEX_H_
#define CHROME_BROWSER_SHARING_CLICK_TO_CALL_PHONE_NUMBER_REGEX_H_

#include "chrome/browser/sharing/sharing_metrics.h"
#include "third_party/re2/src/re2/re2.h"

// Returns an RE2 instance for the given |variant| to detect phone numbers.
const re2::RE2& GetPhoneNumberRegex(PhoneNumberRegexVariant variant);

#endif  // CHROME_BROWSER_SHARING_CLICK_TO_CALL_PHONE_NUMBER_REGEX_H_
