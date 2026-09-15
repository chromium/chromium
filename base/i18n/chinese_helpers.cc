// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/chinese_helpers.h"

#include "base/i18n/language_tag.h"
#include "base/i18n/language_tag_matcher.h"
#include "base/no_destructor.h"

namespace base::i18n {
namespace {

const LanguageTagMatcher& TraditionalChineseMatcher() {
  static base::NoDestructor<LanguageTagMatcher> matcher([] {
    return LanguageTagMatcher::Create({GetKnownLanguageTag("zh-Hant")});
  }());

  return *matcher;
}

const LanguageTagMatcher& SimplifiedChineseMatcher() {
  static base::NoDestructor<LanguageTagMatcher> matcher([] {
    return LanguageTagMatcher::Create({GetKnownLanguageTag("zh-Hans")});
  }());

  return *matcher;
}

}  // namespace

bool IsChinese(const LanguageTag& tag) {
  return tag.language_subtag() == "zh";
}

bool IsSimplifiedChinese(const LanguageTag& tag) {
  return SimplifiedChineseMatcher().Match(tag).has_value();
}

bool IsTraditionalChinese(const LanguageTag& tag) {
  return TraditionalChineseMatcher().Match(tag).has_value();
}
}  // namespace base::i18n
