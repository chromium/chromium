// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/common/backend/accelerator_fetcher.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accelerators/accelerator_lookup.h"
#include "ash/accelerators/ash_accelerator_configuration.h"
#include "ash/public/cpp/accelerator_actions.h"
#include "ash/public/mojom/accelerator_actions.mojom.h"
#include "ash/public/mojom/accelerator_info.mojom.h"
#include "ash/shell.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/ash/keyboard_capability.h"

namespace ash {

namespace {

std::vector<mojom::StandardAcceleratorPropertiesPtr> GetAcceleratorsForActionId(
    AcceleratorAction action) {
  CHECK(Shell::HasInstance());
  const std::vector<AcceleratorLookup::AcceleratorDetails>&
      available_accelerators =
          Shell::Get()->accelerator_lookup()->GetAvailableAcceleratorsForAction(
              action);

  std::vector<mojom::StandardAcceleratorPropertiesPtr> accelerator_properties;
  accelerator_properties.reserve(available_accelerators.size());
  for (const auto& available_accelerator : available_accelerators) {
    accelerator_properties.push_back(mojom::StandardAcceleratorProperties::New(
        available_accelerator.accelerator, available_accelerator.key_display,
        /*original_accelerator=*/std::nullopt));
  }

  return accelerator_properties;
}

}  // namespace

AcceleratorFetcher::AcceleratorFetcher() {
  if (!::features::IsShortcutCustomizationEnabled()) {
    return;
  }

  if (Shell::HasInstance()) {
    Shell::Get()
        ->accelerator_controller()
        ->accelerator_configuration()
        ->AddObserver(this);
  }
  accelerator_observers_.set_disconnect_handler(
      base::BindRepeating(&AcceleratorFetcher::OnObserverDisconnect,
                          weak_ptr_factory_.GetWeakPtr()));
}

AcceleratorFetcher::~AcceleratorFetcher() {
  if (!::features::IsShortcutCustomizationEnabled()) {
    return;
  }

  if (Shell::HasInstance()) {
    Shell::Get()
        ->accelerator_controller()
        ->accelerator_configuration()
        ->RemoveObserver(this);
  }
}

void AcceleratorFetcher::BindInterface(
    mojo::PendingReceiver<common::mojom::AcceleratorFetcher> receiver) {
  CHECK(::features::IsShortcutCustomizationEnabled());
  if (accelerator_fetcher_receiver_.is_bound()) {
    accelerator_fetcher_receiver_.reset();
  }
  accelerator_fetcher_receiver_.Bind(std::move(receiver));
}

void AcceleratorFetcher::ObserveAcceleratorChanges(
    const std::vector<AcceleratorAction>& action_ids,
    mojo::PendingRemote<common::mojom::AcceleratorFetcherObserver> observer) {
  mojo::RemoteSetElementId id = accelerator_observers_.Add(std::move(observer));
  actions_for_receivers_[id] = action_ids;

  common::mojom::AcceleratorFetcherObserver* receiver =
      accelerator_observers_.Get(id);
  // Notify the observer immediately after adding it.
  for (const AcceleratorAction& action_id : action_ids) {
    receiver->OnAcceleratorsUpdated(action_id,
                                    GetAcceleratorsForActionId(action_id));
  }
}

void AcceleratorFetcher::OnAcceleratorsUpdated() {
  for (const auto& [receiver_id, action_ids] : actions_for_receivers_) {
    for (const auto& action_id : action_ids) {
      accelerator_observers_.Get(receiver_id)
          ->OnAcceleratorsUpdated(action_id,
                                  GetAcceleratorsForActionId(action_id));
    }
  }
}

void AcceleratorFetcher::GetMetaKeyToDisplay(
    GetMetaKeyToDisplayCallback callback) {
  std::move(callback).Run(
      Shell::Get()->keyboard_capability()->GetMetaKeyToDisplay());
}

void AcceleratorFetcher::OnObserverDisconnect(mojo::RemoteSetElementId id) {
  actions_for_receivers_.erase(id);
}

void AcceleratorFetcher::FlushMojoForTesting() {
  accelerator_observers_.FlushForTesting();  // IN-TEST
}

}  // namespace ash
