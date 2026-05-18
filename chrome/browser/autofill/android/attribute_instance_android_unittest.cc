// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/attribute_instance_android.h"

#include <string>

#include "base/test/task_environment.h"
#include "chrome/browser/autofill/android/attribute_type_android.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/jni_zero/jni_zero.h"

namespace autofill {

namespace {

using enum AttributeTypeName;
using ::testing::TestWithParam;

struct AttributeInstanceTestParams {
  AttributeType attribute_type;
  FieldType field_type;
  std::u16string value;
  std::optional<AutofillFormatString> format_string;
};

class AttributeInstanceAndroidTest
    : public TestWithParam<AttributeInstanceTestParams> {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_P(AttributeInstanceAndroidTest, DoubleConversion) {
  const AttributeInstanceTestParams& params = GetParam();
  AttributeInstance attribute = AttributeInstance(params.attribute_type);
  attribute.SetInfo(params.field_type, params.value, "en-US",
                    params.format_string, VerificationStatus::kNoStatus);
  if (params.attribute_type == AttributeType(kPassportName)) {
    attribute.FinalizeInfo();
  }

  AttributeInstanceAndroid attribute_instance_android(attribute);

  JNIEnv* env = jni_zero::AttachCurrentThread();
  jni_zero::ScopedJavaLocalRef<jobject> java_instance =
      jni_zero::ToJniType<AttributeInstanceAndroid>(env,
                                                    attribute_instance_android);

  AttributeInstanceAndroid converted_attribute_android =
      jni_zero::FromJniType<AttributeInstanceAndroid>(env, java_instance);

  AttributeInstance converted_attribute =
      converted_attribute_android.ToAttributeInstance();

  EXPECT_EQ(attribute, converted_attribute);
}

INSTANTIATE_TEST_SUITE_P(
    DoubleAttributeConversion,
    AttributeInstanceAndroidTest,
    testing::Values(
        AttributeInstanceTestParams{AttributeType(kPassportName), NAME_FULL,
                                    u"John Doe", std::nullopt},
        AttributeInstanceTestParams{AttributeType(kPassportCountry),
                                    PASSPORT_ISSUING_COUNTRY, u"Ukraine",
                                    std::nullopt},
        AttributeInstanceTestParams{AttributeType(kDriversLicenseState),
                                    DRIVERS_LICENSE_REGION, u"Bavaria",
                                    std::nullopt},
        AttributeInstanceTestParams{
            AttributeType(kPassportExpirationDate), PASSPORT_EXPIRATION_DATE,
            u"2019-08-30",
            AutofillFormatString(u"YYYY-MM-DD", FormatString_Type_DATE)},
        AttributeInstanceTestParams{AttributeType(kPassportNumber),
                                    PASSPORT_NUMBER, u"AA123456BB",
                                    std::nullopt}));

}  // namespace

}  // namespace autofill
