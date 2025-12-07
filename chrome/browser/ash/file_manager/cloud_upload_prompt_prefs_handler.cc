// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/cloud_upload_prompt_prefs_handler.h"

#include <memory>
#include <vector>

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

// Holds information about local and syncable pref pairs, and the corresponding
// cloud provider.
struct PrefInfo {
  const char* local_pref;
  const char* syncable_pref;
  ash::cloud_upload::CloudProvider cloud_provider;
};

const std::vector<PrefInfo> kCloudUploadPrefs = {
    {prefs::kOfficeFilesAlwaysMoveToDrive,
     prefs::kOfficeFilesAlwaysMoveToDriveSyncable,
     ash::cloud_upload::CloudProvider::kGoogleDrive},
    {prefs::kOfficeFilesAlwaysMoveToOneDrive,
     prefs::kOfficeFilesAlwaysMoveToOneDriveSyncable,
     ash::cloud_upload::CloudProvider::kOneDrive},
    {prefs::kOfficeMoveConfirmationShownForDrive,
     prefs::kOfficeMoveConfirmationShownForDriveSyncable,
     ash::cloud_upload::CloudProvider::kGoogleDrive},
    {prefs::kOfficeMoveConfirmationShownForLocalToDrive,
     prefs::kOfficeMoveConfirmationShownForLocalToDriveSyncable,
     ash::cloud_upload::CloudProvider::kGoogleDrive},
    {prefs::kOfficeMoveConfirmationShownForCloudToDrive,
     prefs::kOfficeMoveConfirmationShownForCloudToDriveSyncable,
     ash::cloud_upload::CloudProvider::kGoogleDrive},
    {prefs::kOfficeMoveConfirmationShownForOneDrive,
     prefs::kOfficeMoveConfirmationShownForOneDriveSyncable,
     ash::cloud_upload::CloudProvider::kOneDrive},
    {prefs::kOfficeMoveConfirmationShownForLocalToOneDrive,
     prefs::kOfficeMoveConfirmationShownForLocalToOneDriveSyncable,
     ash::cloud_upload::CloudProvider::kOneDrive},
    {prefs::kOfficeMoveConfirmationShownForCloudToOneDrive,
     prefs::kOfficeMoveConfirmationShownForCloudToOneDriveSyncable,
     ash::cloud_upload::CloudProvider::kOneDrive},
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
void MaybeLogMismatchedValues(Profile* profile, const PrefInfo& pref_info) {
  // Don't check if the feature isn't enabled.
  if (IsSyncEnabled(profile) ||
      !IsCloudUploadAutomated(profile, pref_info.cloud_provider)) {
    return;
  }
  bool local_value = profile->GetPrefs()->GetBoolean(pref_info.local_pref);
  bool syncable_value =
      profile->GetPrefs()->GetBoolean(pref_info.syncable_pref);
  if (local_value != syncable_value) {
    LOG(WARNING) << "Mismatched preferences during initialization: "
                 << pref_info.local_pref << " ("
                 << (local_value ? "true" : "false") << ") and "
                 << pref_info.syncable_pref << " ("
                 << (syncable_value ? "true" : "false") << ")";
  }
}

// Initializes syncable prefs with local pref values if uninitialized,
// otherwise logs unexpected mismatches.
void InitializeSyncablePrefs(Profile* profile,
                             const std::vector<PrefInfo>& prefs) {
  for (const PrefInfo& pref_info : prefs) {
    const PrefService::Preference* pref =
        profile->GetPrefs()->FindPreference(pref_info.syncable_pref);
    if (!pref) {
      LOG(WARNING) << "Preference " << pref_info.syncable_pref << " not found.";
      continue;
    }
    bool local_value = profile->GetPrefs()->GetBoolean(pref_info.local_pref);
    if (pref->IsDefaultValue()) {
      profile->GetPrefs()->SetBoolean(pref_info.syncable_pref, local_value);
      continue;
    }
    MaybeLogMismatchedValues(profile, pref_info);
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
  void OnLocalPrefChanged(const PrefInfo& pref_info);
  void OnSyncablePrefChanged(const PrefInfo& pref_info);
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
  for (const PrefInfo& pref_info : kCloudUploadPrefs) {
    registry->RegisterBooleanPref(
        pref_info.syncable_pref, false,
        user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  }
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
  InitializeSyncablePrefs(profile_, kCloudUploadPrefs);

  pref_change_registrar_->Init(profile_->GetPrefs());
  for (const PrefInfo& pref_info : kCloudUploadPrefs) {
    pref_change_registrar_->Add(
        pref_info.local_pref,
        base::BindRepeating(&CloudUploadPromptPrefsHandler::OnLocalPrefChanged,
                            base::Unretained(this), pref_info));
    pref_change_registrar_->Add(
        pref_info.syncable_pref,
        base::BindRepeating(
            &CloudUploadPromptPrefsHandler::OnSyncablePrefChanged,
            base::Unretained(this), pref_info));
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

void CloudUploadPromptPrefsHandler::OnLocalPrefChanged(
    const PrefInfo& pref_info) {
  bool local_value = profile_->GetPrefs()->GetBoolean(pref_info.local_pref);
  profile_->GetPrefs()->SetBoolean(pref_info.syncable_pref, local_value);
}

void CloudUploadPromptPrefsHandler::OnSyncablePrefChanged(
    const PrefInfo& pref_info) {
  if (!IsCloudUploadAutomated(profile_, pref_info.cloud_provider)) {
    return;
  }

  bool syncable_value =
      profile_->GetPrefs()->GetBoolean(pref_info.syncable_pref);
  profile_->GetPrefs()->SetBoolean(pref_info.local_pref, syncable_value);
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

  for (const PrefInfo& pref_info : kCloudUploadPrefs) {
    OnSyncablePrefChanged(pref_info);
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
