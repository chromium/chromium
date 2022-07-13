// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/api/window_management/cros_window_management_context.h"

#include "base/cxx20_to_address.h"
#include "chrome/browser/ash/system_extensions/api/window_management/cros_window_management_context_factory.h"
#include "chrome/browser/ash/system_extensions/api/window_management/window_management_impl.h"
#include "chrome/browser/ash/system_extensions/system_extensions_provider.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

namespace ash {

// static
CrosWindowManagementContext& CrosWindowManagementContext::Get(
    Profile* profile) {
  return *CrosWindowManagementContextFactory::GetForProfileIfExists(profile);
}

// static
void CrosWindowManagementContext::BindFactory(
    Profile* profile,
    const content::ServiceWorkerVersionBaseInfo& info,
    mojo::PendingReceiver<blink::mojom::CrosWindowManagementFactory>
        pending_receiver) {
  // Profile could be shutting down.
  auto* dispatcher =
      CrosWindowManagementContextFactory::GetForProfileIfExists(profile);
  if (!dispatcher)
    return;

  dispatcher->factory_receivers_.Add(dispatcher, std::move(pending_receiver),
                                     info);
}

CrosWindowManagementContext::CrosWindowManagementContext(Profile* profile)
    : profile_(*profile),
      install_manager_(
          SystemExtensionsProvider::Get(profile).install_manager()) {
  install_manager_observation_.Observe(base::to_address(install_manager_));
}

CrosWindowManagementContext::~CrosWindowManagementContext() = default;

void CrosWindowManagementContext::OnServiceWorkerRegistered(
    const SystemExtensionId& system_extension_id,
    blink::ServiceWorkerStatusCode status_code) {
  auto const* system_extension =
      install_manager_->GetSystemExtensionById(system_extension_id);
  if (!system_extension) {
    LOG(ERROR) << "Tried to start service worker for non-existent extension";
    return;
  }

  const GURL& scope = system_extension->base_url;

  // TODO(b/221123297): Only dispatch `start` event for window manager
  // system extensions. This is OK for now, because we only have window
  // manager extensions.
  auto* worker_context =
      profile_->GetDefaultStoragePartition()->GetServiceWorkerContext();
  worker_context->StartWorkerForScope(
      scope, blink::StorageKey(url::Origin::Create(scope)),
      base::BindOnce(
          &CrosWindowManagementContext::DispatchWindowManagerStartEvent,
          weak_ptr_factory_.GetWeakPtr(), system_extension_id),
      base::BindOnce([](blink::ServiceWorkerStatusCode status_code) {
        LOG(ERROR) << "Failed to start service worker: "
                   << blink::ServiceWorkerStatusToString(status_code);
      }));
}

void CrosWindowManagementContext::DispatchWindowManagerStartEvent(
    const SystemExtensionId& system_extension_id,
    int64_t version_id,
    int process_id,
    int thread_id) {
  auto const* system_extension =
      install_manager_->GetSystemExtensionById(system_extension_id);
  if (!system_extension) {
    LOG(ERROR) << "Tried to dispatch event to non-existent extension.";
    return;
  }

  auto* worker_context =
      profile_->GetDefaultStoragePartition()->GetServiceWorkerContext();
  if (!worker_context->IsLiveRunningServiceWorker(version_id)) {
    LOG(ERROR) << "Started Service Worker version no longer running.";
    return;
  }

  auto& remote_interfaces = worker_context->GetRemoteInterfaces(version_id);
  mojo::Remote<blink::mojom::CrosWindowManagementStartObserver> observer;
  remote_interfaces.GetInterface(observer.BindNewPipeAndPassReceiver());
  observer->DispatchStartEvent();
}

void CrosWindowManagementContext::Create(
    mojo::PendingAssociatedReceiver<blink::mojom::CrosWindowManagement>
        pending_receiver) {
  const content::ServiceWorkerVersionBaseInfo& info =
      factory_receivers_.current_context();
  auto cros_window_management =
      std::make_unique<WindowManagementImpl>(info.process_id);
  cros_window_management_instances_.Add(std::move(cros_window_management),
                                        std::move(pending_receiver));
}

}  // namespace ash
