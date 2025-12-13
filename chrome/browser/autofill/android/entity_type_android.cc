// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/entity_type_android.h"

#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/android/main_autofill_jni_headers/EntityType_jni.h"

namespace autofill {

base::android::ScopedJavaLocalRef<jobject> EntityTypeAndroid::Create(
    JNIEnv* env,
    const EntityTypeAndroid& entity_type) {
  return Java_EntityType_Constructor(
      env, static_cast<jint>(entity_type.type_name), entity_type.is_read_only,
      entity_type.type_name_as_string, entity_type.add_entity_type_string,
      entity_type.edit_entity_type_string,
      entity_type.delete_entity_type_string);
}

EntityTypeAndroid EntityTypeAndroid::FromJavaEntityType(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_entity_type) {
  if (!j_entity_type) {
    return EntityTypeAndroid(EntityType(static_cast<EntityTypeName>(0)));
  }
  return EntityTypeAndroid(EntityType(static_cast<EntityTypeName>(
      Java_EntityType_getTypeName(env, j_entity_type))));
}

EntityTypeAndroid::EntityTypeAndroid(const EntityType& entity_type)
    : type_name(entity_type.name()),
      is_read_only(entity_type.read_only()),
      type_name_as_string(entity_type.GetNameForI18n()),
      // Note that the following string values are not relevant in the C++ side,
      // it is only used by the UI.
      add_entity_type_string(u""),
      edit_entity_type_string(u""),
      delete_entity_type_string(u"") {}

EntityTypeAndroid::EntityTypeAndroid(EntityTypeName type_name,
                                     bool is_read_only,
                                     std::u16string type_name_as_string,
                                     std::u16string add_entity_type_string,
                                     std::u16string edit_entity_type_string,
                                     std::u16string delete_entity_type_string)
    : type_name(type_name),
      is_read_only(is_read_only),
      type_name_as_string(std::move(type_name_as_string)),
      add_entity_type_string(std::move(add_entity_type_string)),
      edit_entity_type_string(std::move(edit_entity_type_string)),
      delete_entity_type_string(std::move(delete_entity_type_string)) {}

EntityTypeAndroid::EntityTypeAndroid(const EntityTypeAndroid&) = default;

EntityTypeAndroid& EntityTypeAndroid::operator=(const EntityTypeAndroid&) =
    default;

EntityTypeAndroid::EntityTypeAndroid(EntityTypeAndroid&&) = default;

EntityTypeAndroid& EntityTypeAndroid::operator=(EntityTypeAndroid&&) = default;

EntityTypeAndroid::~EntityTypeAndroid() = default;

EntityType EntityTypeAndroid::ToEntityType() const {
  return EntityType(type_name);
}

}  // namespace autofill
