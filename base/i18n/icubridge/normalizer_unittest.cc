// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icubridge/normalizer.h"

#include "base/i18n/icu_util.h"
#include "base/i18n/icubridge/features.h"
#include "base/i18n/icubridge/icu_bridge.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::i18n {

class NormalizerParameterizedTest : public testing::TestWithParam<bool> {
 public:
  void SetUp() override {
    base::i18n::InitializeICU();
    bool use_icu4x = GetParam();
    if (use_icu4x) {
      feature_list_.InitAndEnableFeature(kUseIcu4xNormalizer);
    } else {
      feature_list_.InitAndDisableFeature(kUseIcu4xNormalizer);
    }
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(NormalizerParameterizedTest, NormalizationTests) {
  const IcuBridge::Normalizer& normalizer =
      IcuBridge::GetInstance().normalizer();

  // 1. NFC / NFD Test (e.g. 'á')
  std::u16string precomposed = u"\u00e1";
  std::u16string decomposed = u"a\u0301";

  EXPECT_EQ(precomposed,
            normalizer.Normalize(IcuBridge::Normalizer::NormalizationForm::NFC,
                                 decomposed));
  EXPECT_EQ(decomposed,
            normalizer.Normalize(IcuBridge::Normalizer::NormalizationForm::NFD,
                                 precomposed));

  // 2. NFKC Test (e.g. 'ﬁ' ligature)
  std::u16string ligature = u"\ufb01";
  std::u16string expanded = u"fi";

  EXPECT_EQ(expanded,
            normalizer.Normalize(IcuBridge::Normalizer::NormalizationForm::NFKC,
                                 ligature));
}

TEST_P(NormalizerParameterizedTest, IcuBridgeNormalizerDefaultInstance) {
  const IcuBridge::Normalizer& normalizer =
      IcuBridge::GetInstance().normalizer();

  // Test the registered normalizer in IcuBridge.
  std::u16string precomposed = u"\u00e1";
  std::u16string decomposed = u"a\u0301";

  EXPECT_EQ(precomposed,
            normalizer.Normalize(IcuBridge::Normalizer::NormalizationForm::NFC,
                                 decomposed));
}

INSTANTIATE_TEST_SUITE_P(All, NormalizerParameterizedTest, testing::Bool());

}  // namespace base::i18n
