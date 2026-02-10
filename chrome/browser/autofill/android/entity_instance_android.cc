// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/entity_instance_android.h"

#include <optional>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/autofill/android/main_autofill_jni_headers/EntityInstance_jni.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"

namespace autofill {

base::android::ScopedJavaLocalRef<jobject> EntityInstanceAndroid::Create(
    JNIEnv* env,
    const EntityInstanceAndroid& entity_instance) {
  return Java_EntityInstance_Constructor(
      env, entity_instance.guid, static_cast<int>(entity_instance.record_type),
      entity_instance.entity_type, entity_instance.attribute_instances,
      entity_instance.metadata);
}

EntityInstanceAndroid EntityInstanceAndroid::FromJavaEntityInstance(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_entity_instance) {
  std::string guid = Java_EntityInstance_getGUID(env, j_entity_instance);
  EntityInstance::RecordType record_type =
      static_cast<EntityInstance::RecordType>(
          Java_EntityInstance_getRecordType(env, j_entity_instance));
  EntityTypeAndroid entity_type =
      Java_EntityInstance_getEntityType(env, j_entity_instance);

  std::vector<AttributeInstanceAndroid> attributes =
      Java_EntityInstance_getAttributes(env, j_entity_instance);

  EntityMetadataAndroid metadata =
      EntityMetadataAndroid::FromJavaEntityMetadata(
          env, Java_EntityInstance_getMetadata(env, j_entity_instance));

  return EntityInstanceAndroid(std::move(entity_type), std::move(guid),
                               record_type, std::move(attributes),
                               std::move(metadata));
}

EntityInstanceAndroid::EntityInstanceAndroid(
    const EntityInstance& entity_instance)
    : entity_type(entity_instance.type()),
      guid(*entity_instance.guid()),
      record_type(entity_instance.record_type()),
      metadata(entity_instance.date_modified(),
               static_cast<int>(entity_instance.use_count())) {
  for (const auto& attr : entity_instance.attributes()) {
    attribute_instances.emplace_back(attr);
  }
}

EntityInstanceAndroid::EntityInstanceAndroid(
    EntityTypeAndroid entity_type,
    std::string guid,
    EntityInstance::RecordType record_type,
    std::vector<AttributeInstanceAndroid> attribute_instances,
    EntityMetadataAndroid metadata)
    : entity_type(std::move(entity_type)),
      guid(std::move(guid)),
      record_type(record_type),
      attribute_instances(std::move(attribute_instances)),
      metadata(std::move(metadata)) {}

EntityInstanceAndroid::EntityInstanceAndroid(const EntityInstanceAndroid&) =
    default;
EntityInstanceAndroid::EntityInstanceAndroid(EntityInstanceAndroid&&) = default;
EntityInstanceAndroid::~EntityInstanceAndroid() = default;

EntityInstance EntityInstanceAndroid::ToEntityInstance(
    base::optional_ref<const EntityInstance> existing_entity) const {
  CHECK(!existing_entity || existing_entity->guid().value() == guid);

  base::flat_set<AttributeInstance, AttributeInstance::CompareByType>
      attributes_set;

  for (const AttributeInstanceAndroid& attr : attribute_instances) {
    AttributeInstance maybe_new_attribute = attr.ToAttributeInstance();
    base::optional_ref<const AttributeInstance> existing_attribute_instance =
        existing_entity ? existing_entity->attribute(maybe_new_attribute.type())
                        : std::nullopt;
    const std::optional<std::u16string> existing_attribute_raw_value =
        existing_attribute_instance
            ? std::optional<std::u16string>(
                  existing_attribute_instance->GetCompleteRawInfo())
            : std::nullopt;
    const std::u16string maybe_new_attribute_raw_value =
        maybe_new_attribute.GetCompleteRawInfo();

    // Only use the attribute instance coming from the management page if it is
    // new or it has actually changed. This guarantees information such
    // sub-types definitions (such as NAME_FIRST, NAME_LAST etc) are not missed.
    if (!existing_attribute_raw_value ||
        maybe_new_attribute_raw_value != existing_attribute_raw_value.value()) {
      attributes_set.insert(attr.ToAttributeInstance());
    } else {
      attributes_set.insert(
          AttributeInstance(existing_attribute_instance.value()));
    }
  }

  return EntityInstance(
      entity_type.ToEntityType(), std::move(attributes_set),
      EntityInstance::EntityId(
          guid.empty() ? base::Uuid::GenerateRandomV4().AsLowercaseString()
                       : guid),
      /*nickname=*/"", metadata.date_modified, metadata.use_count, base::Time(),
      record_type, EntityInstance::AreAttributesReadOnly(false),
      /*frecency_override=*/"");
}

}  // namespace autofill
