// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_crx_component.h"

#include <memory>

#include "components/update_client/crx_update_item.h"

namespace on_device_ai {

namespace {

bool IsDownloadEvent(const component_updater::CrxUpdateItem& item) {
  // See class comment: components/update_client/component.h
  switch (item.state) {
    case update_client::ComponentState::kDownloading:
    case update_client::ComponentState::kDecompressing:
    case update_client::ComponentState::kPatching:
    case update_client::ComponentState::kUpdating:
    case update_client::ComponentState::kUpToDate:
      return item.downloaded_bytes >= 0 && item.total_bytes >= 0;
    case update_client::ComponentState::kNew:
    case update_client::ComponentState::kChecking:
    case update_client::ComponentState::kCanUpdate:
    case update_client::ComponentState::kUpdated:
    case update_client::ComponentState::kUpdateError:
    case update_client::ComponentState::kRun:
      return false;
  }
}

bool IsAlreadyInstalled(const component_updater::CrxUpdateItem& item) {
  // See class comment: components/update_client/component.h
  switch (item.state) {
    case update_client::ComponentState::kUpdated:
    case update_client::ComponentState::kUpToDate:
      return true;
    case update_client::ComponentState::kNew:
    case update_client::ComponentState::kChecking:
    case update_client::ComponentState::kCanUpdate:
    case update_client::ComponentState::kDownloading:
    case update_client::ComponentState::kDecompressing:
    case update_client::ComponentState::kPatching:
    case update_client::ComponentState::kUpdating:
    case update_client::ComponentState::kUpdateError:
    case update_client::ComponentState::kRun:
      return false;
  }
}

}  // namespace

// static
base::flat_set<std::unique_ptr<AIModelDownloadProgressManager::Component>>
AICrxComponent::FromComponentIds(
    component_updater::ComponentUpdateService* component_update_service,
    base::flat_set<std::string> component_ids) {
  base::flat_set<std::unique_ptr<Component>> components;
  components.reserve(component_ids.size());

  for (std::string component_id : component_ids) {
    components.emplace(std::make_unique<AICrxComponent>(
        component_update_service, std::move(component_id)));
  }

  return components;
}

AICrxComponent::AICrxComponent(
    component_updater::ComponentUpdateService* component_update_service,
    std::string component_id)
    : component_id_(std::move(component_id)) {
  component_updater::CrxUpdateItem item;
  bool success =
      component_update_service->GetComponentDetails(component_id_, &item);

  // When `success` returns false, it means the component hasn't
  // been registered yet. `GetComponentDetails` doesn't fill out `item` in this
  // case, and we can just treat the component as if it had a state of `kNew`.
  if (success && IsAlreadyInstalled(item)) {
    // We just need to set the downloaded bytes and total bytes equal to each
    // other to indicate to the `AIModelDownloadProgressManager` that we're
    // installed.
    //
    // We don't set it to `item`'s `downloaded_bytes` and `total_bytes` because
    // they may be less than zero which `AIModelDownloadProgressManager` doesn't
    // allow.
    SetDownloadedBytes(0);
    SetTotalBytes(0);
    return;
  }

  // Watch for progress updates.
  component_updater_observation_.Observe(component_update_service);
}

AICrxComponent::~AICrxComponent() = default;

// component_updater::ServiceObserver:
void AICrxComponent::OnEvent(const component_updater::CrxUpdateItem& item) {
  if (!IsDownloadEvent(item) || item.id != component_id_) {
    return;
  }

  // Crx components may send downloaded_bytes that exceed the total_bytes.
  SetDownloadedBytes(std::min(item.downloaded_bytes, item.total_bytes));
  SetTotalBytes(item.total_bytes);
}

}  // namespace on_device_ai
