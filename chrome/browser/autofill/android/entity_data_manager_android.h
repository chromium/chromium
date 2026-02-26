// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ANDROID_ENTITY_DATA_MANAGER_ANDROID_H_
#define CHROME_BROWSER_AUTOFILL_ANDROID_ENTITY_DATA_MANAGER_ANDROID_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/autofill/android/entity_instance_with_labels.h"
#include "chrome/browser/autofill/android/entity_type_android.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#include "third_party/jni_zero/jni_zero.h"

class GoogleGroupsManager;
class PrefService;

namespace signin {
class IdentityManager;
}

namespace syncer {
class SyncService;
}

namespace autofill {

class AccountSettingService;

// Android wrapper of the EntityDataManager which provides access from the
// Java layer.
class EntityDataManagerAndroid : public autofill::EntityDataManager::Observer {
 public:
  EntityDataManagerAndroid(JNIEnv* env,
                           const jni_zero::JavaRef<jobject>& obj,
                           const GoogleGroupsManager* google_groups_manager,
                           PrefService* prefs,
                           const signin::IdentityManager* identity_manager,
                           const syncer::SyncService* sync_service,
                           const AccountSettingService* account_setting_service,
                           bool is_off_the_record,
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

  base::android::ScopedJavaLocalRef<jobject> GetEntityInstance(
      JNIEnv* env,
      const std::string& guid);

  // Add or replace an `EntityInstance` depending on whether it already exists
  // or not.
  void AddOrUpdateEntityInstance(JNIEnv* env,
                                 const jni_zero::JavaRef<jobject>& jEntity);

  // Gets information about all entities to be displayed in the management
  // service.
  std::vector<EntityInstanceWithLabels> GetEntitiesWithLabels(JNIEnv* env);

  // Returns all types of entities that Autofill AI supports.
  std::vector<EntityTypeAndroid> GetWritableEntityTypes(JNIEnv* env);

  // Returns all entity types that Autofill AI supports, sorted by
  // usefulness.
  std::vector<EntityTypeAndroid> GetSortedEntityTypesForListDisplay(
      JNIEnv* env) const;

 private:
  ~EntityDataManagerAndroid() override;

  // autofill::EntityDataManager::Observer implementation.
  void OnEntityInstancesChanged() override;

  EntityDataManager& entity_data_manager() {
    return entity_data_manager_.get();
  }

  base::ScopedObservation<autofill::EntityDataManager,
                          autofill::EntityDataManager::Observer>
      entity_data_manager_observer_{this};

  // Pointer to the java counterpart.
  JavaObjectWeakGlobalRef weak_java_obj_;

  const raw_ptr<const GoogleGroupsManager> google_groups_manager_;
  const raw_ptr<PrefService> prefs_;
  const raw_ptr<const signin::IdentityManager> identity_manager_;
  const raw_ptr<const syncer::SyncService> sync_service_;
  const raw_ptr<const AccountSettingService> account_setting_service_;
  const bool is_off_the_record_;

  // Pointer to the EntityDataManager.
  raw_ref<EntityDataManager> entity_data_manager_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ANDROID_ENTITY_DATA_MANAGER_ANDROID_H_
