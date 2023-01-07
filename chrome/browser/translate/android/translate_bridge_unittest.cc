// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/android/translate_bridge.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

// A test class for TranslateBridge class.
class TranslateBridgeTest : public testing::Test {
 public:
  std::string GetAcceptLanguages(std::string locale,
                                 std::string accept_languages) {
    TranslateBridge::PrependToAcceptLanguagesIfNecessary(locale,
                                                         &accept_languages);
    return accept_languages;
  }
};

TEST_F(TranslateBridgeTest, PrependToAcceptLanguagesAsNecessary) {
  EXPECT_EQ("ms-MY,ms,en-US,en", GetAcceptLanguages("ms-MY", "en-US,en"));
  EXPECT_EQ("de-CH,de,zh-TW,zh,en-US,en",
            GetAcceptLanguages("de-CH,zh-TW", "en-US,en"));
  EXPECT_EQ("de-CH,de,zh-TW,zh,fr-FR,fr,en-US,en",
            GetAcceptLanguages("de-CH,zh-TW,fr-FR", "en-US,en"));

  // Make sure a country code in number format is inserted.
  EXPECT_EQ("es-419,es-005,es,en-US,en",
            GetAcceptLanguages("es-419,es-005", "en-US,en"));

  // Make sure we do not prepend language code even when a language code already
  // exists.
  EXPECT_EQ("zh-TW,zh-CN,zh", GetAcceptLanguages("zh-TW", "zh-CN,zh"));
  EXPECT_EQ("de-CH,de-DE,de,en-US,en",
            GetAcceptLanguages("de-CH", "de-DE,de,en-US,en"));
  EXPECT_EQ("en-GB,de-DE,de,en-US,en",
            GetAcceptLanguages("en-GB,de-DE", "en-US,en"));

  // Make sure a language code is only inserted after the last languageTag that
  // contains that language.
  EXPECT_EQ("fr-CA,fr-FR,fr,en-US,en",
            GetAcceptLanguages("fr-CA,fr-FR", "en-US,en"));

  // If a country code is missing, then only the language code is inserted.
  EXPECT_EQ("ms,en-US,en", GetAcceptLanguages("ms", "en-US,en"));
  EXPECT_EQ("mas,en-US,en", GetAcceptLanguages("mas", "en-US,en"));
}

TEST_F(TranslateBridgeTest, ShouldNotPrependToAcceptLanguagesWhenNotNecessary) {
  // This logic should not affect cases where original accept language already
  // reflects the language code in the locale.
  EXPECT_EQ("mas,en-US,en", GetAcceptLanguages("mas", "en-US,en"));
  EXPECT_EQ("en-US,en", GetAcceptLanguages("en-US", "en-US,en"));
  EXPECT_EQ("zh-CN,zh", GetAcceptLanguages("zh-CN", "zh-CN,zh"));
  EXPECT_EQ("ms-MY,ms,en-US,en", GetAcceptLanguages("ms-MY,en-US", "en-US,en"));
}
