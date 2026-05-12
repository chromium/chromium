// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/entity_instance_android.h"

#include <optional>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/autofill/android/main_autofill_jni_headers/EntityInstance_jni.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"

namespace autofill {

base::android::ScopedJavaLocalRef<jobject> EntityInstanceAndroid::Create(
    JNIEnv* env,
    const EntityInstanceAndroid& entity_instance,
    bool requires_reauth_to_see,
    bool is_masked_server_entity) {
  return Java_EntityInstance_Constructor(
      env, static_cast<int>(entity_instance.record_type),
      entity_instance.entity_type, entity_instance.attribute_instances,
      entity_instance.nickname, entity_instance.metadata,
      requires_reauth_to_see, is_masked_server_entity);
}

EntityInstanceAndroid EntityInstanceAndroid::FromJavaEntityInstance(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_entity_instance) {
  EntityInstance::RecordType record_type =
      static_cast<EntityInstance::RecordType>(
          Java_EntityInstance_getRecordType(env, j_entity_instance));
  EntityTypeAndroid entity_type =
      Java_EntityInstance_getEntityType(env, j_entity_instance);

  std::vector<AttributeInstanceAndroid> attributes =
      Java_EntityInstance_getAttributes(env, j_entity_instance);

  std::string nickname =
      Java_EntityInstance_getNickname(env, j_entity_instance);

  EntityMetadataAndroid metadata =
      EntityMetadataAndroid::FromJavaEntityMetadata(
          env, Java_EntityInstance_getMetadata(env, j_entity_instance));

  bool requires_reauth_to_see =
      Java_EntityInstance_requiresReauthToSee(env, j_entity_instance);

  bool is_masked_server_entity =
      Java_EntityInstance_isMaskedServerEntity(env, j_entity_instance);

  return EntityInstanceAndroid(std::move(entity_type), record_type,
                               std::move(attributes), std::move(nickname),
                               std::move(metadata), requires_reauth_to_see,
                               is_masked_server_entity);
}

EntityInstanceAndroid::EntityInstanceAndroid(
    const EntityInstance& entity_instance,
    bool is_enabled,
    bool is_eligible_for_wallet_storage,
    bool requires_reauth_to_see)
    : entity_type(entity_instance.type(),
                  is_enabled,
                  is_eligible_for_wallet_storage,
                  IsMaskedStorageSupported(entity_instance.type(),
                                           entity_instance.record_type())),
      record_type(entity_instance.record_type()),
      nickname(entity_instance.nickname()),
      metadata(entity_instance.guid().value(),
               entity_instance.date_modified(),
               entity_instance.use_count(),
               entity_instance.use_date()),
      requires_reauth_to_see(requires_reauth_to_see),
      is_masked_server_entity(entity_instance.IsMaskedEntity() &&
                              entity_instance.IsServerInstance()) {
  for (const auto& attr : entity_instance.attributes()) {
    attribute_instances.emplace_back(attr);
  }
}

EntityInstanceAndroid::EntityInstanceAndroid(
    EntityTypeAndroid entity_type,
    EntityInstance::RecordType record_type,
    std::vector<AttributeInstanceAndroid> attribute_instances,
    std::string nickname,
    EntityMetadataAndroid metadata,
    bool requires_reauth_to_see,
    bool is_masked_server_entity)
    : entity_type(std::move(entity_type)),
      record_type(record_type),
      attribute_instances(std::move(attribute_instances)),
      nickname(std::move(nickname)),
      metadata(std::move(metadata)),
      requires_reauth_to_see(requires_reauth_to_see),
      is_masked_server_entity(is_masked_server_entity) {}

EntityInstanceAndroid::EntityInstanceAndroid(const EntityInstanceAndroid&) =
    default;
EntityInstanceAndroid::EntityInstanceAndroid(EntityInstanceAndroid&&) = default;
EntityInstanceAndroid::~EntityInstanceAndroid() = default;

EntityInstance EntityInstanceAndroid::ToEntityInstance(
    base::optional_ref<const EntityInstance> existing_entity) const {
  CHECK(!existing_entity || existing_entity->guid().value() == metadata.guid);

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
          metadata.guid.empty()
              ? base::Uuid::GenerateRandomV4().AsLowercaseString()
              : metadata.guid),
      nickname, metadata.date_modified, metadata.use_count, metadata.use_date,
      record_type, EntityInstance::AreAttributesReadOnly(false),
      /*frecency_override=*/"");
}

}  // namespace autofill
