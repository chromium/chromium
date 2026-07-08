// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/internal/bcp47_known_subtags.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base::i18n_internal {

TEST(Bcp47KnownSubtagsTest, Constexprness) {
  // Known subtag checks.
  static_assert(IsKnownLanguageSubtag("en"));
  // Not normalized is unknown.
  static_assert(!IsKnownLanguageSubtag("EN"));
  static_assert(!IsKnownLanguageSubtag(""));
  static_assert(IsKnownLanguageSubtag("und"));
  static_assert(IsKnownLanguageSubtag("zh"));
  static_assert(!IsKnownLanguageSubtag("xx"));

  static_assert(IsKnownScriptSubtag("Hant"));
  // Not normalized script subtag is considered unknown.
  static_assert(!IsKnownScriptSubtag("hant"));
  static_assert(!IsKnownScriptSubtag(""));
  static_assert(!IsKnownScriptSubtag("Zzzz"));

  static_assert(IsKnownRegionSubtag("US"));
  // Not normalized is unknown.
  static_assert(!IsKnownRegionSubtag("us"));
  static_assert(!IsKnownRegionSubtag(""));
  static_assert(IsKnownRegionSubtag("001"));
  static_assert(IsKnownRegionSubtag("JP"));
  static_assert(IsKnownRegionSubtag("KR"));
  static_assert(!IsKnownRegionSubtag("ZZ"));

  static_assert(IsKnownVariantSubtag("oxendict"));
  // Not normalized is unknown.
  static_assert(!IsKnownVariantSubtag("Oxendict"));
  static_assert(!IsKnownVariantSubtag(""));
  static_assert(!IsKnownVariantSubtag("unknown"));
}

TEST(Bcp47KnownSubtagsTest, RuntimeCheck) {
  EXPECT_TRUE(IsKnownLanguageSubtag("en"));
  EXPECT_TRUE(IsKnownLanguageSubtag("zh"));
  EXPECT_FALSE(IsKnownLanguageSubtag("xx"));

  EXPECT_TRUE(IsKnownScriptSubtag("Hant"));
  EXPECT_FALSE(IsKnownScriptSubtag("Zzzz"));

  EXPECT_TRUE(IsKnownRegionSubtag("US"));
  EXPECT_TRUE(IsKnownRegionSubtag("001"));
  EXPECT_TRUE(IsKnownRegionSubtag("JP"));
  EXPECT_TRUE(IsKnownRegionSubtag("KR"));
  EXPECT_FALSE(IsKnownRegionSubtag("ZZ"));

  EXPECT_TRUE(IsKnownVariantSubtag("oxendict"));
  EXPECT_FALSE(IsKnownVariantSubtag("unknown"));
}

}  // namespace base::i18n_internal
