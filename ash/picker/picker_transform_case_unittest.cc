// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_transform_case.h"

#include "base/i18n/rtl.h"
#include "base/test/icu_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

TEST(PickerTransformCase, TransformsCaseCorrectlyForEnglish) {
  base::test::ScopedRestoreICUDefaultLocale restore_locale;
  base::i18n::SetICUDefaultLocale("en_US");

  EXPECT_EQ(PickerTransformToUpperCase(u"Text with UPPer & lowER casE."),
            u"TEXT WITH UPPER & LOWER CASE.");
  EXPECT_EQ(PickerTransformToLowerCase(u"Text with UPPer & lowER casE."),
            u"text with upper & lower case.");
  EXPECT_EQ(PickerTransformToTitleCase(u"Text with UPPer & lowER casE."),
            u"Text With Upper & Lower Case.");
}

TEST(PickerTransformCase, TransformsCaseCorrectlyForMixedScripts) {
  base::test::ScopedRestoreICUDefaultLocale restore_locale;
  base::i18n::SetICUDefaultLocale("en_US");

  EXPECT_EQ(PickerTransformToUpperCase(u"«丰(aBc)»"), u"«丰(ABC)»");
  EXPECT_EQ(PickerTransformToLowerCase(u"«丰(aBc)»"), u"«丰(abc)»");
  EXPECT_EQ(PickerTransformToTitleCase(u"«丰(aBc)»"), u"«丰(Abc)»");
}

TEST(PickerTransformCase, TransformsTitleCaseCorrectlyForDutchInEnglishLocale) {
  base::test::ScopedRestoreICUDefaultLocale restore_locale;
  base::i18n::SetICUDefaultLocale("en_us");

  EXPECT_EQ(PickerTransformToTitleCase(u"ijssel igloo IJMUIDEN"),
            u"Ijssel Igloo Ijmuiden");
}

TEST(PickerTransformCase, TransformsTitleCaseCorrectlyForDutchInDutchLocale) {
  base::test::ScopedRestoreICUDefaultLocale restore_locale;
  base::i18n::SetICUDefaultLocale("nl");

  EXPECT_EQ(PickerTransformToTitleCase(u"ijssel igloo IJMUIDEN"),
            u"IJssel Igloo IJmuiden");
}

}  // namespace
}  // namespace ash
