// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/cloud_upload_prompt_prefs_handler.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/upload_office_to_cloud/upload_office_to_cloud.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace chromeos::cloud_upload {

namespace {

bool IsProfileEnterpriseManaged(Profile* profile) {
  return profile->GetProfilePolicyConnector()->IsManaged() &&
         !profile->IsChild();
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
  void OnOfficeFilesAlwaysMoveToDriveChanged();
  void OnOfficeFilesAlwaysMoveToDriveSyncableChanged();
  void OnOfficeFilesAlwaysMoveToOneDriveChanged();
  void OnOfficeFilesAlwaysMoveToOneDriveSyncableChanged();

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
}

// KeyedService:
void CloudUploadPromptPrefsHandler::Shutdown() {
  pref_change_registrar_.reset();
  profile_ = nullptr;
}

CloudUploadPromptPrefsHandler::CloudUploadPromptPrefsHandler(Profile* profile)
    : profile_(profile),
      pref_change_registrar_(std::make_unique<PrefChangeRegistrar>()) {
  pref_change_registrar_->Init(profile_->GetPrefs());
  pref_change_registrar_->Add(
      prefs::kOfficeFilesAlwaysMoveToDrive,
      base::BindRepeating(
          &CloudUploadPromptPrefsHandler::OnOfficeFilesAlwaysMoveToDriveChanged,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kOfficeFilesAlwaysMoveToDriveSyncable,
      base::BindRepeating(&CloudUploadPromptPrefsHandler::
                              OnOfficeFilesAlwaysMoveToDriveSyncableChanged,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kOfficeFilesAlwaysMoveToOneDrive,
      base::BindRepeating(&CloudUploadPromptPrefsHandler::
                              OnOfficeFilesAlwaysMoveToOneDriveChanged,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kOfficeFilesAlwaysMoveToOneDriveSyncable,
      base::BindRepeating(&CloudUploadPromptPrefsHandler::
                              OnOfficeFilesAlwaysMoveToOneDriveSyncableChanged,
                          base::Unretained(this)));
  // TODO(387268733): Initial sync to syncable prefs if needed.
  // TODO(387268733): Observe CloudUpload policies changes.
}

void CloudUploadPromptPrefsHandler::OnOfficeFilesAlwaysMoveToDriveChanged() {
  if (!IsProfileEnterpriseManaged(profile_)) {
    return;
  }

  bool always_move =
      profile_->GetPrefs()->GetBoolean(prefs::kOfficeFilesAlwaysMoveToDrive);
  profile_->GetPrefs()->SetBoolean(prefs::kOfficeFilesAlwaysMoveToDriveSyncable,
                                   always_move);
}

void CloudUploadPromptPrefsHandler::
    OnOfficeFilesAlwaysMoveToDriveSyncableChanged() {
  if (!IsProfileEnterpriseManaged(profile_)) {
    return;
  }
  if (!cloud_upload::IsGoogleWorkspaceCloudUploadAutomated(profile_)) {
    return;
  }

  bool always_move_syncable = profile_->GetPrefs()->GetBoolean(
      prefs::kOfficeFilesAlwaysMoveToDriveSyncable);
  profile_->GetPrefs()->SetBoolean(prefs::kOfficeFilesAlwaysMoveToDrive,
                                   always_move_syncable);
}

void CloudUploadPromptPrefsHandler::OnOfficeFilesAlwaysMoveToOneDriveChanged() {
  if (!IsProfileEnterpriseManaged(profile_)) {
    return;
  }

  bool always_move =
      profile_->GetPrefs()->GetBoolean(prefs::kOfficeFilesAlwaysMoveToOneDrive);
  profile_->GetPrefs()->SetBoolean(
      prefs::kOfficeFilesAlwaysMoveToOneDriveSyncable, always_move);
}

void CloudUploadPromptPrefsHandler::
    OnOfficeFilesAlwaysMoveToOneDriveSyncableChanged() {
  if (!IsProfileEnterpriseManaged(profile_)) {
    return;
  }
  if (!cloud_upload::IsMicrosoftOfficeCloudUploadAutomated(profile_)) {
    return;
  }

  bool always_move_syncable = profile_->GetPrefs()->GetBoolean(
      prefs::kOfficeFilesAlwaysMoveToOneDriveSyncable);
  profile_->GetPrefs()->SetBoolean(prefs::kOfficeFilesAlwaysMoveToOneDrive,
                                   always_move_syncable);
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
