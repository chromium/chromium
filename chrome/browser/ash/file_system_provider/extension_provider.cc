// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/extension_provider.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_source.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/file_system_provider/mount_request_handler.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system.h"
#include "chrome/browser/ash/file_system_provider/request_dispatcher_impl.h"
#include "chrome/browser/ash/file_system_provider/throttled_file_system.h"
#include "chrome/browser/chromeos/extensions/file_system_provider/service_worker_lifetime_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/permissions/permissions_data.h"

namespace ash {
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

extensions::file_system_provider::ServiceWorkerLifetimeManager*
GetServiceWorkerLifetimeManager(Profile* profile) {
  if (!features::IsUploadOfficeToCloudEnabled()) {
    return nullptr;
  }
  return extensions::file_system_provider::ServiceWorkerLifetimeManager::Get(
      profile);
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

RequestManager* ExtensionProvider::GetRequestManager() {
  return request_manager_.get();
}

bool ExtensionProvider::RequestMount(Profile* profile,
                                     RequestMountCallback callback) {
  extensions::EventRouter* const event_router =
      extensions::EventRouter::Get(profile);
  DCHECK(event_router);
  // Create two callbacks of which only one will be called because
  // RequestManager::CreateRequest() is guaranteed not to call |callback| if it
  // signals an error (by returning request_id == 0).
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  const int request_id = request_manager_->CreateRequest(
      REQUEST_MOUNT,
      std::make_unique<MountRequestHandler>(request_dispatcher_.get(),
                                            std::move(split_callback.first)));
  if (!request_id) {
    std::move(split_callback.second).Run(base::File::FILE_ERROR_FAILED);
    return false;
  }

  return true;
}

ExtensionProvider::ExtensionProvider(
    Profile* profile,
    const extensions::ExtensionId& extension_id,
    const ProvidingExtensionInfo& info)
    : provider_id_(ProviderId::CreateFromExtensionId(extension_id)) {
  request_dispatcher_ = std::make_unique<RequestDispatcherImpl>(
      extension_id, extensions::EventRouter::Get(profile),
      base::BindRepeating(&ExtensionProvider::OnLacrosOperationForwarded,
                          weak_ptr_factory_.GetWeakPtr()),
      GetServiceWorkerLifetimeManager(profile));
  request_manager_ = std::make_unique<RequestManager>(
      profile, /*notification_manager=*/nullptr);
  capabilities_.configurable = info.capabilities.configurable();
  capabilities_.watchable = info.capabilities.watchable();
  capabilities_.multiple_mounts = info.capabilities.multiple_mounts();
  capabilities_.source = info.capabilities.source();
  name_ = info.name;
  ObserveAppServiceForIcons(profile);
}

ExtensionProvider::ExtensionProvider(Profile* profile,
                                     ProviderId id,
                                     Capabilities capabilities,
                                     std::string name)
    : provider_id_(std::move(id)),
      capabilities_(std::move(capabilities)),
      name_(std::move(name)) {
  request_dispatcher_ = std::make_unique<RequestDispatcherImpl>(
      provider_id_.GetExtensionId(), extensions::EventRouter::Get(profile),
      base::BindRepeating(&ExtensionProvider::OnLacrosOperationForwarded,
                          weak_ptr_factory_.GetWeakPtr()),
      GetServiceWorkerLifetimeManager(profile));
  request_manager_ = std::make_unique<RequestManager>(
      profile, /*notification_manager=*/nullptr);
  ObserveAppServiceForIcons(profile);
}

ExtensionProvider::~ExtensionProvider() = default;

void ExtensionProvider::ObserveAppServiceForIcons(Profile* profile) {
  if (apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    auto* AppServiceProxy =
        apps::AppServiceProxyFactory::GetForProfile(profile);

    // AppService loading apps from extensions might be slow due to async. Even
    // if the app doesn't exist in AppRegistryCache, it might be added later. So
    // we still observe the AppRegistry to catch the app update information.
    Observe(&AppServiceProxy->AppRegistryCache());

    if (AppServiceProxy->AppRegistryCache().GetAppType(
            provider_id_.GetExtensionId()) != apps::AppType::kUnknown) {
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

void ExtensionProvider::OnLacrosOperationForwarded(int request_id,
                                                   base::File::Error error) {
  request_manager_->RejectRequest(request_id, std::make_unique<RequestValue>(),
                                  error);
}

}  // namespace file_system_provider
}  // namespace ash
