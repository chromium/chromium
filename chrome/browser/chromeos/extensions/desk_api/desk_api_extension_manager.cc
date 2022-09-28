// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/desk_api/desk_api_extension_manager.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/browser_resources.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_system.h"

using ::extensions::ComponentLoader;

namespace chromeos {
namespace {

class DeskApiExtensionManagerFactory : public ProfileKeyedServiceFactory {
 public:
  DeskApiExtensionManagerFactory();
  DeskApiExtensionManagerFactory(const DeskApiExtensionManagerFactory&) =
      delete;
  DeskApiExtensionManagerFactory& operator=(
      const DeskApiExtensionManagerFactory&) = delete;
  ~DeskApiExtensionManagerFactory() override;

  // Returns an instance of `DeskApiExtensionManager` for the
  // given profile.
  DeskApiExtensionManager* GetForProfile(Profile* profile);

 private:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

DeskApiExtensionManagerFactory::DeskApiExtensionManagerFactory()
    : ProfileKeyedServiceFactory("DeskApiExtensionManager") {}

DeskApiExtensionManagerFactory::~DeskApiExtensionManagerFactory() = default;

DeskApiExtensionManager* DeskApiExtensionManagerFactory::GetForProfile(
    Profile* profile) {
  DCHECK(profile);
  return static_cast<DeskApiExtensionManager*>(
      GetServiceForBrowserContext(profile, true));
}

KeyedService* DeskApiExtensionManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* const profile = Profile::FromBrowserContext(context);
  auto* const component_loader = ::extensions::ExtensionSystem::Get(profile)
                                     ->extension_service()
                                     ->component_loader();
  return new DeskApiExtensionManager(
      component_loader, profile,
      std::make_unique<DeskApiExtensionManager::Delegate>());
}

}  // namespace

void DeskApiExtensionManager::Delegate::InstallExtension(
    ComponentLoader* component_loader) {
  component_loader->Add(IDR_DESK_API_MANIFEST,
                        base::FilePath(FILE_PATH_LITERAL("chromeos/desk_api")));
}

void DeskApiExtensionManager::Delegate::UninstallExtension(
    ComponentLoader* component_loader) {
  component_loader->Remove(extension_misc::kDeskApiExtensionId);
}

bool DeskApiExtensionManager::Delegate::IsProfileAffiliated(
    Profile* profile) const {
  if (profile->IsOffTheRecord())
    return false;

  return ::chrome::enterprise_util::IsProfileAffiliated(profile);
}

bool DeskApiExtensionManager::Delegate::IsExtensionInstalled(
    ComponentLoader* component_loader) const {
  return component_loader->Exists(extension_misc::kDeskApiExtensionId);
}

// static
DeskApiExtensionManager* DeskApiExtensionManager::GetForProfile(
    Profile* profile) {
  return static_cast<DeskApiExtensionManagerFactory*>(GetFactory())
      ->GetForProfile(profile);
}

DeskApiExtensionManager::DeskApiExtensionManager(
    ComponentLoader* component_loader,
    Profile* profile,
    std::unique_ptr<DeskApiExtensionManager::Delegate> delegate)
    : component_loader_(component_loader),
      profile_(profile),
      delegate_(std::move(delegate)) {
  Init();
}

DeskApiExtensionManager::~DeskApiExtensionManager() = default;

// static
BrowserContextKeyedServiceFactory* DeskApiExtensionManager::GetFactory() {
  static base::NoDestructor<DeskApiExtensionManagerFactory> g_factory;
  return g_factory.get();
}

void DeskApiExtensionManager::Init() {
  if (CanInstallExtension()) {
    delegate_->InstallExtension(component_loader_);
  } else {
    // Remove extension if installed because it does not meet necessary
    // pre-conditions.
    RemoveExtensionIfInstalled();
  }

  registrar_.Init(profile_->GetPrefs());

  if (delegate_->IsProfileAffiliated(profile_)) {
    // Setup registrar so it starts listening to relevant pref changes.
    registrar_.Add(::prefs::kDeskAPIThirdPartyAccessEnabled,
                   base::BindRepeating(&DeskApiExtensionManager::OnPrefChanged,
                                       weak_ptr_factory_.GetWeakPtr()));
  }
}

bool DeskApiExtensionManager::CanInstallExtension() const {
  if (!delegate_->IsProfileAffiliated(profile_)) {
    return false;
  }

  const auto& pref_service = *profile_->GetPrefs();
  return pref_service.HasPrefPath(::prefs::kDeskAPIThirdPartyAccessEnabled) &&
         pref_service.GetBoolean(::prefs::kDeskAPIThirdPartyAccessEnabled);
}

void DeskApiExtensionManager::OnPrefChanged() {
  if (CanInstallExtension()) {
    // This will be a no-op if the extension is already installed with the same
    // version, so it is okay to attempt an installation here.
    delegate_->InstallExtension(component_loader_);
    return;
  }

  // Remove/unload component extension if installed because it does
  // not meet necessary pre-conditions.
  RemoveExtensionIfInstalled();
}

void DeskApiExtensionManager::RemoveExtensionIfInstalled() {
  if (delegate_->IsExtensionInstalled(component_loader_)) {
    delegate_->UninstallExtension(component_loader_);
  }
}

}  // namespace chromeos
