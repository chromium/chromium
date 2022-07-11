// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/api/window_management/cros_window_management_context.h"

#include "chrome/browser/ash/system_extensions/api/window_management/cros_window_management_context_factory.h"
#include "chrome/browser/ash/system_extensions/api/window_management/window_management_impl.h"

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

CrosWindowManagementContext::CrosWindowManagementContext() = default;

CrosWindowManagementContext::~CrosWindowManagementContext() = default;

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
