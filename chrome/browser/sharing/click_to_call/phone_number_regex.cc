// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/phone_number_regex.h"

namespace {

// Heuristical regex to search for phone number.
// (^|\p{Z}) makes sure the pattern begins with a new word.
// (\(?\+[0-9]+\)?) checks for optional international code in number.
// ([.\p{Z}\-\(]?[0-9][\p{Z}\-\)]?){8,} checks for at least eight occurrences of
// digits with optional separators to reduce false positives.
const char kPhoneNumberRegexPatternSimple[] =
    R"((?:^|\p{Z})((?:\(?\+[0-9]+\)?)?(?:[.\p{Z}\-(]?[0-9][\p{Z}\-)]?){8,}))";

}  // namespace

const re2::RE2& GetPhoneNumberRegex(PhoneNumberRegexVariant variant) {
  static const re2::LazyRE2 kRegexSimple = {kPhoneNumberRegexPatternSimple};

  switch (variant) {
    case PhoneNumberRegexVariant::kSimple:
      return *kRegexSimple;
  }
}
