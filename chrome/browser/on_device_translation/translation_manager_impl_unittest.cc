// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/translation_manager_impl.h"

#include <string>

#include "base/logging.h"
#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace on_device_translation {

class TranslationManagerImplTest : public testing::Test {
 public:
  TranslationManagerImplTest() = default;
  ~TranslationManagerImplTest() override = default;

 protected:
  static bool PassAcceptLanguagesCheck(const std::string& accept_languages_str,
                                       const std::string& source_lang,
                                       const std::string& target_lang) {
    return TranslationManagerImpl::PassAcceptLanguagesCheck(
        accept_languages_str, source_lang, target_lang);
  }
};

TEST_F(TranslationManagerImplTest, PassAcceptLanguagesCheck) {
  // Source lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : true
  // Target lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : true
  EXPECT_TRUE(PassAcceptLanguagesCheck("en,es", "en", "es"));

  // Source lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : true
  // Target lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : false
  EXPECT_TRUE(PassAcceptLanguagesCheck("en,fr", "en", "fr"));

  // Source lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : true
  // Target lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : true
  EXPECT_TRUE(PassAcceptLanguagesCheck("en,es", "en", "zh"));

  // Source lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : true
  // Target lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : false
  // Target is not in accept-languages, and not popular.
  EXPECT_FALSE(PassAcceptLanguagesCheck("en,es", "en", "fr"));

  // Source lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : false
  // Target lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : true
  EXPECT_TRUE(PassAcceptLanguagesCheck("de,es", "de", "es"));

  // Source lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : false
  // Target lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : false
  EXPECT_TRUE(PassAcceptLanguagesCheck("de,fr", "de", "fr"));

  // Source lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : false
  // Target lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : true
  EXPECT_TRUE(PassAcceptLanguagesCheck("de,es", "de", "zh"));

  // Source lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : false
  // Target lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : false
  // Target is not in accept-languages, and not popular.
  EXPECT_FALSE(PassAcceptLanguagesCheck("de,es", "de", "fr"));

  // Source lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : true
  // Target lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : true
  EXPECT_TRUE(PassAcceptLanguagesCheck("en,es", "ja", "es"));

  // Source lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : true
  // Target lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : false
  EXPECT_TRUE(PassAcceptLanguagesCheck("en,fr", "ja", "fr"));

  // Source lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : true
  // Target lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : true
  // None of source and target lang is in accept-languages.
  EXPECT_FALSE(PassAcceptLanguagesCheck("en,es", "ja", "zh"));

  // Source lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : true
  // Target lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : false
  // None of source and target lang is in accept-languages.
  EXPECT_FALSE(PassAcceptLanguagesCheck("en,es", "ja", "fr"));

  // Source lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : false
  // Target lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : true
  // Source is not in accept-languages, and not popular.
  EXPECT_FALSE(PassAcceptLanguagesCheck("en,es", "de", "es"));

  // Source lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : false
  // Target lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : false
  // Source is not in accept-languages, and not popular.
  EXPECT_FALSE(PassAcceptLanguagesCheck("en,fr", "de", "fr"));

  // Source lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : false
  // Target lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : true
  // None of source and target lang is in accept-languages.
  EXPECT_FALSE(PassAcceptLanguagesCheck("en,es", "de", "zh"));

  // Source lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : false
  // Target lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : false
  // None of source and target lang is in accept-languages.
  EXPECT_FALSE(PassAcceptLanguagesCheck("en,es", "de", "fr"));
}

}  // namespace on_device_translation
