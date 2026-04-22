// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_ENTITY_DATA_MANAGER_ANDROID_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_ENTITY_DATA_MANAGER_ANDROID_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/autofill/android/entity_instance_android.h"
#include "chrome/browser/autofill/android/entity_instance_with_labels.h"
#include "chrome/browser/autofill/android/entity_type_android.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#include "third_party/jni_zero/jni_zero.h"

class GoogleGroupsManager;
class PrefService;

namespace consent_auditor {
class ConsentAuditor;
}

namespace signin {
class IdentityManager;
}

namespace syncer {
class SyncService;
}

namespace account_settings {
class AccountSettingService;
}

namespace autofill {

class WalletPassAccessManager;

// Android wrapper of the EntityDataManager which provides access from the
// Java layer.
class EntityDataManagerAndroid : public autofill::EntityDataManager::Observer {
 public:
  EntityDataManagerAndroid(
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
      EntityDataManager* entity_data_manager);

  EntityDataManagerAndroid(const EntityDataManagerAndroid&) = delete;
  EntityDataManagerAndroid& operator=(const EntityDataManagerAndroid&) = delete;

  // Trigger the destruction of the C++ object from Java.
  void Destroy(JNIEnv* env);

  // Returns whether the user is eligible for Autofill AI.
  bool IsEligibleToAutofillAi(JNIEnv* env);

  // Returns the opt-in status for Autofill AI.
  bool GetAutofillAiOptInStatus(JNIEnv* env);

  // Sets the opt-in status for Autofill AI.
  bool SetAutofillAiOptInStatus(JNIEnv* env,
                                AutofillAiOptInStatus opt_in_status);

  // Removes the entity instance represented by `guid`.
  void RemoveEntityInstance(JNIEnv* env, const std::string& guid);

  std::optional<EntityInstanceAndroid> GetEntityInstance(
      JNIEnv* env,
      const std::string& guid);

  // Add or replace an `EntityInstance` depending on whether it already exists
  // or not.
  // If the user is eligible for Google Wallet private passes, entities such as
  // passport are stored unmaked in Wallet servers, with a masked version stored
  // on device. Otherwise, entities are always stored on device.
  // If an attempt to store `jEntity` in Wallet servers happen but fails, either
  // because of a failed server call or because the user became ineligible,
  // the `on_local_save_fallback` is run, which displays the user a feedback
  // message about their data being stored locally instead.
  // `description_string_id` and `accept_button_string_id` are the resource IDs
  // of the strings used in the UI while creating the description and the button
  // to accept it. Note that these resources IDs are only used for logging
  // purposes, in the case of adding a new private entity that will stored in
  // Google Wallet.
  void AddOrUpdateEntityInstance(JNIEnv* env,
                                 const jni_zero::JavaRef<jobject>& jEntity,
                                 int32_t description_string_id,
                                 int32_t accept_button_string_id,
                                 base::OnceClosure on_local_save_fallback);

  // Gets information about all entities to be displayed in the management
  // service.
  std::vector<EntityInstanceWithLabels> GetEntitiesWithLabels(JNIEnv* env);

  // Returns all types of entities that Autofill AI supports.
  std::vector<EntityTypeAndroid> GetWritableEntityTypes(JNIEnv* env);

  // Returns all entity types that Autofill AI supports, sorted by
  // usefulness.
  std::vector<EntityTypeAndroid> GetSortedEntityTypesForListDisplay(
      JNIEnv* env) const;

  // Checks whether Autofill AI is disabled by enterprise policy.
  // TODO(crbug.com/468236777): Return `ModelExecutionEnterprisePolicyValue`
  // enum instead of having a specific method to check the policy pref state.
  bool GetIsAutofillAiDisabledByEnterprisePolicy(JNIEnv* env);

  // Checks whether Autofill AI is enabled by enterprise policy but without
  // logging.
  bool GetIsAutofillAiEnabledByEnterprisePolicyWithoutLogging(JNIEnv* env);

