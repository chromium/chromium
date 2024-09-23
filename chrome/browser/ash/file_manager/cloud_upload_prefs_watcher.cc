// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/cloud_upload_prefs_watcher.h"

#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/office_file_tasks.h"
#include "chrome/browser/chromeos/upload_office_to_cloud/upload_office_to_cloud.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

namespace chromeos::cloud_upload {

namespace {

namespace fm_tasks = file_manager::file_tasks;

// Associates all office-related file extensions & mime types with Microsoft
// Office. Does not overwrite existing associations previously selected by the
// user.
void SetMicrosoftOfficeAsDefaultHandlerWithoutOverwriting(Profile* profile) {
  fm_tasks::SetWordFileHandlerToFilesSWA(
      profile, fm_tasks::kActionIdOpenInOffice, /*replace_existing=*/false);
  fm_tasks::SetExcelFileHandlerToFilesSWA(
      profile, fm_tasks::kActionIdOpenInOffice, /*replace_existing=*/false);
  fm_tasks::SetPowerPointFileHandlerToFilesSWA(
      profile, fm_tasks::kActionIdOpenInOffice, /*replace_existing=*/false);
}

// Associates all office-related file extensions & mime types with Google
// Workspace. Does not overwrite existing associations previously selected by
// the user.
void SetGoogleWorkspaceAsDefaultHandlerWithoutOverwriting(Profile* profile) {
  fm_tasks::SetWordFileHandlerToFilesSWA(profile,
                                         fm_tasks::kActionIdWebDriveOfficeWord,
                                         /*replace_existing=*/false);
  fm_tasks::SetExcelFileHandlerToFilesSWA(
      profile, fm_tasks::kActionIdWebDriveOfficeExcel,
      /*replace_existing=*/false);
  fm_tasks::SetPowerPointFileHandlerToFilesSWA(
      profile, fm_tasks::kActionIdWebDriveOfficePowerPoint,
      /*replace_existing=*/false);
}

// Clears file associations that are defaulted to Microsoft Office.
void UnsetMicrosoftOfficeAsDefaultHandlerIfNecessary(Profile* profile) {
  fm_tasks::RemoveFilesSWAWordFileHandler(profile,
                                          fm_tasks::kActionIdOpenInOffice);
  fm_tasks::RemoveFilesSWAExcelFileHandler(profile,
                                           fm_tasks::kActionIdOpenInOffice);
  fm_tasks::RemoveFilesSWAPowerPointFileHandler(
      profile, fm_tasks::kActionIdOpenInOffice);
}

// Clears file associations that are defaulted to Google Workspace.
void UnsetGoogleWorkspaceAsDefaultHandlerIfNecessary(Profile* profile) {
  fm_tasks::RemoveFilesSWAWordFileHandler(
      profile, fm_tasks::kActionIdWebDriveOfficeWord);
  fm_tasks::RemoveFilesSWAExcelFileHandler(
      profile, fm_tasks::kActionIdWebDriveOfficeExcel);
  fm_tasks::RemoveFilesSWAPowerPointFileHandler(
      profile, fm_tasks::kActionIdWebDriveOfficePowerPoint);
}

class CloudUploadPrefsWatcher : public KeyedService {
 public:
  ~CloudUploadPrefsWatcher() override;

  static std::unique_ptr<CloudUploadPrefsWatcher> Create(Profile* profile);

  // KeyedService:
  void Shutdown() override;

 private:
  explicit CloudUploadPrefsWatcher(Profile* profile);

  // Sets up watchers.
  void Init();

  // Serves as callback for pref changes.
  void OnCloudUploadPrefChanged();

  raw_ptr<Profile> profile_;

  // The registrar used to watch prefs changes.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
};

CloudUploadPrefsWatcher::CloudUploadPrefsWatcher(Profile* profile)
    : profile_(profile),
      pref_change_registrar_(std::make_unique<PrefChangeRegistrar>()) {}

CloudUploadPrefsWatcher::~CloudUploadPrefsWatcher() = default;

std::unique_ptr<CloudUploadPrefsWatcher> CloudUploadPrefsWatcher::Create(
    Profile* profile) {
  auto watcher = base::WrapUnique(new CloudUploadPrefsWatcher(profile));
  watcher->Init();
  return watcher;
}

void CloudUploadPrefsWatcher::Init() {
  pref_change_registrar_->Init(profile_->GetPrefs());

  for (const auto* pref : {prefs::kMicrosoftOfficeCloudUpload,
                           prefs::kGoogleWorkspaceCloudUpload}) {
    pref_change_registrar_->Add(
        pref,
        base::BindRepeating(&CloudUploadPrefsWatcher::OnCloudUploadPrefChanged,
                            base::Unretained(this)));
  }

  // Performs initial sync.
  OnCloudUploadPrefChanged();
}

void CloudUploadPrefsWatcher::Shutdown() {
  pref_change_registrar_.reset();
}

void CloudUploadPrefsWatcher::OnCloudUploadPrefChanged() {
  if (!IsMicrosoftOfficeCloudUploadAllowed(profile_)) {
    UnsetMicrosoftOfficeAsDefaultHandlerIfNecessary(profile_);
  }

  if (!IsGoogleWorkspaceCloudUploadAllowed(profile_)) {
    UnsetGoogleWorkspaceAsDefaultHandlerIfNecessary(profile_);
  }

  const bool google_workspace_automated =
      IsGoogleWorkspaceCloudUploadAutomated(profile_);
  const bool microsoft_office_automated =
      IsMicrosoftOfficeCloudUploadAutomated(profile_);
  // A special case that is not supposed to happen in production; the agreed
  // decision is to ignore this setup and act as if both values were set to
  // `allowed` instead of `automated`.
  if (google_workspace_automated && microsoft_office_automated) {
    return;
  } else if (google_workspace_automated) {
    SetGoogleWorkspaceAsDefaultHandlerWithoutOverwriting(profile_);
  } else if (microsoft_office_automated) {
    SetMicrosoftOfficeAsDefaultHandlerWithoutOverwriting(profile_);
  }
}

}  // namespace

CloudUploadPrefsWatcherFactory* CloudUploadPrefsWatcherFactory::GetInstance() {
  static base::NoDestructor<CloudUploadPrefsWatcherFactory> instance;
  return instance.get();
}

CloudUploadPrefsWatcherFactory::CloudUploadPrefsWatcherFactory()
    : ProfileKeyedServiceFactory(
          "CloudUploadPrefsWatcherFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

CloudUploadPrefsWatcherFactory::~CloudUploadPrefsWatcherFactory() = default;

std::unique_ptr<KeyedService>
CloudUploadPrefsWatcherFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!chromeos::features::IsUploadOfficeToCloudForEnterpriseEnabled()) {
    return nullptr;
  }
  auto* profile = Profile::FromBrowserContext(context);
  if (!IsEligibleAndEnabledUploadOfficeToCloud(profile)) {
    return nullptr;
  }
  return CloudUploadPrefsWatcher::Create(Profile::FromBrowserContext(context));
}

bool CloudUploadPrefsWatcherFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace chromeos::cloud_upload
