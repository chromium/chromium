// Copyright 2021 The Chromium Authors. All rights reserved.
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
}

void AcceleratorConfiguration::RemoveAcceleratorsUpdatedCallback(
    AcceleratorsUpdatedCallback callback) {
  const auto it = base::ranges::find_if(
      callbacks_, [callback](const auto& o) { return o == callback; });
  if (it == callbacks_.end())
    return;

  callbacks_.erase(it);
}

void AcceleratorConfiguration::NotifyAcceleratorsUpdated(
    const std::multimap<AcceleratorAction, AcceleratorInfo>& accelerators) {
  for (auto& cb : callbacks_) {
    cb.Run(source_, accelerators);
  }
}
}  // namespace ash
