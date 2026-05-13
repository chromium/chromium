// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/attribute_instance_android.h"

#include <variant>

#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "components/autofill/android/main_autofill_jni_headers/AttributeInstance_jni.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/proto/server.pb.h"

namespace autofill {

base::android::ScopedJavaLocalRef<jobject> AttributeInstanceAndroid::Create(
    JNIEnv* env,
    const AttributeInstanceAndroid& attribute_instance) {
  if (std::holds_alternative<AttributeInstanceAndroidDateType>(
          attribute_instance.value)) {
    const AttributeInstanceAndroidDateType& date_value =
        std::get<AttributeInstanceAndroidDateType>(attribute_instance.value);
    return Java_AttributeInstance_Constructor(
        env, jni_zero::ToJniType(env, attribute_instance.attribute_type),
        date_value.day, date_value.month, date_value.year,
        attribute_instance.verification_status);
  } else {
    const std::u16string& string_value =
        std::get<std::u16string>(attribute_instance.value);
    return Java_AttributeInstance_Constructor(
        env, jni_zero::ToJniType(env, attribute_instance.attribute_type),
        string_value, attribute_instance.verification_status);
  }
}

AttributeInstanceAndroid AttributeInstanceAndroid::FromJavaAttributeInstance(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_attribute_instance) {
  AttributeTypeAndroid type = jni_zero::FromJniType<AttributeTypeAndroid>(
      env, Java_AttributeInstance_getAttributeType(env, j_attribute_instance));
  VerificationStatus attribute_verification_status =
      Java_AttributeInstance_getVerificationStatus(env, j_attribute_instance);

  if (Java_AttributeInstance_isDateType(env, j_attribute_instance)) {
    return AttributeInstanceAndroid(
        std::move(type),
        {.day = Java_AttributeInstance_getDay(env, j_attribute_instance),
         .month = Java_AttributeInstance_getMonth(env, j_attribute_instance),
         .year = Java_AttributeInstance_getYear(env, j_attribute_instance)},
        attribute_verification_status);
  } else {
    return AttributeInstanceAndroid(
        std::move(type),
        Java_AttributeInstance_getStringValue(env, j_attribute_instance),
        attribute_verification_status);
  }
}

AttributeInstanceAndroid::AttributeInstanceAndroid(
    const AttributeInstance& attribute_instance)
    : attribute_type(attribute_instance.type()),
      verification_status(attribute_instance.GetVerificationStatus(
          attribute_instance.type().field_type())) {
  if (attribute_type.data_type == AttributeType::DataType::kDate) {
    const std::string& app_locale = g_browser_process->GetApplicationLocale();
    std::optional<autofill::FieldType> field_type =
        attribute_instance.type().field_type();
    const std::u16string day = attribute_instance.GetInfo(
        field_type, app_locale,
        AutofillFormatString(u"D", autofill::FormatString_Type_DATE));
    const std::u16string month = attribute_instance.GetInfo(
        field_type, app_locale,
        AutofillFormatString(u"M", autofill::FormatString_Type_DATE));
    const std::u16string year = attribute_instance.GetInfo(
        field_type, app_locale,
        AutofillFormatString(u"YYYY", autofill::FormatString_Type_DATE));
    value = AttributeInstanceAndroidDateType{
        .day = day, .month = month, .year = year};
  } else {
    value = attribute_instance.GetCompleteRawInfo();
  }
}

AttributeInstanceAndroid::AttributeInstanceAndroid(
    const AttributeInstanceAndroid&) = default;
AttributeInstanceAndroid::AttributeInstanceAndroid(AttributeInstanceAndroid&&) =
    default;
AttributeInstanceAndroid::~AttributeInstanceAndroid() = default;

AttributeInstance AttributeInstanceAndroid::ToAttributeInstance() const {
  AttributeInstance instance(attribute_type.ToAttributeType());
  if (std::holds_alternative<AttributeInstanceAndroidDateType>(value)) {
    const std::string& app_locale = g_browser_process->GetApplicationLocale();
    std::optional<autofill::FieldType> field_type =
        attribute_type.ToAttributeType().field_type();
    const AttributeInstanceAndroidDateType& date_value =
        std::get<AttributeInstanceAndroidDateType>(value);
    instance.SetInfo(
        field_type, date_value.day, app_locale,
        AutofillFormatString(u"D", autofill::FormatString_Type_DATE),
        verification_status);
    instance.SetInfo(
        field_type, date_value.month, app_locale,
        AutofillFormatString(u"M", autofill::FormatString_Type_DATE),
        verification_status);
    instance.SetInfo(
        field_type, date_value.year, app_locale,
        AutofillFormatString(u"YYYY", autofill::FormatString_Type_DATE),
        verification_status);
  } else {
    instance.SetRawInfo(attribute_type.ToAttributeType().field_type(),
                        std::get<std::u16string>(value), verification_status);
  }

  instance.FinalizeInfo();
  return instance;
}

AttributeInstanceAndroid::AttributeInstanceAndroid(
    AttributeTypeAndroid attribute_type,
    std::u16string string_value,
    VerificationStatus verification_status)
    : attribute_type(std::move(attribute_type)),
      value(std::move(string_value)),
      verification_status(verification_status) {}

AttributeInstanceAndroid::AttributeInstanceAndroid(
    AttributeTypeAndroid attribute_type,
    AttributeInstanceAndroidDateType date_value,
    VerificationStatus verification_status)
    : attribute_type(std::move(attribute_type)),
      value(std::move(date_value)),
      verification_status(verification_status) {}

}  // namespace autofill
