// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/entity_instance_with_labels.h"

#include "base/android/jni_string.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/autofill/android/main_autofill_jni_headers/EntityInstanceWithLabels_jni.h"

namespace autofill {

EntityInstanceWithLabels::EntityInstanceWithLabels(
    std::string guid,
    std::u16string entity_instance_label,
    std::u16string entity_instance_sublabel,
    bool stored_in_wallet)
    : guid(std::move(guid)),
      entity_instance_label(std::move(entity_instance_label)),
      entity_instance_sublabel(std::move(entity_instance_sublabel)),
      stored_in_wallet(stored_in_wallet) {}
EntityInstanceWithLabels::~EntityInstanceWithLabels() = default;
EntityInstanceWithLabels::EntityInstanceWithLabels(
    const EntityInstanceWithLabels&) = default;
EntityInstanceWithLabels& EntityInstanceWithLabels::operator=(
    const EntityInstanceWithLabels&) = default;
EntityInstanceWithLabels::EntityInstanceWithLabels(EntityInstanceWithLabels&&) =
    default;
EntityInstanceWithLabels& EntityInstanceWithLabels::operator=(
    EntityInstanceWithLabels&&) = default;

}  // namespace autofill

namespace jni_zero {

using autofill::EntityInstanceWithLabels;

template <>
EntityInstanceWithLabels FromJniType<EntityInstanceWithLabels>(
    JNIEnv* env,
    const JavaRef<jobject>& jobj) {
  EntityInstanceWithLabels instance(
      autofill::Java_EntityInstanceWithLabels_getGuid(env, jobj),
      autofill::Java_EntityInstanceWithLabels_getEntityInstanceLabel(env, jobj),
      autofill::Java_EntityInstanceWithLabels_getEntityInstanceSubLabel(env,
                                                                        jobj),
      autofill::Java_EntityInstanceWithLabels_isStoredInWallet(env, jobj));
  return instance;
}

template <>
ScopedJavaLocalRef<jobject> ToJniType<EntityInstanceWithLabels>(
    JNIEnv* env,
    const EntityInstanceWithLabels& native_instance) {
  return autofill::Java_EntityInstanceWithLabels_Constructor(
      env, native_instance.guid, native_instance.entity_instance_label,
      native_instance.entity_instance_sublabel,
      native_instance.stored_in_wallet);
}

}  // namespace jni_zero
