// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_file_system_provider.h"

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/file_system_provider/service_worker_lifetime_manager.h"
#include "chrome/browser/lacros/profile_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h"
#include "chromeos/lacros/lacros_service.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/image_loader.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/icons/extension_icon_set.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace {

const extensions::Extension* GetEnabledExtension(
    content::BrowserContext* browser_context,
    const extensions::ExtensionId& extension_id) {
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser_context);
  return registry->enabled_extensions().GetByID(extension_id);
}

// Loads an icon of a single size.
void LoadExtensionIcon(content::BrowserContext* browser_context,
                       const extensions::ExtensionId& extension_id,
                       int size,
                       extensions::ImageLoaderImageCallback callback) {
  const extensions::Extension* extension =
      GetEnabledExtension(browser_context, extension_id);
  if (!extension) {
    return;
  }
  extensions::ExtensionResource icon = extensions::IconsInfo::GetIconResource(
      extension, size, ExtensionIconSet::Match::kBigger);
  extensions::ImageLoader::Get(browser_context)
      ->LoadImageAsync(extension, icon, gfx::Size(size, size),
                       std::move(callback));
}

void OnLoadedIcon32x32(base::WeakPtr<Profile> weak_profile_ptr,
                       const extensions::ExtensionId& extension_id,
                       const gfx::Image& icon16x16,
                       const gfx::Image& icon32x32) {
  Profile* profile = weak_profile_ptr.get();
  if (!profile) {
    return;
  }
  const extensions::Extension* extension =
      GetEnabledExtension(profile, extension_id);
  if (!extension) {
    return;
  }
  auto* capabilities =
      extensions::FileSystemProviderCapabilities::Get(extension);
  if (!capabilities) {
    return;
  }

  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  int fsp_service_version =
      service->GetInterfaceVersion<crosapi::mojom::FileSystemProviderService>();
  if (fsp_service_version <
      int{crosapi::mojom::FileSystemProviderService::MethodMinVersions::
              kExtensionLoadedDeprecatedMinVersion}) {
    return;
  }

  crosapi::mojom::FileSystemSource source;
  switch (capabilities->source()) {
    case extensions::FileSystemProviderSource::SOURCE_FILE:
      source = crosapi::mojom::FileSystemSource::kFile;
      break;
    case extensions::FileSystemProviderSource::SOURCE_NETWORK:
      source = crosapi::mojom::FileSystemSource::kNetwork;
      break;
    case extensions::FileSystemProviderSource::SOURCE_DEVICE:
      source = crosapi::mojom::FileSystemSource::kDevice;
      break;
  }

  auto& fsp_service =
      service->GetRemote<crosapi::mojom::FileSystemProviderService>();
  if (fsp_service_version <
      int{crosapi::mojom::FileSystemProviderService::MethodMinVersions::
              kExtensionLoadedMinVersion}) {
    fsp_service->ExtensionLoadedDeprecated(
        capabilities->configurable(), capabilities->watchable(),
        capabilities->multiple_mounts(), source, extension->name(),
        extension->id());
    return;
  }

  fsp_service->ExtensionLoaded(
      capabilities->configurable(), capabilities->watchable(),
      capabilities->multiple_mounts(), source, extension->name(),
      extension->id(), icon16x16.AsImageSkia(), icon32x32.AsImageSkia());
}

void OnLoadedIcon16x16(base::WeakPtr<Profile> weak_profile_ptr,
                       const extensions::ExtensionId& extension_id,
                       const gfx::Image& icon16x16) {
  Profile* profile = weak_profile_ptr.get();
  if (!profile) {
    return;
  }
  LoadExtensionIcon(profile, extension_id, 32,
                    base::BindOnce(&OnLoadedIcon32x32, profile->GetWeakPtr(),
                                   extension_id, icon16x16));
}

}  // namespace

LacrosFileSystemProvider::LacrosFileSystemProvider() : receiver_{this} {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::FileSystemProviderService>())
    return;
  service->GetRemote<crosapi::mojom::FileSystemProviderService>()
      ->RegisterFileSystemProvider(receiver_.BindNewPipeAndPassRemote());

  Profile* main_profile = GetMainProfile();
  if (main_profile) {
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(main_profile);
    extension_observation_.Observe(registry);

    // Initial conditions
    for (const scoped_refptr<const extensions::Extension> extension :
         registry->enabled_extensions()) {
      OnExtensionLoaded(main_profile, extension.get());
    }
  }
}
LacrosFileSystemProvider::~LacrosFileSystemProvider() = default;

void LacrosFileSystemProvider::DeprecatedDeprecatedForwardOperation(
    const std::string& provider,
    int32_t histogram_value,
    const std::string& event_name,
    std::vector<base::Value> args) {
  DeprecatedForwardOperation(provider, histogram_value, event_name,
                             std::move(args), base::DoNothing());
}

void LacrosFileSystemProvider::DeprecatedForwardOperation(
    const std::string& provider,
    int32_t histogram_value,
    const std::string& event_name,
    std::vector<base::Value> args,
    ForwardOperationCallback callback) {
  base::Value::List list;
  for (auto& value : args) {
    list.Append(std::move(value));
  }
  ForwardOperation(provider, histogram_value, event_name, std::move(list),
                   base::DoNothing());
}

