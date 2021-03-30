// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/extension_provider.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "chrome/browser/apps/app_service/app_icon_source.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system.h"
#include "chrome/browser/chromeos/file_system_provider/throttled_file_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/permissions/permissions_data.h"

namespace chromeos {
namespace file_system_provider {
namespace {

// Returns boolean indicating success. result->capabilities contains the
// capabilites of the extension.
bool GetProvidingExtensionInfo(const extensions::ExtensionId& extension_id,
                               ProvidingExtensionInfo* result,
                               extensions::ExtensionRegistry* registry) {
  DCHECK(result);
  DCHECK(registry);

  const extensions::Extension* const extension = registry->GetExtensionById(
      extension_id, extensions::ExtensionRegistry::ENABLED);
  if (!extension ||
      !extension->permissions_data()->HasAPIPermission(
          extensions::mojom::APIPermissionID::kFileSystemProvider)) {
    return false;
  }

  result->extension_id = extension->id();
  result->name = extension->name();
  const extensions::FileSystemProviderCapabilities* const capabilities =
      extensions::FileSystemProviderCapabilities::Get(extension);
  DCHECK(capabilities);
  result->capabilities = *capabilities;

  return true;
}

}  // namespace

ProvidingExtensionInfo::ProvidingExtensionInfo() = default;

ProvidingExtensionInfo::~ProvidingExtensionInfo() = default;

// static
std::unique_ptr<ProviderInterface> ExtensionProvider::Create(
    extensions::ExtensionRegistry* registry,
    const extensions::ExtensionId& extension_id) {
  ProvidingExtensionInfo info;
  if (!GetProvidingExtensionInfo(extension_id, &info, registry))
    return nullptr;

  return std::make_unique<ExtensionProvider>(
      Profile::FromBrowserContext(registry->browser_context()), extension_id,
      info);
}

std::unique_ptr<ProvidedFileSystemInterface>
ExtensionProvider::CreateProvidedFileSystem(
    Profile* profile,
    const ProvidedFileSystemInfo& file_system_info) {
  DCHECK(profile);
  return std::make_unique<ThrottledFileSystem>(
      std::make_unique<ProvidedFileSystem>(profile, file_system_info));
}

const Capabilities& ExtensionProvider::GetCapabilities() const {
  return capabilities_;
}

const ProviderId& ExtensionProvider::GetId() const {
  return provider_id_;
}

const std::string& ExtensionProvider::GetName() const {
  return name_;
}

const IconSet& ExtensionProvider::GetIconSet() const {
  return icon_set_;
}

bool ExtensionProvider::RequestMount(Profile* profile) {
  extensions::EventRouter* const event_router =
      extensions::EventRouter::Get(profile);
  DCHECK(event_router);

  if (!event_router->ExtensionHasEventListener(
          provider_id_.GetExtensionId(), extensions::api::file_system_provider::
                                             OnMountRequested::kEventName)) {
    return false;
  }

  event_router->DispatchEventToExtension(
      provider_id_.GetExtensionId(),
      std::make_unique<extensions::Event>(
          extensions::events::FILE_SYSTEM_PROVIDER_ON_MOUNT_REQUESTED,
          extensions::api::file_system_provider::OnMountRequested::kEventName,
          std::unique_ptr<base::ListValue>(new base::ListValue())));

  return true;
}

ExtensionProvider::ExtensionProvider(
    Profile* profile,
    const extensions::ExtensionId& extension_id,
    const ProvidingExtensionInfo& info)
    : provider_id_(ProviderId::CreateFromExtensionId(extension_id)) {
  capabilities_.configurable = info.capabilities.configurable();
  capabilities_.watchable = info.capabilities.watchable();
  capabilities_.multiple_mounts = info.capabilities.multiple_mounts();
  capabilities_.source = info.capabilities.source();
  name_ = info.name;

  if (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon) &&
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    auto* AppServiceProxy =
        apps::AppServiceProxyFactory::GetForProfile(profile);

    // AppService loading apps from extensions might be slow due to async. Even
    // if the app doesn't exist in AppRegistryCache, it might be added later. So
    // we still observe the AppRegistry to catch the app update information.
    Observe(&AppServiceProxy->AppRegistryCache());

    if (AppServiceProxy->AppRegistryCache().GetAppType(
            provider_id_.GetExtensionId()) != apps::mojom::AppType::kUnknown) {
      icon_set_.SetIcon(
          IconSet::IconSize::SIZE_16x16,
          apps::AppIconSource::GetIconURL(provider_id_.GetExtensionId(), 16));
      icon_set_.SetIcon(
          IconSet::IconSize::SIZE_32x32,
          apps::AppIconSource::GetIconURL(provider_id_.GetExtensionId(), 32));
      return;
    }
  }

  icon_set_.SetIcon(IconSet::IconSize::SIZE_16x16,
                    GURL(std::string("chrome://extension-icon/") +
                         provider_id_.GetExtensionId() + "/16/1"));
  icon_set_.SetIcon(IconSet::IconSize::SIZE_32x32,
                    GURL(std::string("chrome://extension-icon/") +
                         provider_id_.GetExtensionId() + "/32/1"));
}

ExtensionProvider::~ExtensionProvider() = default;

void ExtensionProvider::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.AppId() != provider_id_.GetExtensionId() ||
      !update.IconKeyChanged()) {
    return;
  }

  icon_set_.SetIcon(
      IconSet::IconSize::SIZE_16x16,
      apps::AppIconSource::GetIconURL(provider_id_.GetExtensionId(), 16));
  icon_set_.SetIcon(
      IconSet::IconSize::SIZE_32x32,
      apps::AppIconSource::GetIconURL(provider_id_.GetExtensionId(), 32));
}

void ExtensionProvider::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}

}  // namespace file_system_provider
}  // namespace chromeos
