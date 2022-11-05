// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/accelerator_configuration.h"

#include "base/ranges/algorithm.h"

namespace ash {

AcceleratorConfiguration::AcceleratorConfiguration(
    ash::mojom::AcceleratorSource source)
    : source_(source) {}

AcceleratorConfiguration::~AcceleratorConfiguration() = default;

void AcceleratorConfiguration::AddAcceleratorsUpdatedCallback(
    AcceleratorsUpdatedCallback callback) {
  callbacks_.push_back(callback);

  // If there is a stored cache, notify event immediately.
  if (!accelerator_mapping_cache_.empty()) {
    NotifyAcceleratorsUpdated();
  }
}

void AcceleratorConfiguration::RemoveAcceleratorsUpdatedCallback(
    AcceleratorsUpdatedCallback callback) {
  const auto it = base::ranges::find(callbacks_, callback);
  if (it == callbacks_.end())
    return;

  callbacks_.erase(it);
}

void AcceleratorConfiguration::UpdateAccelerators(
    const ActionIdToAcceleratorsMap& accelerators) {
  // Update local cache everything an observable event is fired.
  accelerator_mapping_cache_ = accelerators;

  NotifyAcceleratorsUpdated();
}

void AcceleratorConfiguration::NotifyAcceleratorsUpdated() {
  for (auto& cb : callbacks_) {
    cb.Run(source_, accelerator_mapping_cache_);
  }
}

}  // namespace ash
