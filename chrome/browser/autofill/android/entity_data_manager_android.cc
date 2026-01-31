// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/entity_data_manager_android.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/check_deref.h"
#include "base/types/zip.h"
#include "chrome/browser/autofill/android/entity_instance_android.h"
#include "chrome/browser/autofill/android/entity_type_android.h"
#include "chrome/browser/autofill/autofill_entity_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_labels.h"
#include "components/autofill/core/browser/integrators/autofill_ai/management_utils.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/autofill/android/jni_headers/EntityDataManager_jni.h"
#include "components/autofill/android/main_autofill_jni_headers/EntityInstanceWithLabels_jni.h"

namespace autofill {

EntityDataManagerAndroid::EntityDataManagerAndroid(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj,
    EntityDataManager* entity_data_manager)
    : weak_java_obj_(env, obj),
      entity_data_manager_(CHECK_DEREF(entity_data_manager)) {}

EntityDataManagerAndroid::~EntityDataManagerAndroid() = default;

static int64_t JNI_EntityDataManager_Init(JNIEnv* env,
                                          const jni_zero::JavaRef<jobject>& obj,
                                          Profile* profile) {
  CHECK(profile);
  EntityDataManagerAndroid* entity_data_manager_android =
      new EntityDataManagerAndroid(
          env, obj, AutofillEntityDataManagerFactory::GetForProfile(profile));
  return reinterpret_cast<intptr_t>(entity_data_manager_android);
}

void EntityDataManagerAndroid::Destroy(JNIEnv* env) {
  delete this;
}

jni_zero::ScopedJavaLocalRef<jobject>
EntityDataManagerAndroid::GetEntityInstance(const std::string& guid) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  base::optional_ref<const EntityInstance> entity =
      entity_data_manager_->GetEntityInstance(EntityInstance::EntityId(guid));
  if (!entity) {
    return nullptr;
  }

  return EntityInstanceAndroid::Create(env, EntityInstanceAndroid(*entity));
}

void EntityDataManagerAndroid::RemoveEntityInstance(JNIEnv* env,
                                                    const std::string& guid) {
  entity_data_manager().RemoveEntityInstance(EntityInstance::EntityId(guid));
}

void EntityDataManagerAndroid::AddOrUpdateEntityInstance(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& jEntity) {
  EntityInstanceAndroid entity_android =
      EntityInstanceAndroid::FromJavaEntityInstance(env, jEntity);

  entity_data_manager().AddOrUpdateEntityInstance(
      entity_android.ToEntityInstance(entity_data_manager_->GetEntityInstance(
          EntityInstance::EntityId(entity_android.guid))));
}

jni_zero::ScopedJavaLocalRef<jobjectArray>
EntityDataManagerAndroid::GetEntitiesWithLabels(JNIEnv* env) {
  // Entity labels should be generated based on other entities of the same
  // type. This is because the disambiguation values of attributes are only
  // relevant inside a specific entity type.
  base::span<const EntityInstance> entities =
      entity_data_manager().GetEntityInstances();
  std::map<EntityType, std::vector<const EntityInstance*>> entities_per_type;
  for (const EntityInstance& entity : entities) {
    entities_per_type[entity.type()].push_back(&entity);
  }

  std::vector<jni_zero::ScopedJavaLocalRef<jobject>> j_entities;
  j_entities.reserve(entities.size());
  for (const auto& [type, entities_of_type] : entities_per_type) {
    std::vector<EntityLabel> labels =
        GetLabelsForEntities(entities_of_type,
                             /*attribute_types_to_ignore=*/
                             {},
                             /*only_disambiguating_types=*/
                             false, /*obfuscate_sensitive_types=*/true,
                             g_browser_process->GetApplicationLocale());
    CHECK_EQ(entities_of_type.size(), labels.size());

    for (const auto [entity, label] : base::zip(entities_of_type, labels)) {
      j_entities.push_back(Java_EntityInstanceWithLabels_Constructor(
          env, entity->guid().value(), entity->type().GetNameForI18n(),
          base::JoinString(label, kLabelSeparator),
          entity->record_type() == EntityInstance::RecordType::kServerWallet));
    }
  }
  return base::android::ToJavaArrayOfObjects(env, j_entities);
}

std::vector<EntityTypeAndroid> EntityDataManagerAndroid::GetWritableEntityTypes(
    JNIEnv* env) {
  std::vector<EntityTypeAndroid> entity_types;
  for (EntityType entity_type : autofill::GetWritableEntityTypes(
           entity_data_manager_->GetVariationCountryCode())) {
    entity_types.emplace_back(EntityTypeAndroid(entity_type));
  }
  return entity_types;
}

}  // namespace autofill

DEFINE_JNI(EntityDataManager)
