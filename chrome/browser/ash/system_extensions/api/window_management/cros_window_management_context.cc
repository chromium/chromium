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
          SystemExtensionsProvider::Get(profile).install_manager()) {}

CrosWindowManagementContext::~CrosWindowManagementContext() = default;

void CrosWindowManagementContext::Create(
    mojo::PendingAssociatedReceiver<blink::mojom::CrosWindowManagement>
        pending_receiver,
    mojo::PendingAssociatedRemote<
        blink::mojom::CrosWindowManagementStartObserver> observer_remote) {
  const content::ServiceWorkerVersionBaseInfo& info =
      factory_receivers_.current_context();

  auto cros_window_management = std::make_unique<WindowManagementImpl>(
      info.process_id, std::move(observer_remote));
  auto* cros_window_management_ptr = cros_window_management.get();

  cros_window_management_instances_.Add(std::move(cros_window_management),
                                        std::move(pending_receiver));

  auto* system_extension =
      install_manager_->GetSystemExtensionByURL(info.scope);
  if (!system_extension)
    return;

  auto [it, did_insert] =
      start_dispatched_for_extension_.insert(system_extension->id);
  // If the id is already in the set 'start' has already been dispatched for
  // the extension.
  if (!did_insert)
    return;

  cros_window_management_ptr->DispatchStartEvent();
}

}  // namespace ash
