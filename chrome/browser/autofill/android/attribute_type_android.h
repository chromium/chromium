// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_ATTRIBUTE_TYPE_ANDROID_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_ATTRIBUTE_TYPE_ANDROID_H_

#include <string>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "third_party/jni_zero/jni_zero.h"

namespace autofill {

// This class is the C++ version of the Java class AttributeType.
struct AttributeTypeAndroid {
 public:
  static jni_zero::ScopedJavaLocalRef<jobject> Create(
      JNIEnv* env,
      const AttributeTypeAndroid& attribute_type);

  static AttributeTypeAndroid FromJavaAttributeType(
      JNIEnv* env,
      const jni_zero::JavaRef<jobject>& j_attribute_type);

  explicit AttributeTypeAndroid(const AttributeType& attribute_type);
  AttributeTypeAndroid(AttributeTypeName type_name,
                       std::u16string type_name_as_string,
                       AttributeType::DataType data_type,
                       FieldType field_type);

  AttributeTypeAndroid(const AttributeTypeAndroid&) = default;
  AttributeTypeAndroid& operator=(const AttributeTypeAndroid&) = default;
  AttributeTypeAndroid(AttributeTypeAndroid&&) = default;
  AttributeTypeAndroid& operator=(AttributeTypeAndroid&&) = default;
  ~AttributeTypeAndroid() = default;

  AttributeType ToAttributeType() const;

  AttributeTypeName type_name;
  std::u16string type_name_as_string;
  AttributeType::DataType data_type;
  FieldType field_type;
};

}  // namespace autofill

namespace jni_zero {
template <>
inline autofill::AttributeTypeAndroid
FromJniType<autofill::AttributeTypeAndroid>(JNIEnv* env,
                                            const JavaRef<jobject>& j_object) {
  return autofill::AttributeTypeAndroid::FromJavaAttributeType(env, j_object);
}

template <>
inline ScopedJavaLocalRef<jobject> ToJniType<autofill::AttributeTypeAndroid>(
    JNIEnv* env,
    const autofill::AttributeTypeAndroid& attribute_type) {
  return autofill::AttributeTypeAndroid::Create(env, attribute_type);
}
}  // namespace jni_zero

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_ATTRIBUTE_TYPE_ANDROID_H_
