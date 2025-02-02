// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/cloud_upload_prompt_prefs_handler.h"

#include <memory>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/upload_office_to_cloud/upload_office_to_cloud.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace chromeos::cloud_upload {

namespace {

using LocalToSyncablePrefMap =
    base::flat_map<const char*,
                   std::pair<const char*, ash::cloud_upload::CloudProvider>>;

// Maps the names of local "always move to drive/onedrive" prefs to the
// corresponding syncable prefs.
const LocalToSyncablePrefMap kAlwaysMoveToCloudPrefMap = {
    {prefs::kOfficeFilesAlwaysMoveToDrive,
     {prefs::kOfficeFilesAlwaysMoveToDriveSyncable,
      ash::cloud_upload::CloudProvider::kGoogleDrive}},
    {prefs::kOfficeFilesAlwaysMoveToOneDrive,
     {prefs::kOfficeFilesAlwaysMoveToOneDriveSyncable,
      ash::cloud_upload::CloudProvider::kOneDrive}},
};

// Maps the names of local "move confirmation shown" prefs to the corresponding
// syncable prefs.
const LocalToSyncablePrefMap kMoveConfirmationShownPrefMap = {
    {prefs::kOfficeMoveConfirmationShownForDrive,
     {prefs::kOfficeMoveConfirmationShownForDriveSyncable,
      ash::cloud_upload::CloudProvider::kGoogleDrive}},
    {prefs::kOfficeMoveConfirmationShownForLocalToDrive,
     {prefs::kOfficeMoveConfirmationShownForLocalToDriveSyncable,
      ash::cloud_upload::CloudProvider::kGoogleDrive}},
    {prefs::kOfficeMoveConfirmationShownForCloudToDrive,
     {prefs::kOfficeMoveConfirmationShownForCloudToDriveSyncable,
      ash::cloud_upload::CloudProvider::kGoogleDrive}},
    {prefs::kOfficeMoveConfirmationShownForOneDrive,
     {prefs::kOfficeMoveConfirmationShownForOneDriveSyncable,
      ash::cloud_upload::CloudProvider::kOneDrive}},
    {prefs::kOfficeMoveConfirmationShownForLocalToOneDrive,
     {prefs::kOfficeMoveConfirmationShownForLocalToOneDriveSyncable,
      ash::cloud_upload::CloudProvider::kOneDrive}},
    {prefs::kOfficeMoveConfirmationShownForCloudToOneDrive,
     {prefs::kOfficeMoveConfirmationShownForCloudToOneDriveSyncable,
      ash::cloud_upload::CloudProvider::kOneDrive}},
};

bool IsProfileEnterpriseManaged(Profile* profile) {
  return profile->GetProfilePolicyConnector()->IsManaged() &&
         !profile->IsChild();
}

bool IsSyncEnabled(Profile* profile) {
  const syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  if (!sync_service) {
    VLOG(1) << "Sync service not available";
    return false;
  }

  return sync_service->GetActiveDataTypes().Has(syncer::OS_PREFERENCES);
}

bool IsCloudUploadAutomated(Profile* profile,
                            ash::cloud_upload::CloudProvider cloud_provider) {
  return (cloud_provider == ash::cloud_upload::CloudProvider::kGoogleDrive &&
          cloud_upload::IsGoogleWorkspaceCloudUploadAutomated(profile)) ||
         (cloud_provider == ash::cloud_upload::CloudProvider::kOneDrive &&
          cloud_upload::IsMicrosoftOfficeCloudUploadAutomated(profile));
}

// Checks the values of local and syncable prefs, and logs if they're different
// when expected to be the same.
void MaybeLogMismatchedValues(Profile* profile,
                              const char* local_pref,
                              const LocalToSyncablePrefMap& pref_map) {
  auto it = pref_map.find(local_pref);
  DCHECK(it != pref_map.end());
  // Don't check if the feature isn't enabled.
  if (IsSyncEnabled(profile) ||
      !IsCloudUploadAutomated(profile, it->second.second)) {
    return;
  }
  bool local_value = profile->GetPrefs()->GetBoolean(local_pref);
  bool syncable_value = profile->GetPrefs()->GetBoolean(it->second.first);
  if (local_value != syncable_value) {
    LOG(WARNING) << "Mismatched preferences during initialization: "
                 << local_pref << " (" << (local_value ? "true" : "false")
                 << ") and " << it->second.first << " ("
                 << (syncable_value ? "true" : "false") << ")";
  }
}

// Initializes syncable prefs with local pref values if uninitialized,
// otherwise logs unexpected mismatches.
void InitializeSyncablePrefs(Profile* profile,
                                    const LocalToSyncablePrefMap& pref_map) {
  for (const auto& [local_pref, syncable_pref] : pref_map) {
    const PrefService::Preference* pref =
        profile->GetPrefs()->FindPreference(syncable_pref.first);
    if (!pref) {
      LOG(WARNING) << "Preference " << syncable_pref.first << " not found.";
      continue;
    }
    bool local_value = profile->GetPrefs()->GetBoolean(local_pref);
    if (pref->IsDefaultValue()) {
      profile->GetPrefs()->SetBoolean(syncable_pref.first, local_value);
      continue;
    }
    MaybeLogMismatchedValues(profile, local_pref, pref_map);
  }
}

class CloudUploadPromptPrefsHandler : public KeyedService {
 public:
  static std::unique_ptr<CloudUploadPromptPrefsHandler> Create(
      Profile* profile);

