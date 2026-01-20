// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_ENTITY_METADATA_ANDROID_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_ENTITY_METADATA_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/time/time.h"
#include "third_party/jni_zero/jni_zero.h"

namespace autofill {

// This class is the C++ version of the Java class EntityMetadata.
struct EntityMetadataAndroid {
 public:
  static jni_zero::ScopedJavaLocalRef<jobject> Create(
      JNIEnv* env,
      const EntityMetadataAndroid& metadata);

  static EntityMetadataAndroid FromJavaEntityMetadata(
      JNIEnv* env,
      const jni_zero::JavaRef<jobject>& j_metadata);

  EntityMetadataAndroid(base::Time date_modified, int use_count);
  EntityMetadataAndroid(const EntityMetadataAndroid&);
  EntityMetadataAndroid& operator=(const EntityMetadataAndroid&);
  EntityMetadataAndroid(EntityMetadataAndroid&&);
  EntityMetadataAndroid& operator=(EntityMetadataAndroid&&);
  ~EntityMetadataAndroid();

  base::Time date_modified;
  int use_count;
};

}  // namespace autofill

namespace jni_zero {
template <>
inline autofill::EntityMetadataAndroid
FromJniType<autofill::EntityMetadataAndroid>(JNIEnv* env,
                                             const JavaRef<jobject>& j_object) {
  return autofill::EntityMetadataAndroid::FromJavaEntityMetadata(env, j_object);
}

template <>
inline ScopedJavaLocalRef<jobject> ToJniType<autofill::EntityMetadataAndroid>(
    JNIEnv* env,
    const autofill::EntityMetadataAndroid& metadata) {
  return autofill::EntityMetadataAndroid::Create(env, metadata);
}
}  // namespace jni_zero

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_ENTITY_METADATA_ANDROID_H_
