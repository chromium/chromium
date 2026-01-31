// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/entity_metadata_android.h"

#include "components/autofill/android/main_autofill_jni_headers/EntityMetadata_jni.h"

namespace autofill {

base::android::ScopedJavaLocalRef<jobject> EntityMetadataAndroid::Create(
    JNIEnv* env,
    const EntityMetadataAndroid& metadata) {
  base::Time::Exploded exploded;
  metadata.date_modified.LocalExplode(&exploded);
  return Java_EntityMetadata_Constructor(env, exploded.day_of_month,
                                         exploded.month, exploded.year,
                                         metadata.use_count);
}

EntityMetadataAndroid EntityMetadataAndroid::FromJavaEntityMetadata(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_metadata) {
  base::Time::Exploded exploded = {};
  exploded.day_of_month = Java_EntityMetadata_getModifiedDay(env, j_metadata);
  exploded.month = Java_EntityMetadata_getModifiedMonth(env, j_metadata);
  exploded.year = Java_EntityMetadata_getModifiedYear(env, j_metadata);
  base::Time date_modified;
  bool success = base::Time::FromLocalExploded(exploded, &date_modified);
  DCHECK(success);

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