void LacrosFileSystemProvider::ForwardOperation(
    const std::string& provider,
    int32_t histogram_value,
    const std::string& event_name,
    base::Value::List args,
    ForwardOperationCallback callback) {
  ForwardRequest(
      provider, "", 0, histogram_value, event_name, std::move(args),
      base::BindOnce(
          [](ForwardOperationCallback callback,
             crosapi::mojom::FSPForwardResult result) {
            std::move(callback).Run(result !=
                                    crosapi::mojom::FSPForwardResult::kSuccess);
          },
          std::move(callback)));
}

void LacrosFileSystemProvider::ForwardRequest(
    const std::string& provider,
    const std::optional<std::string>& file_system_id,
    int64_t request_id,
    int32_t histogram_value,
    const std::string& event_name,
    base::Value::List args,
    ForwardRequestCallback callback) {
  Profile* main_profile = GetMainProfile();
  if (!main_profile) {
    LOG(ERROR) << "Could not get main profile";
    std::move(callback).Run(crosapi::mojom::FSPForwardResult::kInternalError);
    return;
  }

  extensions::EventRouter* router = extensions::EventRouter::Get(main_profile);
  if (!router) {
    LOG(ERROR) << "Could not get event router";
    std::move(callback).Run(crosapi::mojom::FSPForwardResult::kInternalError);
    return;
  }

  if (!router->ExtensionHasEventListener(provider, event_name)) {
    LOG(ERROR) << "Could not get event listener";
    std::move(callback).Run(crosapi::mojom::FSPForwardResult::kNoListener);
    return;
  }

  // Conversions are safe since the enum is stable. See documentation.
  int32_t lowest_valid_enum =
      static_cast<int32_t>(extensions::events::HistogramValue::UNKNOWN);
  int32_t highest_valid_enum =
      static_cast<int32_t>(extensions::events::HistogramValue::ENUM_BOUNDARY) -
      1;
  if (histogram_value < lowest_valid_enum ||
      histogram_value > highest_valid_enum) {
    LOG(ERROR) << "Invalid histogram";
    std::move(callback).Run(crosapi::mojom::FSPForwardResult::kInternalError);
    return;
  }
  extensions::events::HistogramValue histogram =
      static_cast<extensions::events::HistogramValue>(histogram_value);

  if (request_id > 0) {
    // request_id will be > 0 if Ash calls ForwardRequest (crosapi with
    // request_id as an argument).
    auto* sw_lifetime_manager =
        extensions::file_system_provider::ServiceWorkerLifetimeManager::Get(
            main_profile);
    if (!sw_lifetime_manager) {
      LOG(ERROR) << "Could not get service worker lifetime manager";
      std::move(callback).Run(crosapi::mojom::FSPForwardResult::kInternalError);
      return;
    }
    auto event = std::make_unique<extensions::Event>(histogram, event_name,
                                                     std::move(args));
    extensions::file_system_provider::RequestKey request_key{
        provider, file_system_id.value_or(""), request_id};
    event->did_dispatch_callback =
        sw_lifetime_manager->CreateDispatchCallbackForRequest(request_key);
    router->DispatchEventToExtension(provider, std::move(event));
    sw_lifetime_manager->StartRequest(request_key);
  } else {
    // request_id is 0 if Ash calls ForwardOperation (older crosapi without
    // request_id).
    auto event = std::make_unique<extensions::Event>(histogram, event_name,
                                                     std::move(args));
    router->DispatchEventToExtension(provider, std::move(event));
  }
  std::move(callback).Run(crosapi::mojom::FSPForwardResult::kSuccess);
}

void LacrosFileSystemProvider::CancelRequest(
    const std::string& provider,
    const std::optional<std::string>& file_system_id,
    int64_t request_id) {
  Profile* main_profile = GetMainProfile();
  if (!main_profile) {
    LOG(ERROR) << "Could not get main profile";
    return;
  }
  auto* sw_lifetime_manager =
      extensions::file_system_provider::ServiceWorkerLifetimeManager::Get(
          main_profile);
  sw_lifetime_manager->FinishRequest(
      extensions::file_system_provider::RequestKey{
          provider, file_system_id.value_or(""), request_id});
}

void LacrosFileSystemProvider::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  if (!extension->permissions_data()->HasAPIPermission(
          extensions::mojom::APIPermissionID::kFileSystemProvider)) {
    return;
  }
  if (!extensions::FileSystemProviderCapabilities::Get(extension)) {
    return;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context);
  LoadExtensionIcon(profile, extension->id(), 16,
                    base::BindOnce(&OnLoadedIcon16x16, profile->GetWeakPtr(),
                                   extension->id()));
}

void LacrosFileSystemProvider::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (service
          ->GetInterfaceVersion<crosapi::mojom::FileSystemProviderService>() <
      int{crosapi::mojom::FileSystemProviderService::MethodMinVersions::
              kExtensionUnloadedMinVersion}) {
    return;
  }
  service->GetRemote<crosapi::mojom::FileSystemProviderService>()
      ->ExtensionUnloaded(
          extension->id(),
          reason == extensions::UnloadedExtensionReason::PROFILE_SHUTDOWN);
}

void LacrosFileSystemProvider::OnShutdown(
    extensions::ExtensionRegistry* registry) {
  extension_observation_.Reset();
}