  CloudUploadPromptPrefsHandler(const CloudUploadPromptPrefsHandler&) = delete;
  CloudUploadPromptPrefsHandler& operator=(
      const CloudUploadPromptPrefsHandler&) = delete;

  ~CloudUploadPromptPrefsHandler() override;

  // Registers preferences related to enterprise cloud upload flows.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // KeyedService:
  void Shutdown() override;

 private:
  explicit CloudUploadPromptPrefsHandler(Profile* profile);

  // Callbacks for pref changes. Synchronize local and syncable prefs, if
  // needed.
  void OnOfficeFilesAlwaysMoveChanged(const char* local_pref);
  void OnOfficeFilesAlwaysMoveSyncableChanged(const char* local_pref);
  void OnOfficeMoveConfirmationShownChanged(const char* local_pref);
  void OnOfficeMoveConfirmationShownSyncableChanged(const char* local_pref);
  void OnCloudUploadPrefChanged();

  raw_ptr<Profile> profile_ = nullptr;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
};

// static
std::unique_ptr<CloudUploadPromptPrefsHandler>
CloudUploadPromptPrefsHandler::Create(Profile* profile) {
  return base::WrapUnique(new CloudUploadPromptPrefsHandler(profile));
}

CloudUploadPromptPrefsHandler::~CloudUploadPromptPrefsHandler() = default;

// static
void CloudUploadPromptPrefsHandler::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kOfficeFilesAlwaysMoveToDriveSyncable, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kOfficeFilesAlwaysMoveToOneDriveSyncable, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kOfficeMoveConfirmationShownForDriveSyncable, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kOfficeMoveConfirmationShownForOneDriveSyncable, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kOfficeMoveConfirmationShownForLocalToDriveSyncable, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kOfficeMoveConfirmationShownForLocalToOneDriveSyncable, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kOfficeMoveConfirmationShownForCloudToDriveSyncable, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(
      prefs::kOfficeMoveConfirmationShownForCloudToOneDriveSyncable, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
}

// KeyedService:
void CloudUploadPromptPrefsHandler::Shutdown() {
  pref_change_registrar_.reset();
  profile_ = nullptr;
}

CloudUploadPromptPrefsHandler::CloudUploadPromptPrefsHandler(Profile* profile)
    : profile_(profile),
      pref_change_registrar_(std::make_unique<PrefChangeRegistrar>()) {
  // Initially set the syncable prefs to match the local values.
  InitializeSyncablePrefs(profile_, kAlwaysMoveToCloudPrefMap);
  InitializeSyncablePrefs(profile_, kMoveConfirmationShownPrefMap);

  pref_change_registrar_->Init(profile_->GetPrefs());
  for (const auto& [local_pref, syncable_pref] : kAlwaysMoveToCloudPrefMap) {
    pref_change_registrar_->Add(
        local_pref,
        base::BindRepeating(
            &CloudUploadPromptPrefsHandler::OnOfficeFilesAlwaysMoveChanged,
            base::Unretained(this), local_pref));
    pref_change_registrar_->Add(
        syncable_pref.first,
        base::BindRepeating(&CloudUploadPromptPrefsHandler::
                                OnOfficeFilesAlwaysMoveSyncableChanged,
                            base::Unretained(this), local_pref));
  }

  for (const auto& [local_pref, syncable_pref] :
       kMoveConfirmationShownPrefMap) {
    pref_change_registrar_->Add(
        local_pref,
        base::BindRepeating(&CloudUploadPromptPrefsHandler::
                                OnOfficeMoveConfirmationShownChanged,
                            base::Unretained(this), local_pref));
    pref_change_registrar_->Add(
        syncable_pref.first,
        base::BindRepeating(&CloudUploadPromptPrefsHandler::
                                OnOfficeMoveConfirmationShownSyncableChanged,
                            base::Unretained(this), local_pref));
  }

  pref_change_registrar_->Add(
      prefs::kGoogleWorkspaceCloudUpload,
      base::BindRepeating(
          &CloudUploadPromptPrefsHandler::OnCloudUploadPrefChanged,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kMicrosoftOfficeCloudUpload,
      base::BindRepeating(
          &CloudUploadPromptPrefsHandler::OnCloudUploadPrefChanged,
          base::Unretained(this)));
}

void CloudUploadPromptPrefsHandler::OnOfficeFilesAlwaysMoveChanged(
    const char* local_pref) {
  auto it = kAlwaysMoveToCloudPrefMap.find(local_pref);
  DCHECK(it != kAlwaysMoveToCloudPrefMap.end());
  bool always_move = profile_->GetPrefs()->GetBoolean(local_pref);
  profile_->GetPrefs()->SetBoolean(it->second.first, always_move);
}

void CloudUploadPromptPrefsHandler::OnOfficeFilesAlwaysMoveSyncableChanged(
    const char* local_pref) {
  auto it = kAlwaysMoveToCloudPrefMap.find(local_pref);
  DCHECK(it != kAlwaysMoveToCloudPrefMap.end());
  if (!IsCloudUploadAutomated(profile_, it->second.second)) {
    return;
  }

  bool always_move_syncable =
      profile_->GetPrefs()->GetBoolean(it->second.first);
  profile_->GetPrefs()->SetBoolean(local_pref, always_move_syncable);
}

void CloudUploadPromptPrefsHandler::OnOfficeMoveConfirmationShownChanged(
    const char* local_pref) {
  auto it = kMoveConfirmationShownPrefMap.find(local_pref);
  DCHECK(it != kMoveConfirmationShownPrefMap.end());
  bool move_confirmation_shown = profile_->GetPrefs()->GetBoolean(local_pref);
  profile_->GetPrefs()->SetBoolean(it->second.first, move_confirmation_shown);
}

void CloudUploadPromptPrefsHandler::
    OnOfficeMoveConfirmationShownSyncableChanged(const char* local_pref) {
  auto it = kMoveConfirmationShownPrefMap.find(local_pref);
  DCHECK(it != kMoveConfirmationShownPrefMap.end());
  if (!IsCloudUploadAutomated(profile_, it->second.second)) {
    return;
  }
  bool move_confirmation_shown_syncable =
      profile_->GetPrefs()->GetBoolean(it->second.first);
  profile_->GetPrefs()->SetBoolean(local_pref,
                                   move_confirmation_shown_syncable);
}

void CloudUploadPromptPrefsHandler::OnCloudUploadPrefChanged() {
  const bool google_workspace_automated =
      IsGoogleWorkspaceCloudUploadAutomated(profile_);
  const bool microsoft_office_automated =
      IsMicrosoftOfficeCloudUploadAutomated(profile_);
  // If neither policy is set to `automated`, no updates are needed.
  if (!google_workspace_automated && !microsoft_office_automated) {
    return;
  }
  // A special case that is not supposed to happen in production; the agreed
  // decision is to ignore this setup and act as if both values were set to
  // `allowed` instead of `automated`.
  if (google_workspace_automated && microsoft_office_automated) {
    return;
  }
  if (!IsSyncEnabled(profile_)) {
    return;
  }

  for (const auto& [local_pref, syncable_pref] : kAlwaysMoveToCloudPrefMap) {
    OnOfficeFilesAlwaysMoveSyncableChanged(local_pref);
  }
  for (const auto& [local_pref, syncable_pref] :
       kMoveConfirmationShownPrefMap) {
    OnOfficeMoveConfirmationShownSyncableChanged(local_pref);
  }
}

}  // namespace

// static
CloudUploadPromptPrefsHandlerFactory*
CloudUploadPromptPrefsHandlerFactory::GetInstance() {
  static base::NoDestructor<CloudUploadPromptPrefsHandlerFactory> instance;
  return instance.get();
}

CloudUploadPromptPrefsHandlerFactory::CloudUploadPromptPrefsHandlerFactory()
    : ProfileKeyedServiceFactory("CloudUploadPromptPrefsHandlerFactory") {}

CloudUploadPromptPrefsHandlerFactory::~CloudUploadPromptPrefsHandlerFactory() =
    default;

void CloudUploadPromptPrefsHandlerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  CloudUploadPromptPrefsHandler::RegisterProfilePrefs(registry);
}

std::unique_ptr<KeyedService>
CloudUploadPromptPrefsHandlerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!IsProfileEnterpriseManaged(Profile::FromBrowserContext(context))) {
    return nullptr;
  }
  CHECK(features::IsUploadOfficeToCloudForEnterpriseEnabled() &&
        features::IsUploadOfficeToCloudSyncEnabled());
  return CloudUploadPromptPrefsHandler::Create(
      Profile::FromBrowserContext(context));
}

bool CloudUploadPromptPrefsHandlerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace chromeos::cloud_upload
