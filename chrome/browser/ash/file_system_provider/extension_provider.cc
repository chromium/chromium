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

extensions::file_system_provider::ServiceWorkerLifetimeManager*
GetServiceWorkerLifetimeManager(Profile* profile) {
  if (!features::IsUploadOfficeToCloudEnabled()) {
    return nullptr;
  }
  return extensions::file_system_provider::ServiceWorkerLifetimeManager::Get(
      profile);
}

IconSet DefaultIconSet(const extensions::ExtensionId& extension_id) {
  IconSet icon_set;
  icon_set.SetIcon(
      IconSet::IconSize::SIZE_16x16,
      GURL(std::string("chrome://extension-icon/") + extension_id + "/16/1"));
  icon_set.SetIcon(
      IconSet::IconSize::SIZE_32x32,
      GURL(std::string("chrome://extension-icon/") + extension_id + "/32/1"));
  return icon_set;
}

IconSet AppServiceIconSet(const extensions::ExtensionId& extension_id) {
  IconSet icon_set;
  icon_set.SetIcon(IconSet::IconSize::SIZE_16x16,
                   apps::AppIconSource::GetIconURL(extension_id, 16));
  icon_set.SetIcon(IconSet::IconSize::SIZE_32x32,
                   apps::AppIconSource::GetIconURL(extension_id, 32));
  return icon_set;
}

}  // namespace

// static
std::unique_ptr<ProviderInterface> ExtensionProvider::Create(
    extensions::ExtensionRegistry* registry,
    const extensions::ExtensionId& extension_id) {
  const extensions::Extension* const extension = registry->GetExtensionById(
      extension_id, extensions::ExtensionRegistry::ENABLED);
  if (!extension ||
      !extension->permissions_data()->HasAPIPermission(
          extensions::mojom::APIPermissionID::kFileSystemProvider)) {
    return nullptr;
  }

  const extensions::FileSystemProviderCapabilities* const capabilities =
      extensions::FileSystemProviderCapabilities::Get(extension);
  DCHECK(capabilities);

  return std::make_unique<ExtensionProvider>(
      Profile::FromBrowserContext(registry->browser_context()),
      ProviderId::CreateFromExtensionId(extension->id()),
      Capabilities{.configurable = capabilities->configurable(),
                   .watchable = capabilities->watchable(),
                   .multiple_mounts = capabilities->multiple_mounts(),
                   .source = capabilities->source()},
      extension->name(),
      /*icon_set=*/absl::nullopt);
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

ExtensionProvider::ExtensionProvider(Profile* profile,
                                     ProviderId id,
                                     Capabilities capabilities,
                                     std::string name,
                                     absl::optional<IconSet> icon_set)
    : provider_id_(std::move(id)),
      capabilities_(std::move(capabilities)),
      name_(std::move(name)),
      icon_set_(
          icon_set.value_or(DefaultIconSet(provider_id_.GetExtensionId()))) {
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
      icon_set_ = AppServiceIconSet(provider_id_.GetExtensionId());
    }
  }
}

void ExtensionProvider::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.AppId() != provider_id_.GetExtensionId() ||
      !update.IconKeyChanged()) {
    return;
  }
  icon_set_ = AppServiceIconSet(provider_id_.GetExtensionId());
}

void ExtensionProvider::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}

void ExtensionProvider::OnLacrosOperationForwarded(int request_id,
                                                   base::File::Error error) {
  request_manager_->RejectRequest(request_id, RequestValue(), error);
}

}  // namespace file_system_provider
}  // namespace ash
