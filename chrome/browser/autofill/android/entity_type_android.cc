// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/entity_type_android.h"

#include "base/android/jni_string.h"
#include "base/check_op.h"
#include "base/containers/to_vector.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/android/main_autofill_jni_headers/EntityType_jni.h"
#include "components/autofill/core/browser/integrators/autofill_ai/management_utils.h"
#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_metrics.h"
#include "components/autofill/core/common/dense_set.h"

namespace autofill {

base::android::ScopedJavaLocalRef<jobject> EntityTypeAndroid::Create(
    JNIEnv* env,
    const EntityTypeAndroid& entity_type) {
  return Java_EntityType_Constructor(
      env, static_cast<int32_t>(entity_type.type_name),
      entity_type.is_read_only, entity_type.is_enabled,
      entity_type.type_name_as_string, entity_type.type_name_as_metrics_string,
      entity_type.add_entity_type_string, entity_type.edit_entity_type_string,
      entity_type.delete_entity_type_string, entity_type.attribute_types,
      entity_type.required_types);
}

EntityTypeAndroid EntityTypeAndroid::FromJavaEntityType(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& j_entity_type) {
  if (!j_entity_type) {
    return EntityTypeAndroid(EntityType(static_cast<EntityTypeName>(0)), false);
  }
  return EntityTypeAndroid(
      EntityType(static_cast<EntityTypeName>(
          Java_EntityType_getTypeName(env, j_entity_type))),
      Java_EntityType_isEnabled(env, j_entity_type));
}

EntityTypeAndroid::EntityTypeAndroid(const EntityType& entity_type,
                                     bool is_enabled)
    : type_name(entity_type.name()),
      is_read_only(entity_type.read_only()),
      is_enabled(is_enabled),
      type_name_as_string(entity_type.GetNameForI18n()),
      type_name_as_metrics_string(EntityTypeToMetricsString(entity_type)),
      add_entity_type_string(GetAddEntityTypeStringForI18n(entity_type)),
      edit_entity_type_string(GetEditEntityTypeStringForI18n(entity_type)),
      delete_entity_type_string(GetDeleteEntityTypeStringForI18n(entity_type)),
      attribute_types(base::ToVector(entity_type.attributes(),
                                     [](const AttributeType& attr) {
                                       return AttributeTypeAndroid(attr);
                                     })),
      required_types(
          base::ToVector(entity_type.import_constraints(),
                         [](autofill::DenseSet<AttributeType> group) {
                           // It was decided to keep the schema expressive to
                           // allow future complex
                           // constraints, rather than restricting it to match
                           // current UI capabilities. Consequently, this code
                           // enforces the current UI limitation (simple
                           // disjunctions only) via runtime checks. Ex.
                           // "[[a]]", "[[a], [b]]", "[[a], [b], [c]]" etc are
                           // supported in UI. More details here:
                           // crrev.com/c/7245980
                           CHECK_EQ(group.size(), 1u)
                               << "Unsupported format: Complex constraint "
                                  "groups not supported by UI";

                           return AttributeTypeAndroid(*group.begin());
                         })) {}

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
