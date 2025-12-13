// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_ATTRIBUTE_INSTANCE_ANDROID_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_ATTRIBUTE_INSTANCE_ANDROID_H_

#include <optional>
#include <string>
#include <variant>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/autofill/android/attribute_type_android.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "third_party/jni_zero/jni_zero.h"

namespace autofill {

struct AttributeInstanceAndroidDateType {
  std::u16string day;
  std::u16string month;
  std::u16string year;
};

// This class is the C++ version of the Java class AttributeInstance.
struct AttributeInstanceAndroid {
 public:
  static jni_zero::ScopedJavaLocalRef<jobject> Create(
      JNIEnv* env,
      const AttributeInstanceAndroid& attribute_instance);

  static AttributeInstanceAndroid FromJavaAttributeInstance(
      JNIEnv* env,
      const jni_zero::JavaRef<jobject>& j_attribute_instance);

  explicit AttributeInstanceAndroid(
      const AttributeInstance& attribute_instance);
  AttributeInstanceAndroid(AttributeTypeAndroid attribute_type,
                           AttributeInstanceAndroidDateType date_value);

  AttributeInstanceAndroid(AttributeTypeAndroid attribute_type,
                           std::u16string string_value);

  AttributeInstanceAndroid(const AttributeInstanceAndroid&);
  AttributeInstanceAndroid& operator=(const AttributeInstanceAndroid&) =
      default;
  AttributeInstanceAndroid(AttributeInstanceAndroid&&);
  AttributeInstanceAndroid& operator=(AttributeInstanceAndroid&&) = default;
  ~AttributeInstanceAndroid();

  AttributeInstance ToAttributeInstance() const;

  AttributeTypeAndroid attribute_type;
  // For non date types `value` holds a raw string that represents an attribute
  // value, otherwise it holds a `AttributeInstanceAndroidDateType`.
  std::variant<std::u16string, AttributeInstanceAndroidDateType> value;
};

}  // namespace autofill

namespace jni_zero {
template <>
inline autofill::AttributeInstanceAndroid
FromJniType<autofill::AttributeInstanceAndroid>(
    JNIEnv* env,
    const JavaRef<jobject>& j_object) {
  return autofill::AttributeInstanceAndroid::FromJavaAttributeInstance(
      env, j_object);
}

template <>
inline ScopedJavaLocalRef<jobject>
ToJniType<autofill::AttributeInstanceAndroid>(
    JNIEnv* env,
    const autofill::AttributeInstanceAndroid& attribute_instance) {
  return autofill::AttributeInstanceAndroid::Create(env, attribute_instance);
}
}  // namespace jni_zero

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_ATTRIBUTE_INSTANCE_ANDROID_H_
