// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_ITEM_UPDATED_FIELDS_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_ITEM_UPDATED_FIELDS_H_

#include <optional>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/holding_space/holding_space_colors.h"
#include "ash/public/cpp/holding_space/holding_space_file.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_progress.h"
#include "ui/color/color_id.h"

namespace ash {

// Indicates which fields of a holding space item were changed during an update
// operation. See `HoldingSpaceModelObserver::OnHoldingSpaceItemUpdated()`.
struct ASH_PUBLIC_EXPORT HoldingSpaceItemUpdatedFields {
  HoldingSpaceItemUpdatedFields();
  HoldingSpaceItemUpdatedFields(const HoldingSpaceItemUpdatedFields&);
  HoldingSpaceItemUpdatedFields& operator=(
      const HoldingSpaceItemUpdatedFields&);
  HoldingSpaceItemUpdatedFields(HoldingSpaceItemUpdatedFields&&);
  HoldingSpaceItemUpdatedFields& operator=(HoldingSpaceItemUpdatedFields&&);
  ~HoldingSpaceItemUpdatedFields();

  bool operator==(const HoldingSpaceItemUpdatedFields&) const;
  bool operator!=(const HoldingSpaceItemUpdatedFields&) const;

  // Returns whether any fields were updated as indicated by the presence
  // of previous values.
  bool IsEmpty() const;

  // Contains the previous accessible name iff the field was updated.
  std::optional<std::u16string> previous_accessible_name;

  // Contains the previous backing file iff the field was updated.
  std::optional<HoldingSpaceFile> previous_backing_file;

  // Contains the previous in-progress commands iff the field was updated.
  std::optional<std::vector<HoldingSpaceItem::InProgressCommand>>
      previous_in_progress_commands;

  // Contains the previous progress iff the field was updated.
  std::optional<HoldingSpaceProgress> previous_progress;

  // Contains the previous secondary text iff the field was updated.
  std::optional<std::optional<std::u16string>> previous_secondary_text;

  // Contains the previous secondary text color variant iff the field was
  // updated.
  std::optional<std::optional<HoldingSpaceColorVariant>>
      previous_secondary_text_color_variant;

  // Contains the previous text iff the field was updated.
  std::optional<std::optional<std::u16string>> previous_text;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_ITEM_UPDATED_FIELDS_H_