  // See `AutofillAiAction::kEnableOrDisable` for details.
  bool CanEnableOrDisableAutofillAi(JNIEnv* env);

  // Returns whether the user might perform
  // `AutofillAiAction::kListEntityInstancesInSettings`.
  bool CanListEntityInstancesInSettings(JNIEnv* env);

  // Returns true if the user can store and read public passes on Google Wallet
  // servers. Used to display a notice in the management UI.
  bool IsWalletPublicPassStorageEnabled(JNIEnv* env);

 private:
  friend class EntityDataManagerAndroidTestApi;

  ~EntityDataManagerAndroid() override;

  bool RunMayPerformAutofillAiAction(
      AutofillAiAction action,
      std::optional<EntityType> entity_type) const;

  // EntityDataManager::Observer implementation.
  void OnEntityInstancesChanged() override;

  EntityDataManager& entity_data_manager() {
    return entity_data_manager_.get();
  }

  EntityDataManager& entity_data_manager() const {
    return entity_data_manager_.get();
  }

  // Same as `IsWalletPublicPassStorageEnabled` but without the `env` so
  // it can be reused internally.
  bool IsWalletPublicPassStorageEnabledHelper() const;

  // Runs permission checks on whether an entity of `entity_type` can be stored
  // on Google Wallet servers.
  bool IsEligibleForWalletStorage(EntityType entity_type) const;

  // `entity_instance` is the instance that is going to be saved either locally
  // or to Google Wallet servers. `targeted_record_type` reflects whether the
  // user attempted to store the entity locally or to Wallet when
  // interacting with the management page. As an example, both can differ if
  // syncing is disabled between the moment whe the user opens the add entity
  // dialog and the moment the save button is clicked. In this case,
  // `entity_instance` will have its record type as `kLocal`, while
  // `targeted_record_type` will `kServerWallet`, which will lead to the
  // `on_local_save_fallback` being run, displaying a feedback message to users
  // to let them know the entity was stored locally instead.
  // `description_string_id` and `accept_button_string_id` are the resource IDs
  // of the strings used in the UI while creating the description and the button
  // to accept it. Note that these resources IDs are only used for logging
  // purposes, in the case of adding a new private entity that will stored in
  // Google Wallet.
  void AddOrUpdateEntityInstance(
      EntityInstance entity_instance,
      EntityInstance::RecordType targeted_record_type,
      int description_string_id,
      int accept_button_string_id,
      base::OnceClosure on_local_save_fallback);

  // Called after an attempt to save a private pass to Google Wallet.
  // If `saved_entity` exists, it will be a masked entity which will be stored
  // locally. `original_entity` is the unmasked entity used during saving
  // attempt. It will be stored if `saved_entity` does not exist, likely due
  // to a server call failure.
  void OnSavePrivatePassToWalletFinished(
      base::OnceClosure on_local_save_fallback,
      EntityInstance original_entity,
      std::optional<EntityInstance> saved_entity);

  base::ScopedObservation<EntityDataManager, EntityDataManager::Observer>
      entity_data_manager_observer_{this};

  // Pointer to the java counterpart.
  JavaObjectWeakGlobalRef weak_java_obj_;

  const raw_ptr<const GoogleGroupsManager> google_groups_manager_;
  const raw_ptr<PrefService> prefs_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  const raw_ptr<const syncer::SyncService> sync_service_;
  const raw_ptr<const account_settings::AccountSettingService>
      account_setting_service_;
  const raw_ptr<consent_auditor::ConsentAuditor> consent_auditor_;
  const bool is_off_the_record_;
  const raw_ptr<WalletPassAccessManager> wallet_pass_access_manager_;

  // Pointer to the EntityDataManager.
  raw_ref<EntityDataManager> entity_data_manager_;

  base::WeakPtrFactory<EntityDataManagerAndroid> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_ENTITY_DATA_MANAGER_ANDROID_H_
