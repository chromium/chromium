// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/android/entity_data_manager_android.h"

#include <algorithm>
#include <optional>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/check_deref.h"
#include "base/containers/to_vector.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/zip.h"
#include "chrome/browser/account_settings/account_setting_service_factory.h"
#include "chrome/browser/autofill/android/entity_instance_android.h"
#include "chrome/browser/autofill/android/entity_instance_with_labels.h"
#include "chrome/browser/autofill/android/entity_type_android.h"
#include "chrome/browser/autofill/autofill_entity_data_manager_factory.h"
#include "chrome/browser/autofill/wallet_pass_access_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/metrics/variations/google_groups_manager_factory.h"
#include "chrome/browser/personal_context/personal_context_enablement_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/accessibility_annotator/core/url_constants.h"
#include "components/account_settings/account_setting_service.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_labels.h"
#include "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_wallet_utils.h"
#include "components/autofill/core/browser/integrators/autofill_ai/management_utils.h"
#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_metrics.h"
#include "components/autofill/core/browser/network/autofill_ai/wallet_pass_access_manager.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/personal_context/core/personal_context_enablement_service.h"
#include "components/personal_context/core/personal_context_features.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/wallet/core/common/wallet_features.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/autofill/android/jni_headers/EntityDataManager_jni.h"
#include "components/autofill/android/main_autofill_jni_headers/EntityInstance_jni.h"

namespace autofill {

EntityDataManagerAndroid::EntityDataManagerAndroid(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj,
    const GoogleGroupsManager* google_groups_manager,
    PrefService* prefs,
    signin::IdentityManager* identity_manager,
    const syncer::SyncService* sync_service,
    const account_settings::AccountSettingService* account_setting_service,
    consent_auditor::ConsentAuditor* consent_auditor,
    bool is_off_the_record,
    WalletPassAccessManager* wallet_pass_access_manager,
    EntityDataManager* entity_data_manager)
    : weak_java_obj_(env, obj),
      google_groups_manager_(google_groups_manager),
      prefs_(prefs),
      identity_manager_(identity_manager),
      sync_service_(sync_service),
      account_setting_service_(account_setting_service),
      consent_auditor_(consent_auditor),
      is_off_the_record_(is_off_the_record),
      wallet_pass_access_manager_(wallet_pass_access_manager),
      entity_data_manager_(CHECK_DEREF(entity_data_manager)) {
  entity_data_manager_observer_.Observe(entity_data_manager);
}

EntityDataManagerAndroid::~EntityDataManagerAndroid() = default;

static jboolean JNI_EntityDataManager_IsAccessibilityAnnotatorSettingVisible(
    JNIEnv* env,
    Profile* profile) {
  CHECK(profile);

  if (!base::FeatureList::IsEnabled(
          personal_context::features::kPersonalContext)) {
    return false;
  }

  personal_context::PersonalContextEnablementService* enablement_service =
      PersonalContextEnablementServiceFactory::GetForProfile(profile);
  return enablement_service &&
         enablement_service->GetEnablementState() ==
             personal_context::PersonalContextEnablementState::kEnabled;
}

static std::string JNI_EntityDataManager_GetAccessibilityAnnotatorSettingsUrl(
    JNIEnv* env) {
  return accessibility_annotator::kAccessibilityAnnotatorSettingsURL;
}

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
          ConsentAuditorFactory::GetForProfile(profile),
          profile->IsOffTheRecord(),
          WalletPassAccessManagerFactory::GetForProfile(profile),
          entity_data_manager);
  return reinterpret_cast<intptr_t>(entity_data_manager_android);
}

void EntityDataManagerAndroid::Destroy(JNIEnv* env) {
  delete this;
}

bool EntityDataManagerAndroid::IsEligibleToAutofillAi(JNIEnv* env) {
  return RunMayPerformAutofillAiAction(AutofillAiAction::kOptIn,
                                       /*entity_type=*/std::nullopt);
}

bool EntityDataManagerAndroid::GetAutofillAiOptInStatus(JNIEnv* env) {
  return autofill::GetAutofillAiOptInStatus(prefs_, identity_manager_);
}

bool EntityDataManagerAndroid::SetAutofillAiOptInStatus(
    JNIEnv* env,
    AutofillAiOptInStatus opt_in_status) {
  const bool is_wallet_public_pass_storage_enabled =
      account_setting_service_ &&
      account_setting_service_
          ->GetBoolean(account_settings::kWalletPrivacyContextualSurfacing)
          .value_or(false);

  return autofill::SetAutofillAiOptInStatus(
      google_groups_manager_, prefs_, &entity_data_manager(), identity_manager_,
      sync_service_, is_wallet_public_pass_storage_enabled, is_off_the_record_,
      entity_data_manager_->GetVariationCountryCode(), opt_in_status);
}

