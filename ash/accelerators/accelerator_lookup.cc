// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_lookup.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/accelerators_util.h"
#include "base/types/optional_ref.h"

namespace ash {

namespace {

using AcceleratorDetails = AcceleratorLookup::AcceleratorDetails;

using OptionalAccelerators =
    base::optional_ref<const std::vector<ui::Accelerator>>;

}  // namespace

AcceleratorLookup::AcceleratorLookup(
    raw_ptr<AcceleratorConfiguration> ash_accelerators)
    : ash_accelerator_configuration_(ash_accelerators) {}

AcceleratorLookup::~AcceleratorLookup() = default;

std::vector<AcceleratorDetails> AcceleratorLookup::GetAcceleratorsForAction(
    uint32_t action) const {
  CHECK(ash_accelerator_configuration_);

  std::vector<AcceleratorDetails> details;
  OptionalAccelerators accelerators =
      ash_accelerator_configuration_->GetAcceleratorsForAction(action);
  if (!accelerators.has_value()) {
    return details;
  }

  for (const auto& accelerator : *accelerators) {
    details.push_back({accelerator, GetKeyDisplay(accelerator.key_code())});
  }

  return details;
}

}  // namespace ash
