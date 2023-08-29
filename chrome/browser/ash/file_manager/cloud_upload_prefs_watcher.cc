// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/cloud_upload_prefs_watcher.h"

#include "base/notreached.h"
#include "chrome/browser/chromeos/upload_office_to_cloud/upload_office_to_cloud.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

namespace chromeos::cloud_upload {

namespace {

// This class is responsible for watching the prefs for a particular profile.
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
  // TODO(b/296282654): Check pref values and update file handlers accordingly.
  NOTIMPLEMENTED_LOG_ONCE();
}

}  // namespace

CloudUploadPrefsWatcherFactory* CloudUploadPrefsWatcherFactory::GetInstance() {
  static base::NoDestructor<CloudUploadPrefsWatcherFactory> instance;
  return instance.get();
}

CloudUploadPrefsWatcherFactory::CloudUploadPrefsWatcherFactory()
    : ProfileKeyedServiceFactory("CloudUploadPrefsWatcherFactory",
                                 ProfileSelections::BuildForRegularProfile()) {}

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