std::optional<EntityInstanceAndroid>
EntityDataManagerAndroid::GetEntityInstance(JNIEnv* env,
                                            const std::string& guid) {
  base::optional_ref<const EntityInstance> entity =
      entity_data_manager_->GetEntityInstance(EntityInstance::EntityId(guid));
  if (!entity) {
    return std::nullopt;
  }

  const bool requires_reauth_to_see =
      base::FeatureList::IsEnabled(features::kAutofillAiReauthRequired) &&
      prefs::IsAutofillAiReauthBeforeFillingEnabled(prefs_) &&
      std::ranges::any_of(
          entity->attributes(),
          [](const AttributeInstance& attribute_instance) {
            return attribute_instance.type().is_obfuscated() &&
                   !attribute_instance.GetCompleteRawInfo().empty();
          });
  return EntityInstanceAndroid(
      *entity,
      entity->type().enabled(entity_data_manager_->GetVariationCountryCode()),
      IsEligibleForWalletStorage(entity->type()), requires_reauth_to_see);
}

void EntityDataManagerAndroid::RemoveEntityInstance(JNIEnv* env,
                                                    const std::string& guid) {
  const EntityInstance::EntityId entity_id(guid);
  if (base::optional_ref<const EntityInstance> entity =
          entity_data_manager().GetEntityInstance(entity_id)) {
    LogEntityDeletedFromSettings(entity->type(), entity->record_type());
    entity_data_manager().RemoveEntityInstance(entity_id);
  }
}

void EntityDataManagerAndroid::AddOrUpdateEntityInstance(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& jEntity,
    int32_t description_string_id,
    int32_t accept_button_string_id,
    base::OnceClosure on_local_save_fallback) {
  EntityInstanceAndroid entity_android =
      EntityInstanceAndroid::FromJavaEntityInstance(env, jEntity);

  EntityInstance::RecordType targeted_record_type = entity_android.record_type;
  entity_android.record_type =
      (IsEligibleForWalletStorage(entity_android.entity_type.ToEntityType()) &&
       entity_android.record_type == EntityInstance::RecordType::kServerWallet)
          ? EntityInstance::RecordType::kServerWallet
          : EntityInstance::RecordType::kLocal;
  EntityInstance entity_instance =
      entity_android.ToEntityInstance(entity_data_manager_->GetEntityInstance(
          EntityInstance::EntityId(entity_android.guid)));

  AddOrUpdateEntityInstance(std::move(entity_instance), targeted_record_type,
                            description_string_id, accept_button_string_id,
                            std::move(on_local_save_fallback));
}

void EntityDataManagerAndroid::AddOrUpdateEntityInstance(
    EntityInstance entity_instance,
    EntityInstance::RecordType targeted_record_type,
    int description_string_id,
    int accept_button_string_id,
    base::OnceClosure on_local_save_fallback) {
  const bool is_new_entity =
      !entity_data_manager().GetEntityInstance(entity_instance.guid());
  if (is_new_entity) {
    LogEntityAddedFromSettings(entity_instance.type(),
                               entity_instance.record_type());
  } else {
    LogEntityUpdatedFromSettings(entity_instance.type(),
                                 entity_instance.record_type());
  }

  if (base::FeatureList::IsEnabled(features::kAutofillAiWalletPrivatePasses)) {
    const bool is_masked_storage_supported = IsMaskedStorageSupported(
        entity_instance.type(), entity_instance.record_type());
    // Wallet passes are strictly read-only from the client's perspective in
    // settings. Therefore, we only ever "Save" them. Any downstream "Update"
    // attempts are inapplicable.
    if (is_masked_storage_supported) {
      consent_auditor::ConsentAuditor::SessionId session_id;
      if (base::FeatureList::IsEnabled(
              wallet::features::kWalletApiPrivatePassesConsent)) {
        session_id = RecordWalletPrivatePassConsent(
            description_string_id, accept_button_string_id, *consent_auditor_,
            *identity_manager_);
      }
      wallet_pass_access_manager_->SaveWalletEntityInstance(
          entity_instance, session_id,
          base::BindOnce(
              &EntityDataManagerAndroid::OnSavePrivatePassToWalletFinished,
              weak_ptr_factory_.GetWeakPtr(), std::move(on_local_save_fallback),
              entity_instance));
    } else {
      // If `IsMaskedStorageSupported` returns true for
      // `entity_instance.type()` and `targeted_record_type` the user
      // initially wanted to store the entity on the server but became
      // ineligible.
      if (IsMaskedStorageSupported(entity_instance.type(),
                                   targeted_record_type)) {
        std::move(on_local_save_fallback).Run();
      }
      entity_data_manager().AddOrUpdateEntityInstance(
          std::move(entity_instance));
    }
  } else {
    entity_data_manager().AddOrUpdateEntityInstance(std::move(entity_instance));
  }
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
      const bool stored_in_wallet =
          entity->record_type() == EntityInstance::RecordType::kServerWallet;
      entities_with_labels.emplace_back(
          entity->guid().value(),
          EntityTypeAndroid(
              type,
              type.enabled(entity_data_manager_->GetVariationCountryCode()),
              IsEligibleForWalletStorage(type),
              IsMaskedStorageSupported(type, entity->record_type())),
          entity->type().GetNameForI18n(),
          base::JoinString(label, kLabelSeparator), stored_in_wallet,
          stored_in_wallet ? std::make_optional(GetWalletManagementURL(*entity))
                           : std::nullopt);
    }
  }
  return entities_with_labels;
}

