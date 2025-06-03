// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/translation_manager_util.h"

#include <string>

#include "base/logging.h"
#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace on_device_translation {

class TranslationManagerUtilTest : public testing::Test {
 public:
  TranslationManagerUtilTest() = default;
  ~TranslationManagerUtilTest() override = default;
};

TEST_F(TranslationManagerUtilTest, PassAcceptLanguagesCheck) {
  // Source lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : true
  // Target lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : true
  EXPECT_TRUE(PassAcceptLanguagesCheck({"en", "es"}, "en", "es"));

  // Source lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : true
  // Target lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : false
  EXPECT_TRUE(PassAcceptLanguagesCheck({"en", "fr"}, "en", "fr"));

  // Source lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : true
  // Target lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : true
  EXPECT_TRUE(PassAcceptLanguagesCheck({"en", "es"}, "en", "zh"));

  // Source lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : true
  // Target lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : false
  // Target is not in accept-languages, and not popular.
  EXPECT_FALSE(PassAcceptLanguagesCheck({"en", "es"}, "en", "fr"));

  // Source lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : false
  // Target lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : true
  EXPECT_TRUE(PassAcceptLanguagesCheck({"de", "es"}, "de", "es"));

  // Source lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : false
  // Target lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : false
  EXPECT_TRUE(PassAcceptLanguagesCheck({"de", "fr"}, "de", "fr"));

  // Source lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : false
  // Target lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : true
  EXPECT_TRUE(PassAcceptLanguagesCheck({"de", "es"}, "de", "zh"));

  // Source lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : false
  // Target lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : false
  // Target is not in accept-languages, and not popular.
  EXPECT_FALSE(PassAcceptLanguagesCheck({"de", "es"}, "de", "fr"));

  // Source lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : true
  // Target lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : true
  EXPECT_TRUE(PassAcceptLanguagesCheck({"en", "es"}, "ja", "es"));

  // Source lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : true
  // Target lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : false
  EXPECT_TRUE(PassAcceptLanguagesCheck({"en", "fr"}, "ja", "fr"));

  // Source lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : true
  // Target lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : true
  EXPECT_TRUE(PassAcceptLanguagesCheck({"en", "es"}, "ja", "zh"));

  // Source lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : true
  // Target lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : false
  // Target is not in accept-languages, and not popular.
  EXPECT_FALSE(PassAcceptLanguagesCheck({"en", "es"}, "ja", "fr"));

  // Source lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : false
  // Target lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : true
  // Source is not in accept-languages, and not popular.
  EXPECT_FALSE(PassAcceptLanguagesCheck({"en", "es"}, "de", "es"));

  // Source lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : false
  // Target lang:
  //   - Is in accept-languages : true
  //   - Is popular lang        : false
  // Source is not in accept-languages, and not popular.
  EXPECT_FALSE(PassAcceptLanguagesCheck({"en", "fr"}, "de", "fr"));

  // Source lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : false
  // Target lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : true
  // Source is not in accept-languages, and not popular.
  EXPECT_FALSE(PassAcceptLanguagesCheck({"en", "es"}, "de", "zh"));

  // Source lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : false
  // Target lang:
  //   - Is in accept-languages : false
  //   - Is popular lang        : false
  // Target and source are not in accept-languages, and not popular.
  EXPECT_FALSE(PassAcceptLanguagesCheck({"en", "es"}, "de", "fr"));
}

TEST_F(TranslationManagerUtilTest,
       LookupMatchingLocaleByBestFitFindsSupportedLanguageTag) {
  std::vector<std::string> en_variations{
      "en",
      "en-Latn",
      "en-Latn-GB",
      "en-GB",
      "en-fonipa-scouse",
      "en-Latn-fonipa-scouse",
      "en-Latn-GB-fonipa-scouse",
      "en-Latn-x-this-is-a-private-use-extensio-n",
  };
  for (const auto& variant : en_variations) {
    EXPECT_EQ(LookupMatchingLocaleByBestFit(kSupportedLanguageCodes, variant),
              "en");
    EXPECT_EQ(LookupMatchingLocaleByBestFit(std::set<std::string>(), variant),
              std::nullopt);
  }

  std::vector<std::string> es_variations{
      "es",
      "es-419",
      "es-ES",
      "es-ES-1979",
  };
  for (const auto& variant : es_variations) {
    EXPECT_EQ(LookupMatchingLocaleByBestFit(kSupportedLanguageCodes, variant),
              "es");
    EXPECT_EQ(LookupMatchingLocaleByBestFit(std::set<std::string>(), variant),
              std::nullopt);
  }
}

TEST_F(TranslationManagerUtilTest,
       LookupMatchingLocaleByBestFitReturnsNulloptForUnsupportedLanguageTags) {
  std::vector<std::string> language_tags{
      "xx",
      "xx-Latn",
      "xx-Latn-GB",
      "xx-GB",
      "xx-fonipa-scouse",
      "xx-Latn-fonipa-scouse",
      "xx-Latn-GB-fonipa-scouse",
      "xx-Latn-x-this-is-a-private-use-extensio-n",
      "xx-419",
      "xx-ES",
      "xx-ES-1979",
  };
  for (const auto& language_tag : language_tags) {
    EXPECT_EQ(
        LookupMatchingLocaleByBestFit(kSupportedLanguageCodes, language_tag),
        std::nullopt);
  }
}

TEST_F(TranslationManagerUtilTest,
       LookupMatchingLocaleByBestFitReturnsMostSpecificLanguageTag) {
  std::vector<std::string> zh_variations{
      "zh",
      "zh-Latn",
      "zh-Latn-GB",
      "zh-GB",
      "zh-fonipa-scouse",
      "zh-Latn-fonipa-scouse",
      "zh-Latn-GB-fonipa-scouse",
      "zh-Latn-x-this-is-a-private-use-extensio-n",
      "zh-419",
      "zh-ES",
      "zh-ES-1979",
  };
  for (const auto& variant : zh_variations) {
    EXPECT_EQ(LookupMatchingLocaleByBestFit(kSupportedLanguageCodes, variant),
              "zh");
  }
  std::vector<std::string> zh_hant_variations{
      "zh-Hant",
      "zh-Hant-GB",
      "zh-Hant-fonipa-scouse",
      "zh-Hant-GB-fonipa-scouse",
      "zh-Hant-x-this-is-a-private-use-extensio-n",
  };
  for (const auto& variant : zh_hant_variations) {
    EXPECT_EQ(LookupMatchingLocaleByBestFit(kSupportedLanguageCodes, variant),
              "zh-Hant");
  }
}

}  // namespace on_device_translation
