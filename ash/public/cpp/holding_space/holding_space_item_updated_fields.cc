// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_item_updated_fields.h"

namespace ash {

HoldingSpaceItemUpdatedFields::HoldingSpaceItemUpdatedFields() = default;

HoldingSpaceItemUpdatedFields::~HoldingSpaceItemUpdatedFields() = default;

HoldingSpaceItemUpdatedFields::HoldingSpaceItemUpdatedFields(
    const HoldingSpaceItemUpdatedFields&) = default;

HoldingSpaceItemUpdatedFields& HoldingSpaceItemUpdatedFields::operator=(
    const HoldingSpaceItemUpdatedFields&) = default;

HoldingSpaceItemUpdatedFields::HoldingSpaceItemUpdatedFields(
    HoldingSpaceItemUpdatedFields&&) = default;

HoldingSpaceItemUpdatedFields& HoldingSpaceItemUpdatedFields::operator=(
    HoldingSpaceItemUpdatedFields&&) = default;

bool HoldingSpaceItemUpdatedFields::operator==(
    const HoldingSpaceItemUpdatedFields&) const = default;

bool HoldingSpaceItemUpdatedFields::operator!=(
    const HoldingSpaceItemUpdatedFields&) const = default;

bool HoldingSpaceItemUpdatedFields::IsEmpty() const {
  return !previous_accessible_name && !previous_backing_file &&
         !previous_in_progress_commands && !previous_progress &&
         !previous_secondary_text && !previous_secondary_text_color_variant &&
         !previous_text;
}

}  // namespace ash
