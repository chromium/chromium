// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/attribute_type_android.h"

#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/field_types.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/autofill/android/main_autofill_jni_headers/AttributeType_jni.h"

namespace autofill {

base::android::ScopedJavaLocalRef<jobject> AttributeTypeAndroid::Create(
    JNIEnv* env,
    const AttributeTypeAndroid& attribute_type) {
  return Java_AttributeType_Constructor(
      env, static_cast<int>(attribute_type.type_name),
      attribute_type.type_name_as_string,
      static_cast<int>(attribute_type.data_type),
      static_cast<int>(attribute_type.field_type));
}

AttributeTypeAndroid AttributeTypeAndroid::FromJavaAttributeType(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_attribute_type) {
  return AttributeTypeAndroid(AttributeType(static_cast<AttributeTypeName>(
      Java_AttributeType_getTypeName(env, j_attribute_type))));
}

AttributeType AttributeTypeAndroid::ToAttributeType() const {
  return AttributeType(type_name);
}

AttributeTypeAndroid::AttributeTypeAndroid(const AttributeType& attribute_type)
    : type_name(attribute_type.name()),
      type_name_as_string(attribute_type.GetNameForI18n()),
      data_type(attribute_type.data_type()),
      field_type(attribute_type.field_type()) {}

AttributeTypeAndroid::AttributeTypeAndroid(AttributeTypeName type_name,
                                           std::u16string type_name_as_string,
                                           AttributeType::DataType data_type,
                                           FieldType field_type)
    : type_name(type_name),
      type_name_as_string(std::move(type_name_as_string)),
      data_type(data_type),
      field_type(field_type) {}

}  // namespace autofill
