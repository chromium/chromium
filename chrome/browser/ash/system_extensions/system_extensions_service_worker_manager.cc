// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/system_extensions_service_worker_manager.h"

#include "chrome/browser/ash/system_extensions/system_extensions_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"

namespace ash {

SystemExtensionsServiceWorkerManager::SystemExtensionsServiceWorkerManager(
    Profile* profile,
    SystemExtensionsRegistry& registry)
    : profile_(profile), registry_(registry) {}

SystemExtensionsServiceWorkerManager::~SystemExtensionsServiceWorkerManager() =
    default;

void SystemExtensionsServiceWorkerManager::RegisterServiceWorker(
    const SystemExtensionId& system_extension_id) {
  auto* system_extension = registry_->GetById(system_extension_id);
  if (!system_extension) {
    LOG(ERROR) << "Tried to install service worker for non-existent extension";
    return;
  }

  blink::mojom::ServiceWorkerRegistrationOptions options(
      system_extension->base_url, blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kImports);
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(options.scope));

  auto* worker_context =
      profile_->GetDefaultStoragePartition()->GetServiceWorkerContext();
  worker_context->RegisterServiceWorker(
      system_extension->service_worker_url, key, options,
      base::BindOnce(
          &SystemExtensionsServiceWorkerManager::NotifyServiceWorkerRegistered,
          weak_ptr_factory_.GetWeakPtr(), system_extension_id));
}

void SystemExtensionsServiceWorkerManager::UnregisterServiceWorker(
    const SystemExtensionId& system_extension_id) {
  auto* system_extension = registry_->GetById(system_extension_id);
  if (!system_extension) {
    return;
  }

  const GURL& scope = system_extension->base_url;
  const url::Origin& origin = url::Origin::Create(system_extension->base_url);

  auto* worker_context =
      profile_->GetDefaultStoragePartition()->GetServiceWorkerContext();
  const blink::StorageKey key = blink::StorageKey::CreateFirstParty(origin);
  worker_context->UnregisterServiceWorker(
      scope, key,
      base::BindOnce(&SystemExtensionsServiceWorkerManager::
                         NotifyServiceWorkerUnregistered,
                     weak_ptr_factory_.GetWeakPtr(), system_extension_id));
}

void SystemExtensionsServiceWorkerManager::StartServiceWorker(
    const SystemExtensionId& system_extension_id,
    StartServiceWorkerCallback callback) {
  auto* system_extension = registry_->GetById(system_extension_id);
  if (!system_extension) {
    return;
  }

  auto [callback1, callback2] = base::SplitOnceCallback(std::move(callback));

  const GURL& scope = system_extension->base_url;
  const url::Origin& origin = url::Origin::Create(system_extension->base_url);
  const blink::StorageKey key = blink::StorageKey::CreateFirstParty(origin);

  auto* worker_context =
      profile_->GetDefaultStoragePartition()->GetServiceWorkerContext();
  worker_context->StartWorkerForScope(
      scope, key,
      base::BindOnce(
          &SystemExtensionsServiceWorkerManager::OnStartServiceWorkerSuccess,
          weak_ptr_factory_.GetWeakPtr(), system_extension_id,
          std::move(callback1)),
      base::BindOnce(
          &SystemExtensionsServiceWorkerManager::OnStartServiceWorkerFailure,
          weak_ptr_factory_.GetWeakPtr(), system_extension_id,
          std::move(callback2)));
}

void SystemExtensionsServiceWorkerManager::OnStartServiceWorkerSuccess(
    const SystemExtensionId& system_extension_id,
    StartServiceWorkerCallback callback,
    int64_t service_worker_version_id,
    int service_worker_process_id,
    int service_worker_thread_id) {
  std::move(callback).Run(SystemExtensionsServiceWorkerInfo{
      .system_extension_id = system_extension_id,
      .service_worker_version_id = service_worker_version_id,
      .service_worker_process_id = service_worker_process_id});
}

void SystemExtensionsServiceWorkerManager::OnStartServiceWorkerFailure(
    const SystemExtensionId& system_extension_id,
    StartServiceWorkerCallback callback,
    blink::ServiceWorkerStatusCode service_worker_status_code) {
  std::move(callback).Run(service_worker_status_code);
}

void SystemExtensionsServiceWorkerManager::NotifyServiceWorkerRegistered(
    const SystemExtensionId& system_extension_id,
    blink::ServiceWorkerStatusCode status_code) {
  if (status_code != blink::ServiceWorkerStatusCode::kOk) {
    LOG(ERROR) << "Failed to register Service Worker: "
               << blink::ServiceWorkerStatusToString(status_code);
  }

  for (auto& observer : observers_) {
    observer.OnRegisterServiceWorker(system_extension_id, status_code);
  }
}

void SystemExtensionsServiceWorkerManager::NotifyServiceWorkerUnregistered(
    const SystemExtensionId& system_extension_id,
    bool succeeded) {
  // TODO(b/238578914): Consider changing UnregisterServiceWorker to pass a
  // ServiceWorkerStatusCode instead of a bool.
  if (!succeeded)
    LOG(ERROR) << "Failed to unregister Service Worker.";

  for (auto& observer : observers_)
    observer.OnUnregisterServiceWorker(system_extension_id, succeeded);
}

}  // namespace ash
