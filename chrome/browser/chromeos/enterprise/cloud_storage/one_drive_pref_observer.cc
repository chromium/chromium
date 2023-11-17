// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/enterprise/cloud_storage/one_drive_pref_observer.h"

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extensions_browser_client.h"

namespace chromeos::cloud_storage {

namespace {

// This class is responsible for watching the prefs for a particular profile.
class OneDrivePrefObserver : public KeyedService {
 public:
  ~OneDrivePrefObserver() override;

  static std::unique_ptr<OneDrivePrefObserver> Create(Profile* profile);

  // KeyedService:
  void Shutdown() override;

 private:
  explicit OneDrivePrefObserver(Profile* profile);

  // Sets up watchers.
  void Init();

  // Serves as callback for pref changes.
  void OnMicrosoftOneDriveMountPrefChanged();
  void OnMicrosoftOneDriveAccountRestrictionsPrefChanged();

  raw_ptr<Profile> profile_ = nullptr;

  // The registrar used to watch prefs changes.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
};

OneDrivePrefObserver::OneDrivePrefObserver(Profile* profile)
    : profile_(profile),
      pref_change_registrar_(std::make_unique<PrefChangeRegistrar>()) {}

OneDrivePrefObserver::~OneDrivePrefObserver() = default;

std::unique_ptr<OneDrivePrefObserver> OneDrivePrefObserver::Create(
    Profile* profile) {
  auto watcher = base::WrapUnique(new OneDrivePrefObserver(profile));
  watcher->Init();
  return watcher;
}

void OneDrivePrefObserver::Init() {
  pref_change_registrar_->Init(profile_->GetPrefs());
  pref_change_registrar_->Add(
      prefs::kMicrosoftOneDriveMount,
      base::BindRepeating(
          &OneDrivePrefObserver::OnMicrosoftOneDriveMountPrefChanged,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kMicrosoftOneDriveAccountRestrictions,
      base::BindRepeating(&OneDrivePrefObserver::
                              OnMicrosoftOneDriveAccountRestrictionsPrefChanged,
                          base::Unretained(this)));
}

void OneDrivePrefObserver::Shutdown() {
  pref_change_registrar_.reset();
}

void OneDrivePrefObserver::OnMicrosoftOneDriveMountPrefChanged() {
  // TODO(b/294983416): Implement this change listener.
  NOTREACHED();
}

void OneDrivePrefObserver::OnMicrosoftOneDriveAccountRestrictionsPrefChanged() {
  // TODO(b/294983416): Implement this change listener.
  NOTREACHED();
}

}  // namespace

OneDrivePrefObserverFactory* OneDrivePrefObserverFactory::GetInstance() {
  static base::NoDestructor<OneDrivePrefObserverFactory> instance;
  return instance.get();
}

OneDrivePrefObserverFactory::OneDrivePrefObserverFactory()
    : ProfileKeyedServiceFactory("OneDrivePrefObserverFactory",
                                 ProfileSelections::BuildForRegularProfile()) {}

OneDrivePrefObserverFactory::~OneDrivePrefObserverFactory() = default;

std::unique_ptr<KeyedService>
OneDrivePrefObserverFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!chromeos::features::IsUploadOfficeToCloudEnabled() ||
      !chromeos::features::
          IsMicrosoftOneDriveIntegrationForEnterpriseEnabled()) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!profile->IsMainProfile()) {
    return nullptr;
  }
#endif

  return OneDrivePrefObserver::Create(profile);
}

bool OneDrivePrefObserverFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace chromeos::cloud_storage
