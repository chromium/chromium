// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/desk_api/desk_api_extension_manager.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
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
#include "extensions/common/url_pattern.h"
#include "ui/base/resource/resource_bundle.h"

using ::extensions::ComponentLoader;

namespace chromeos {
namespace {

// Tag in the manifest to be replaced.
constexpr char kDomainsTag[] = "\"$DOMAIN_LIST\"";

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
    : ProfileKeyedServiceFactory(
          "DeskApiExtensionManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

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
    ComponentLoader* component_loader,
    const std::string& manifest_content) {
  component_loader->Add(manifest_content,
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
  LoadOrUnloadExtension();

  registrar_.Init(profile_->GetPrefs());

  if (delegate_->IsProfileAffiliated(profile_)) {
    // Setup registrar so it starts listening to relevant pref changes.
    registrar_.Add(::prefs::kDeskAPIThirdPartyAccessEnabled,
                   base::BindRepeating(&DeskApiExtensionManager::OnPrefChanged,
                                       weak_ptr_factory_.GetWeakPtr()));
    registrar_.Add(::prefs::kDeskAPIThirdPartyAllowlist,
                   base::BindRepeating(&DeskApiExtensionManager::OnPrefChanged,
                                       weak_ptr_factory_.GetWeakPtr()));
  }
}

bool DeskApiExtensionManager::CanInstallExtension() const {
  if (!delegate_->IsProfileAffiliated(profile_)) {
    return false;
  }

  const auto& pref_service = *profile_->GetPrefs();
  // Defaults to not enabled.
  if (!pref_service.HasPrefPath(::prefs::kDeskAPIThirdPartyAccessEnabled))
    return false;

  if (!pref_service.GetBoolean(::prefs::kDeskAPIThirdPartyAccessEnabled))
    return false;

  const std::string manifest = GetManifest();

  // Cannot install extension when there is no valid domains in allowlist.
  if (manifest.empty())
    return false;

  return true;
}

std::string DeskApiExtensionManager::GetManifest() const {
  const auto* pref_service = profile_->GetPrefs();
  std::vector<std::string> domains;

  // Prepare the domain allowlist to follow the Chrome extension Manifest V3
  // `externally_connectable` `matches` property format. See more
  // https://developer.chrome.com/docs/extensions/mv3/manifest/externally_connectable/#reference
  for (const auto& domain :
       pref_service->GetList(::prefs::kDeskAPIThirdPartyAllowlist)) {
    URLPattern pattern(URLPattern::SCHEME_ALL);
    if (pattern.Parse(domain.GetString()) !=
        URLPattern::ParseResult::kSuccess) {
      LOG(WARNING) << "Desk API ignored invalid URL pattern: "
                   << domain.GetString();
      continue;
    }

    domains.push_back("\"" + domain.GetString() + "\"");
  }

  if (domains.size() == 0) {
    return "";
  }

  const std::string domain_list = base::JoinString(domains, ",");

  std::string manifest_contents(
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_DESK_API_MANIFEST));
  DCHECK(manifest_contents.find(kDomainsTag) != std::string::npos);
  base::ReplaceFirstSubstringAfterOffset(&manifest_contents, 0, kDomainsTag,
                                         domain_list);

  return manifest_contents;
}

void DeskApiExtensionManager::OnPrefChanged() {
  LoadOrUnloadExtension();
}

void DeskApiExtensionManager::LoadOrUnloadExtension() {
  if (CanInstallExtension()) {
    // This will update the extension if the allowlist changes, even if the
    // extension version remains the same.
    delegate_->InstallExtension(component_loader_, GetManifest());
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

// static
void DeskApiExtensionManager::EnsureFactoryBuilt() {
  DeskApiExtensionManager::GetFactory();
}

}  // namespace chromeos
