// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_ENTITY_INSTANCE_ANDROID_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_ENTITY_INSTANCE_ANDROID_H_

#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/time/time.h"
#include "chrome/browser/autofill/android/attribute_instance_android.h"
#include "chrome/browser/autofill/android/entity_metadata_android.h"
#include "chrome/browser/autofill/android/entity_type_android.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "third_party/jni_zero/jni_zero.h"

namespace autofill {

// This class is the C++ version of the Java class EntityInstance.
struct EntityInstanceAndroid {
 public:
  static jni_zero::ScopedJavaLocalRef<jobject> Create(
      JNIEnv* env,
      const EntityInstanceAndroid& entity_instance);

  static EntityInstanceAndroid FromJavaEntityInstance(
      JNIEnv* env,
      const jni_zero::JavaRef<jobject>& j_entity_instance);

  explicit EntityInstanceAndroid(const EntityInstance& entity_instance);
  EntityInstanceAndroid(EntityTypeAndroid entity_type,
                        std::string guid,
                        EntityInstance::RecordType record_type,
                        std::vector<AttributeInstanceAndroid> attribute_values,
                        EntityMetadataAndroid metadata);
  EntityInstanceAndroid(const EntityInstanceAndroid&);
  EntityInstanceAndroid& operator=(const EntityInstanceAndroid&) = default;
  EntityInstanceAndroid(EntityInstanceAndroid&&);
  EntityInstanceAndroid& operator=(EntityInstanceAndroid&&) = default;
  ~EntityInstanceAndroid();

  // Convert `EntityInstanceAndroid` to `EntityInstance`.
  // If `existing_entity` exists, this method reuses the attribute instances
  // from `existing_entity` iff it's raw value is the same as the one in `this`.
  // This guarantees that for unmodified attributes, their field types structure
  // (such as for names) remain the same.
  EntityInstance ToEntityInstance(
      base::optional_ref<const EntityInstance> existing_entity) const;

  EntityTypeAndroid entity_type;
  std::string guid;
  EntityInstance::RecordType record_type;
  std::vector<AttributeInstanceAndroid> attribute_instances;
  EntityMetadataAndroid metadata;
};

}  // namespace autofill

namespace jni_zero {
template <>
inline autofill::EntityInstanceAndroid
FromJniType<autofill::EntityInstanceAndroid>(JNIEnv* env,
                                             const JavaRef<jobject>& j_object) {
  return autofill::EntityInstanceAndroid::FromJavaEntityInstance(env, j_object);
}

template <>
inline ScopedJavaLocalRef<jobject> ToJniType<autofill::EntityInstanceAndroid>(
    JNIEnv* env,
    const autofill::EntityInstanceAndroid& entity_instance) {
  return autofill::EntityInstanceAndroid::Create(env, entity_instance);
}
}  // namespace jni_zero

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_ENTITY_INSTANCE_ANDROID_H_
