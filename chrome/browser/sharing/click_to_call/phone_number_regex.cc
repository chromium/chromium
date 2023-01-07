// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/phone_number_regex.h"

#include <string>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "third_party/re2/src/re2/re2.h"

namespace {

// Heuristical regex to search for phone number.
// (^|\p{Z}) makes sure the pattern begins with a new word.
// (\(?\+[0-9]+\)?) checks for optional international code in number.
// ([.\p{Z}\-\(]?[0-9][\p{Z}\-\)]?){8,} checks for at least eight occurrences of
// digits with optional separators to reduce false positives.
const char kPhoneNumberRegexPatternSimple[] =
    R"((?:^|\p{Z})((?:\(?\+[0-9]+\)?)?(?:[.\p{Z}\-(]?[0-9][\p{Z}\-)]?){8,}))";

void PrecompilePhoneNumberRegexes() {
  static const char kExampleInput[] = "+01(2)34-5678 9012";

  std::string parsed;
  // Run RE2::PartialMatch over some example input to speed up future queries.
  re2::RE2::PartialMatch(kExampleInput, GetPhoneNumberRegex(), &parsed);
}

}  // namespace

const re2::RE2& GetPhoneNumberRegex() {
  static const re2::LazyRE2 regex_simple = {kPhoneNumberRegexPatternSimple};
  return *regex_simple;
}

void PrecompilePhoneNumberRegexesAsync() {
  constexpr auto kParseDelay = base::Seconds(15);
  base::ThreadPool::PostDelayedTask(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&PrecompilePhoneNumberRegexes), kParseDelay);
}
