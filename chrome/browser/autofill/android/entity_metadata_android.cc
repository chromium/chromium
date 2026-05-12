// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/entity_metadata_android.h"

#include "base/android/jni_string.h"
#include "components/autofill/android/main_autofill_jni_headers/EntityMetadata_jni.h"

namespace autofill {

base::android::ScopedJavaLocalRef<jobject> EntityMetadataAndroid::Create(
    JNIEnv* env,
    const EntityMetadataAndroid& metadata) {
  return Java_EntityMetadata_Constructor(
      env, metadata.guid, metadata.date_modified.InMillisecondsSinceUnixEpoch(),
      metadata.use_count, metadata.use_date.InMillisecondsSinceUnixEpoch());
}

EntityMetadataAndroid EntityMetadataAndroid::FromJavaEntityMetadata(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_metadata) {
  std::string guid = Java_EntityMetadata_getGuid(env, j_metadata);
  base::Time date_modified = base::Time::FromMillisecondsSinceUnixEpoch(
      Java_EntityMetadata_getModifiedTimeMillis(env, j_metadata));
  int use_count = Java_EntityMetadata_getUseCount(env, j_metadata);
  base::Time use_date = base::Time::FromMillisecondsSinceUnixEpoch(
      Java_EntityMetadata_getUseDateMillis(env, j_metadata));

  return EntityMetadataAndroid(std::move(guid), date_modified, use_count,
                               use_date);
}

EntityMetadataAndroid::EntityMetadataAndroid(std::string guid,
                                             base::Time date_modified,
                                             int use_count,
                                             base::Time use_date)
    : guid(std::move(guid)),
      date_modified(date_modified),
      use_count(use_count),
      use_date(use_date) {}

EntityMetadataAndroid::EntityMetadataAndroid(const EntityMetadataAndroid&) =
    default;

EntityMetadataAndroid& EntityMetadataAndroid::operator=(
    const EntityMetadataAndroid&) = default;

EntityMetadataAndroid::EntityMetadataAndroid(EntityMetadataAndroid&&) = default;

EntityMetadataAndroid& EntityMetadataAndroid::operator=(
    EntityMetadataAndroid&&) = default;

EntityMetadataAndroid::~EntityMetadataAndroid() = default;

}  // namespace autofill
