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
      system_extensions_registry_(
          SystemExtensionsProvider::Get(profile).registry()),
      system_extensions_service_worker_manager_(
          SystemExtensionsProvider::Get(profile).service_worker_manager()) {
  cros_window_management_instances_.set_disconnect_handler(base::BindRepeating(
      &CrosWindowManagementContext::OnCrosWindowManagementDisconnect,
      // Safe because disconnect handlers aren't dispatched
      // after this class is destroyed.
      base::Unretained(this)));

  service_worker_manager_observation_.Observe(
      base::to_address(system_extensions_service_worker_manager_));
}

CrosWindowManagementContext::~CrosWindowManagementContext() = default;

void CrosWindowManagementContext::OnCrosWindowManagementDisconnect() {
  const SystemExtensionsServiceWorkerInfo& info =
      cros_window_management_instances_.current_context();

  bool info_removed = service_worker_info_to_impl_map_.erase(info);
  DCHECK(info_removed);

  // No need to remove it from cros_window_management_instances_ because
  // the ReceiverSet takes care of it.
}

void CrosWindowManagementContext::Create(
    mojo::PendingAssociatedReceiver<blink::mojom::CrosWindowManagement>
        pending_receiver,
    mojo::PendingAssociatedRemote<
        blink::mojom::CrosWindowManagementStartObserver> observer_remote) {
  const content::ServiceWorkerVersionBaseInfo& service_worker_version_info =
      factory_receivers_.current_context();

  // TODO(b/242264794): Change this to a DCHECK once we stop running tests
  // without an installed System Extension.
  const auto* system_extension =
      system_extensions_registry_->GetByUrl(service_worker_version_info.scope);

  // TODO(b/242264794): Remove the ternary operator once we stop running tests
  // without an installed System Extension.
  SystemExtensionsServiceWorkerInfo service_worker_info{
      .system_extension_id =
          system_extension ? system_extension->id : SystemExtensionId(),
      .service_worker_version_id = service_worker_version_info.version_id,
      .service_worker_process_id = service_worker_version_info.process_id};

  auto cros_window_management = std::make_unique<WindowManagementImpl>(
      service_worker_info.service_worker_process_id,
      std::move(observer_remote));
  auto* cros_window_management_ptr = cros_window_management.get();

  cros_window_management_instances_.Add(std::move(cros_window_management),
                                        std::move(pending_receiver),
                                        service_worker_info);

  auto [_, inserted] = service_worker_info_to_impl_map_.emplace(
      service_worker_info, cros_window_management_ptr);
  DCHECK(inserted);

  // TODO(b/242264794): Remove once we stop running tests without an
  // installed System Extension.
  if (system_extension)
    RunPendingTasks(system_extension->id, *cros_window_management_ptr);
}

void CrosWindowManagementContext::OnRegisterServiceWorker(
    const SystemExtensionId& system_extension_id,
    blink::ServiceWorkerStatusCode status_code) {
  if (status_code != blink::ServiceWorkerStatusCode::kOk)
    return;

  GetCrosWindowManagement(
      system_extension_id,
      base::BindOnce([](WindowManagementImpl& cros_window_management) {
        cros_window_management.DispatchStartEvent();
      }));
}

void CrosWindowManagementContext::GetCrosWindowManagement(
    const SystemExtensionId& system_extension_id,
    base::OnceCallback<void(WindowManagementImpl&)> callback) {
  auto& pending_callbacks = id_to_pending_callbacks_[system_extension_id];

  const bool need_to_start_worker = pending_callbacks.empty();
  pending_callbacks.push_back(std::move(callback));

  if (!need_to_start_worker)
    return;

  system_extensions_service_worker_manager_->StartServiceWorker(
      system_extension_id,
      base::BindOnce(&CrosWindowManagementContext::OnServiceWorkerStarted,
                     weak_ptr_factory_.GetWeakPtr(), system_extension_id));
}

void CrosWindowManagementContext::OnServiceWorkerStarted(
    const SystemExtensionId& system_extension_id,
    StatusOrSystemExtensionsServiceWorkerInfo status_or_info) {
  if (!status_or_info.ok()) {
    id_to_pending_callbacks_.erase(system_extension_id);
    return;
  }

  auto info_and_impl_it =
      service_worker_info_to_impl_map_.find(status_or_info.value());
  if (info_and_impl_it == service_worker_info_to_impl_map_.end())
    return;

  RunPendingTasks(system_extension_id, *info_and_impl_it->second);
}

void CrosWindowManagementContext::RunPendingTasks(
    const SystemExtensionId& system_extension_id,
    WindowManagementImpl& window_management_impl) {
  std::vector<base::OnceCallback<void(WindowManagementImpl&)>> callbacks;

  std::swap(callbacks, id_to_pending_callbacks_[system_extension_id]);
  for (auto& callback : callbacks) {
    std::move(callback).Run(window_management_impl);
  }
}

}  // namespace ash
