// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/entity_data_manager_android.h"

#include <algorithm>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/check_deref.h"
#include "base/containers/to_vector.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/zip.h"
#include "chrome/browser/autofill/account_setting_service_factory.h"
#include "chrome/browser/autofill/android/entity_instance_android.h"
#include "chrome/browser/autofill/android/entity_instance_with_labels.h"
#include "chrome/browser/autofill/android/entity_type_android.h"
#include "chrome/browser/autofill/autofill_entity_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/variations/google_groups_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_labels.h"
#include "components/autofill/core/browser/integrators/autofill_ai/management_utils.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#include "components/autofill/core/browser/webdata/account_settings/account_setting_service.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/autofill/android/jni_headers/EntityDataManager_jni.h"

namespace autofill {

EntityDataManagerAndroid::EntityDataManagerAndroid(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj,
    const GoogleGroupsManager* google_groups_manager,
    PrefService* prefs,
    const signin::IdentityManager* identity_manager,
    const syncer::SyncService* sync_service,
    const AccountSettingService* account_setting_service,
    bool is_off_the_record,
    EntityDataManager* entity_data_manager)
    : weak_java_obj_(env, obj),
      google_groups_manager_(google_groups_manager),
      prefs_(prefs),
      identity_manager_(identity_manager),
      sync_service_(sync_service),
      account_setting_service_(account_setting_service),
      is_off_the_record_(is_off_the_record),
      entity_data_manager_(CHECK_DEREF(entity_data_manager)) {}

EntityDataManagerAndroid::~EntityDataManagerAndroid() = default;

static int64_t JNI_EntityDataManager_Init(JNIEnv* env,
                                          const jni_zero::JavaRef<jobject>& obj,
                                          Profile* profile) {
  CHECK(profile);
  EntityDataManager* entity_data_manager =
      AutofillEntityDataManagerFactory::GetForProfile(profile);
  if (!entity_data_manager) {
    return 0;
  }

  EntityDataManagerAndroid* entity_data_manager_android =
      new EntityDataManagerAndroid(
          env, obj, GoogleGroupsManagerFactory::GetForBrowserContext(profile),
          profile->GetPrefs(), IdentityManagerFactory::GetForProfile(profile),
          SyncServiceFactory::GetForProfile(profile),
          AccountSettingServiceFactory::GetForBrowserContext(profile),
          profile->IsOffTheRecord(), entity_data_manager);
  return reinterpret_cast<intptr_t>(entity_data_manager_android);
}

void EntityDataManagerAndroid::Destroy(JNIEnv* env) {
  delete this;
}

bool EntityDataManagerAndroid::IsEligibleToAutofillAi(JNIEnv* env) {
  const bool is_wallet_storage_enabled =
      account_setting_service_ &&
      account_setting_service_->IsWalletPrivacyContextualSurfacingEnabled();

  return MayPerformAutofillAiAction(
      google_groups_manager_, prefs_, &entity_data_manager(), identity_manager_,
      sync_service_, is_wallet_storage_enabled, is_off_the_record_,
      entity_data_manager_->GetVariationCountryCode(),
      AutofillAiAction::kOptIn);
}

bool EntityDataManagerAndroid::GetAutofillAiOptInStatus(JNIEnv* env) {
  return autofill::GetAutofillAiOptInStatus(prefs_, identity_manager_);
}

bool EntityDataManagerAndroid::SetAutofillAiOptInStatus(
    JNIEnv* env,
    AutofillAiOptInStatus opt_in_status) {
  const bool is_wallet_storage_enabled =
      account_setting_service_ &&
      account_setting_service_->IsWalletPrivacyContextualSurfacingEnabled();

  return autofill::SetAutofillAiOptInStatus(
      google_groups_manager_, prefs_, &entity_data_manager(), identity_manager_,
      sync_service_, is_wallet_storage_enabled, is_off_the_record_,
      entity_data_manager_->GetVariationCountryCode(), opt_in_status);
}

jni_zero::ScopedJavaLocalRef<jobject>
EntityDataManagerAndroid::GetEntityInstance(JNIEnv* env,
                                            const std::string& guid) {
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

std::vector<EntityInstanceWithLabels>
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

  std::vector<EntityInstanceWithLabels> entities_with_labels;
  entities_with_labels.reserve(entities.size());
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
      entities_with_labels.emplace_back(
          entity->guid().value(), entity->type().GetNameForI18n(),
          base::JoinString(label, kLabelSeparator),
          entity->record_type() == EntityInstance::RecordType::kServerWallet);
    }
  }
  return entities_with_labels;
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

std::vector<EntityTypeAndroid>
EntityDataManagerAndroid::GetSortedEntityTypesForListDisplay(
    JNIEnv* env) const {
  std::vector<EntityType> all_types =
      base::ToVector(DenseSet<EntityType>::all());
  std::ranges::sort(all_types, EntityType::ListOrder);
  return base::ToVector(all_types, [](const EntityType& type) {
    return EntityTypeAndroid(type);
  });
}

void EntityDataManagerAndroid::OnEntityInstancesChanged() {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto java_obj = weak_java_obj_.get(env);
  if (java_obj.is_null()) {
    return;
  }
  Java_EntityDataManager_onEntityInstancesChanged(env, java_obj);
}

}  // namespace autofill

DEFINE_JNI(EntityDataManager)
