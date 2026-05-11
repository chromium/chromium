// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/entity_metadata_android.h"

#include "components/autofill/android/main_autofill_jni_headers/EntityMetadata_jni.h"

namespace autofill {

base::android::ScopedJavaLocalRef<jobject> EntityMetadataAndroid::Create(
    JNIEnv* env,
    const EntityMetadataAndroid& metadata) {
  return Java_EntityMetadata_Constructor(
      env, metadata.date_modified.InMillisecondsSinceUnixEpoch(),
      metadata.use_count);
}

EntityMetadataAndroid EntityMetadataAndroid::FromJavaEntityMetadata(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_metadata) {
  base::Time date_modified = base::Time::FromMillisecondsSinceUnixEpoch(
      Java_EntityMetadata_getModifiedTimeMillis(env, j_metadata));
  int use_count = Java_EntityMetadata_getUseCount(env, j_metadata);

  return EntityMetadataAndroid(date_modified, use_count);
}

EntityMetadataAndroid::EntityMetadataAndroid(base::Time date_modified,
                                             int use_count)
    : date_modified(date_modified), use_count(use_count) {}

EntityMetadataAndroid::EntityMetadataAndroid(const EntityMetadataAndroid&) =
    default;

EntityMetadataAndroid& EntityMetadataAndroid::operator=(
    const EntityMetadataAndroid&) = default;

EntityMetadataAndroid::EntityMetadataAndroid(EntityMetadataAndroid&&) = default;

EntityMetadataAndroid& EntityMetadataAndroid::operator=(
    EntityMetadataAndroid&&) = default;

EntityMetadataAndroid::~EntityMetadataAndroid() = default;

}  // namespace autofill
