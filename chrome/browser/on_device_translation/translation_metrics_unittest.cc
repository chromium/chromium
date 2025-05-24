// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/translation_metrics.h"

#include "base/logging.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/on_device_translation/language_pack_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace on_device_translation {
namespace {

class TranslationMetricsTest : public ::testing::Test {
 public:
  static constexpr int kLanguageCodeEn =
      static_cast<int>(SupportedLanguage::kEn);
  static constexpr int kLanguageCodeEs =
      static_cast<int>(SupportedLanguage::kEs);
};

TEST_F(TranslationMetricsTest, RecordLanguageUma) {
  const char kTestHistogramName[] =
      "Translate.OnDeviceTranslation.Create.SourceLanguage";
  {
    base::HistogramTester histogram_tester;
    RecordLanguageUma(kTestHistogramName, "en");
    histogram_tester.ExpectUniqueSample(kTestHistogramName, kLanguageCodeEn, 1);
  }
  {
    // Test recording an unknown language code
    base::HistogramTester histogram_tester;
    RecordLanguageUma(kTestHistogramName, "xxxxx");
    histogram_tester.ExpectTotalCount(kTestHistogramName, 0);
  }
}

TEST_F(TranslationMetricsTest, RecordLanguagePairUma) {
  const char kTestHistogramName[] =
      "Translate.OnDeviceTranslation.Create.LangaugePair";
  {
    base::HistogramTester histogram_tester;
    RecordLanguagePairUma(kTestHistogramName, "en", "es");
    histogram_tester.ExpectUniqueSample(
        kTestHistogramName, kLanguageCodeEn * 1000 + kLanguageCodeEs, 1);
  }

  {
    // Test recording an unknown language code as source language
    base::HistogramTester histogram_tester;
    RecordLanguagePairUma(kTestHistogramName, "xxxxx", "en");
    histogram_tester.ExpectTotalCount(kTestHistogramName, 0);
  }

  {
    // Test recording an unknown language code as target language
    base::HistogramTester histogram_tester;
    RecordLanguagePairUma(kTestHistogramName, "en", "xxxxx");
    histogram_tester.ExpectTotalCount(kTestHistogramName, 0);
  }

  {
    // Test recording an unknown language code as source and target language
    base::HistogramTester histogram_tester;
    RecordLanguagePairUma(kTestHistogramName, "xxxxx", "xxxxx");
    histogram_tester.ExpectTotalCount(kTestHistogramName, 0);
  }
}

TEST_F(TranslationMetricsTest,
       RecordLanguageTranslationAPICallForLanguagePair) {
  const char kTestSourceLanguageHistogramName[] =
      "Translate.OnDeviceTranslation.Create.SourceLanguage";
  const char kTestTargetLanguageHistogramName[] =
      "Translate.OnDeviceTranslation.Create.TargetLanguage";
  const char kTestLanguagePairHistogramName[] =
      "Translate.OnDeviceTranslation.Create.LanguagePair";
  {
    base::HistogramTester histogram_tester;
    RecordTranslationAPICallForLanguagePair("Create", "en", "es");
    histogram_tester.ExpectUniqueSample(kTestSourceLanguageHistogramName,
                                        kLanguageCodeEn, 1);
    histogram_tester.ExpectUniqueSample(kTestTargetLanguageHistogramName,
                                        kLanguageCodeEs, 1);
    histogram_tester.ExpectUniqueSample(
        kTestLanguagePairHistogramName,
        kLanguageCodeEn * 1000 + kLanguageCodeEs, 1);
  }

  {
    // Test recording an unknown language code as source language
    base::HistogramTester histogram_tester;
    RecordTranslationAPICallForLanguagePair("Create", "xxxxx", "es");
    histogram_tester.ExpectTotalCount(kTestSourceLanguageHistogramName, 0);
    histogram_tester.ExpectTotalCount(kTestTargetLanguageHistogramName, 0);
    histogram_tester.ExpectTotalCount(kTestLanguagePairHistogramName, 0);
  }

  {
    // Test recording an unknown language code as target language
    base::HistogramTester histogram_tester;
    RecordTranslationAPICallForLanguagePair("Create", "en", "xxxxx");
    histogram_tester.ExpectTotalCount(kTestSourceLanguageHistogramName, 0);
    histogram_tester.ExpectTotalCount(kTestTargetLanguageHistogramName, 0);
    histogram_tester.ExpectTotalCount(kTestLanguagePairHistogramName, 0);
  }

  {
    // Test recording an unknown language code as source and target language
    base::HistogramTester histogram_tester;
    RecordTranslationAPICallForLanguagePair("Create", "xxxx", "xxxxx");
    histogram_tester.ExpectTotalCount(kTestSourceLanguageHistogramName, 0);
    histogram_tester.ExpectTotalCount(kTestTargetLanguageHistogramName, 0);
    histogram_tester.ExpectTotalCount(kTestLanguagePairHistogramName, 0);
  }
}

TEST_F(TranslationMetricsTest, RecordTranslationCharacterCount) {
  base::HistogramTester histogram_tester;
  RecordTranslationCharacterCount("en", "es", 100);
  histogram_tester.ExpectUniqueSample(
      "Translate.OnDeviceTranslation.CharacterCount", 100, 1);
  histogram_tester.ExpectUniqueSample(
      "Translate.OnDeviceTranslation.SourceLanguage.en.CharacterCount", 100, 1);
  histogram_tester.ExpectUniqueSample(
      "Translate.OnDeviceTranslation.TargetLanguage.es.CharacterCount", 100, 1);
}
}  // namespace
}  // namespace on_device_translation
