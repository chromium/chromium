// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/enterprise/cloud_storage/one_drive_pref_observer.h"

#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/chromeos/enterprise/cloud_storage/policy_utils.h"
#include "chrome/browser/chromeos/extensions/odfs_config_private/odfs_config_private_api.h"
#include "chrome/browser/chromeos/upload_office_to_cloud/upload_office_to_cloud.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/common/extensions/api/odfs_config_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

using extensions::api::odfs_config_private::AccountRestrictionsInfo;
using extensions::api::odfs_config_private::Mount;
using extensions::api::odfs_config_private::MountInfo;

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

  void BroadcastModeChanged(Mount mode);
  void BroadcastAccountRestrictionsChanged(base::Value::List restrictions);

  void MaybeUninstallOdfsExtension(Mount mode);

  // Keyed services are shut down from the embedder's destruction of the profile
  // and this pointer is reset in `ShutDown`. Therefore it is safe to use this
  // raw pointer.
  raw_ptr<Profile> profile_ = nullptr;

  // This keyed service depends on the EventRouter keyed service and will be
  // destroyed before the EventRouter. Therefore it is safe to use this raw
  // pointer.
  raw_ptr<extensions::EventRouter> event_router_ = nullptr;

  // The registrar used to watch prefs changes.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
};

OneDrivePrefObserver::OneDrivePrefObserver(Profile* profile)
    : profile_(profile),
      event_router_(extensions::EventRouter::Get(profile)),
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
  OnMicrosoftOneDriveMountPrefChanged();
  OnMicrosoftOneDriveAccountRestrictionsPrefChanged();
}

void OneDrivePrefObserver::Shutdown() {
  pref_change_registrar_.reset();
  event_router_ = nullptr;
  profile_ = nullptr;
}

void OneDrivePrefObserver::OnMicrosoftOneDriveMountPrefChanged() {
  const Mount mount = GetMicrosoftOneDriveMount(profile_);
  BroadcastModeChanged(mount);
  MaybeUninstallOdfsExtension(mount);
}

void OneDrivePrefObserver::OnMicrosoftOneDriveAccountRestrictionsPrefChanged() {
  BroadcastAccountRestrictionsChanged(
      GetMicrosoftOneDriveAccountRestrictions(profile_.get()));
}

void OneDrivePrefObserver::MaybeUninstallOdfsExtension(Mount mount) {
  if (cloud_upload::IsMicrosoftOfficeOneDriveIntegrationAllowed(profile_) ||
      !CHECK_DEREF(extensions::ExtensionRegistry::Get(profile_))
           .enabled_extensions()
           .GetByID(extension_misc::kODFSExtensionId)) {
    return;
  }
  CHECK_DEREF(extensions::ExtensionSystem::Get(profile_)->extension_service())
      .RemoveComponentExtension(extension_misc::kODFSExtensionId);
}

}  // namespace

OneDrivePrefObserverFactory* OneDrivePrefObserverFactory::GetInstance() {
  static base::NoDestructor<OneDrivePrefObserverFactory> instance;
  return instance.get();
}

OneDrivePrefObserverFactory::OneDrivePrefObserverFactory()
    : ProfileKeyedServiceFactory(
          "OneDrivePrefObserverFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
  DependsOn(extensions::EventRouterFactory::GetInstance());
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

OneDrivePrefObserverFactory::~OneDrivePrefObserverFactory() = default;

std::unique_ptr<KeyedService>
OneDrivePrefObserverFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!features::IsUploadOfficeToCloudEnabled() ||
      !features::IsMicrosoftOneDriveIntegrationForEnterpriseEnabled()) {
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

void OneDrivePrefObserver::BroadcastModeChanged(Mount mode) {
  if (!event_router_) {
    CHECK_IS_TEST();
    return;
  }

  MountInfo metadata;
  metadata.mode = mode;

  auto event = std::make_unique<extensions::Event>(
      extensions::events::ODFS_CONFIG_PRIVATE_MOUNT_CHANGED,
      extensions::api::odfs_config_private::OnMountChanged::kEventName,
      extensions::api::odfs_config_private::OnMountChanged::Create(
          std::move(metadata)),
      profile_);

  event_router_->BroadcastEvent(std::move(event));
}

void OneDrivePrefObserver::BroadcastAccountRestrictionsChanged(
    base::Value::List restrictions) {
  if (!event_router_) {
    CHECK_IS_TEST();
    return;
  }

  std::vector<std::string> restrictions_vector;
  for (auto& restriction : restrictions) {
    if (restriction.is_string()) {
      restrictions_vector.emplace_back(restriction.GetString());
    }
  }

  AccountRestrictionsInfo event_data;
  event_data.restrictions = std::move(restrictions_vector);

  auto event = std::make_unique<extensions::Event>(
      extensions::events::ODFS_CONFIG_PRIVATE_ACCOUNT_RESTRICTIONS_CHANGED,
      extensions::api::odfs_config_private::OnAccountRestrictionsChanged::
          kEventName,
      extensions::api::odfs_config_private::OnAccountRestrictionsChanged::
          Create(event_data),
      profile_);

  event_router_->BroadcastEvent(std::move(event));
}

}  // namespace chromeos::cloud_storage
