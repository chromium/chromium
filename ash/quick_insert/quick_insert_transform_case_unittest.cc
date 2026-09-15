// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/quick_insert_transform_case.h"

#include "base/i18n/language_tag.h"
#include "base/i18n/rtl.h"
#include "base/i18n/tag_converters.h"
#include "base/i18n/test/scoped_icu_locale.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

TEST(QuickInsertTransformCase, TransformsCaseCorrectlyForEnglish) {
  base::i18n::ScopedDefaultIcuLocale scoped_locale(
      base::i18n::GetKnownLanguageTag("en-US"));

  EXPECT_EQ(QuickInsertTransformToUpperCase(u"Text with UPPer & lowER casE."),
            u"TEXT WITH UPPER & LOWER CASE.");
  EXPECT_EQ(QuickInsertTransformToLowerCase(u"Text with UPPer & lowER casE."),
            u"text with upper & lower case.");
  EXPECT_EQ(QuickInsertTransformToTitleCase(u"Text with UPPer & lowER casE."),
            u"Text With Upper & Lower Case.");
}

TEST(QuickInsertTransformCase, TransformsCaseCorrectlyForMixedScripts) {
  base::i18n::ScopedDefaultIcuLocale scoped_locale(
      base::i18n::GetKnownLanguageTag("en-US"));

  EXPECT_EQ(QuickInsertTransformToUpperCase(u"«丰(aBc)»"), u"«丰(ABC)»");
  EXPECT_EQ(QuickInsertTransformToLowerCase(u"«丰(aBc)»"), u"«丰(abc)»");
  EXPECT_EQ(QuickInsertTransformToTitleCase(u"«丰(aBc)»"), u"«丰(Abc)»");
}

TEST(QuickInsertTransformCase,
     TransformsTitleCaseCorrectlyForDutchInEnglishLocale) {
  base::i18n::ScopedDefaultIcuLocale scoped_locale(
      base::i18n::GetKnownLanguageTag("en-US"));

  EXPECT_EQ(QuickInsertTransformToTitleCase(u"ijssel igloo IJMUIDEN"),
            u"Ijssel Igloo Ijmuiden");
}

TEST(QuickInsertTransformCase,
     TransformsTitleCaseCorrectlyForDutchInDutchLocale) {
  base::i18n::ScopedDefaultIcuLocale scoped_locale(
      base::i18n::GetKnownLanguageTag("nl"));

  EXPECT_EQ(QuickInsertTransformToTitleCase(u"ijssel igloo IJMUIDEN"),
            u"IJssel Igloo IJmuiden");
}

}  // namespace
}  // namespace ash
