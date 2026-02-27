// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_ENTITY_TYPE_ANDROID_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_ENTITY_TYPE_ANDROID_H_

#include <string>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/autofill/android/attribute_type_android.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "third_party/jni_zero/jni_zero.h"

namespace autofill {

// This class is the C++ version of the Java class EntityType.
struct EntityTypeAndroid {
 public:
  static jni_zero::ScopedJavaLocalRef<jobject> Create(
      JNIEnv* env,
      const EntityTypeAndroid& entity_type);

  static EntityTypeAndroid FromJavaEntityType(
      JNIEnv* env,
      const jni_zero::JavaRef<jobject>& j_entity_type);

  explicit EntityTypeAndroid(const EntityType& entity_type);
  EntityTypeAndroid(const EntityTypeAndroid&);
  EntityTypeAndroid& operator=(const EntityTypeAndroid&);
  EntityTypeAndroid(EntityTypeAndroid&&);
  EntityTypeAndroid& operator=(EntityTypeAndroid&&);
  ~EntityTypeAndroid();

  EntityType ToEntityType() const;

  EntityTypeName type_name;
  bool is_read_only;
  std::u16string type_name_as_string;
  std::string type_name_as_metrics_string;
  std::string add_entity_type_string;
  std::string edit_entity_type_string;
  std::string delete_entity_type_string;
  std::vector<AttributeTypeAndroid> attribute_types;
};

}  // namespace autofill

namespace jni_zero {
template <>
inline autofill::EntityTypeAndroid FromJniType<autofill::EntityTypeAndroid>(
    JNIEnv* env,
    const JavaRef<jobject>& j_object) {
  return autofill::EntityTypeAndroid::FromJavaEntityType(env, j_object);
}

template <>
inline ScopedJavaLocalRef<jobject> ToJniType<autofill::EntityTypeAndroid>(
    JNIEnv* env,
    const autofill::EntityTypeAndroid& entity_type) {
  return autofill::EntityTypeAndroid::Create(env, entity_type);
}
}  // namespace jni_zero

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_ENTITY_TYPE_ANDROID_H_