std::vector<EntityTypeAndroid> EntityDataManagerAndroid::GetWritableEntityTypes(
    JNIEnv* env) {
  std::vector<EntityTypeAndroid> entity_types;
  for (EntityType entity_type : autofill::GetWritableEntityTypes(
           entity_data_manager_->GetVariationCountryCode())) {
    entity_types.emplace_back(
        entity_type,
        entity_type.enabled(entity_data_manager_->GetVariationCountryCode()),
        IsEligibleForWalletStorage(entity_type),
        IsMaskedStorageSupported(entity_type,
                                 EntityInstance::RecordType::kServerWallet));
  }
  return entity_types;
}

std::vector<EntityTypeAndroid>
EntityDataManagerAndroid::GetSortedEntityTypesForListDisplay(
    JNIEnv* env) const {
  std::vector<EntityType> all_types =
      base::ToVector(DenseSet<EntityType>::all());
  std::ranges::sort(all_types, EntityType::ListOrder);
  return base::ToVector(all_types, [this](const EntityType& type) {
    return EntityTypeAndroid(
        type, type.enabled(entity_data_manager_->GetVariationCountryCode()),
        IsEligibleForWalletStorage(type),
        IsMaskedStorageSupported(type,
                                 EntityInstance::RecordType::kServerWallet));
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

bool EntityDataManagerAndroid::GetIsAutofillAiDisabledByEnterprisePolicy(
    JNIEnv* env) {
  return IsAutofillAiDisabledByEnterprisePolicy(prefs_);
}

bool EntityDataManagerAndroid::GetIsAutofillAiAllowedByEnterprisePolicy(
    JNIEnv* env) {
  return IsAutofillAiAllowedByEnterprisePolicy(prefs_);
}

bool EntityDataManagerAndroid::CanEnableOrDisableAutofillAi(JNIEnv* env) {
  return RunMayPerformAutofillAiAction(AutofillAiAction::kEnableOrDisable,
                                       /*entity_type=*/std::nullopt);
}

bool EntityDataManagerAndroid::CanListEntityInstancesInSettings(JNIEnv* env) {
  return RunMayPerformAutofillAiAction(
      AutofillAiAction::kListEntityInstancesInSettings,
      /*entity_type=*/std::nullopt);
}

bool EntityDataManagerAndroid::IsWalletPublicPassStorageEnabledHelper() const {
  return account_setting_service_ &&
         account_setting_service_
             ->GetBoolean(account_settings::kWalletPrivacyContextualSurfacing)
             .value_or(false);
}

bool EntityDataManagerAndroid::IsWalletPublicPassStorageEnabled(JNIEnv* env) {
  return IsWalletPublicPassStorageEnabledHelper();
}

bool EntityDataManagerAndroid::RunMayPerformAutofillAiAction(
    AutofillAiAction action,
    std::optional<EntityType> entity_type) const {
  return MayPerformAutofillAiAction(
      google_groups_manager_, prefs_, &entity_data_manager(), identity_manager_,
      sync_service_, IsWalletPublicPassStorageEnabledHelper(),
      is_off_the_record_, entity_data_manager_->GetVariationCountryCode(),
      action, entity_type);
}

// Returns true if the `entity_type` supports wallet storage.
bool EntityDataManagerAndroid::IsEligibleForWalletStorage(
    EntityType entity_type) const {
  return RunMayPerformAutofillAiAction(AutofillAiAction::kImportToWallet,
                                       entity_type) &&
         base::FeatureList::IsEnabled(
             features::kAutofillEnableSaveToWalletFromSettings);
}

void EntityDataManagerAndroid::OnSavePrivatePassToWalletFinished(
    base::OnceClosure on_local_save_fallback,
    EntityInstance original_entity,
    std::optional<EntityInstance> saved_entity) {
  if (saved_entity.has_value()) {
    entity_data_manager().AddOrUpdateEntityInstance(std::move(*saved_entity));
  } else {
    std::move(on_local_save_fallback).Run();
    EntityInstance local_entity = original_entity.CopyWithNewRecordType(
        EntityInstance::RecordType::kLocal);
    entity_data_manager().AddOrUpdateEntityInstance(std::move(local_entity));
  }
}

}  // namespace autofill

DEFINE_JNI(EntityDataManager)
