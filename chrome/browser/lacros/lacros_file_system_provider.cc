// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_file_system_provider.h"

#include "base/ranges/algorithm.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h"
#include "chromeos/lacros/lacros_service.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permissions_data.h"

namespace {

// Returns the single main profile, or nullptr if none is found.
Profile* GetMainProfile() {
  auto profiles = g_browser_process->profile_manager()->GetLoadedProfiles();
  const auto main_it = base::ranges::find_if(profiles, &Profile::IsMainProfile);
  if (main_it == profiles.end())
    return nullptr;
  return *main_it;
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
  Profile* main_profile = GetMainProfile();
  if (!main_profile) {
    LOG(ERROR) << "Could not get main profile";
    std::move(callback).Run(/*delivery_failure=*/true);
    return;
  }

  extensions::EventRouter* router = extensions::EventRouter::Get(main_profile);
  if (!router) {
    LOG(ERROR) << "Could not get event router";
    std::move(callback).Run(/*delivery_failure=*/true);
    return;
  }

  if (!router->ExtensionHasEventListener(provider, event_name)) {
    LOG(ERROR) << "Could not get event listener";
    std::move(callback).Run(/*delivery_failure=*/true);
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
    std::move(callback).Run(/*delivery_failure=*/true);
    return;
  }
  extensions::events::HistogramValue histogram =
      static_cast<extensions::events::HistogramValue>(histogram_value);

  auto event = std::make_unique<extensions::Event>(histogram, event_name,
                                                   std::move(args));
  router->DispatchEventToExtension(provider, std::move(event));
  std::move(callback).Run(/*delivery_failure=*/false);
}

void LacrosFileSystemProvider::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  if (!extension->permissions_data()->HasAPIPermission(
          extensions::mojom::APIPermissionID::kFileSystemProvider)) {
    return;
  }
  const extensions::FileSystemProviderCapabilities* const capabilities =
      extensions::FileSystemProviderCapabilities::Get(extension);
  if (!capabilities)
    return;

  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (service->GetInterfaceVersion(
          crosapi::mojom::FileSystemProviderService::Uuid_) <
      int{crosapi::mojom::FileSystemProviderService::MethodMinVersions::
              kExtensionLoadedMinVersion}) {
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

  service->GetRemote<crosapi::mojom::FileSystemProviderService>()
      ->ExtensionLoaded(capabilities->configurable(), capabilities->watchable(),
                        capabilities->multiple_mounts(), source,
                        extension->name(), extension->id());
}

void LacrosFileSystemProvider::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (service->GetInterfaceVersion(
          crosapi::mojom::FileSystemProviderService::Uuid_) <
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
