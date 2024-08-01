// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_SECTION_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_SECTION_H_

#include <set>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"

namespace ash {

// Enumeration of unique identifiers for a holding space section.
enum class HoldingSpaceSectionId {
  kMinValue = 0,
  kDownloads = kMinValue,
  kPinnedFiles,
  kScreenCaptures,
  kSuggestions,
  kMaxValue = kSuggestions,
};

// Model for a holding space section.
struct ASH_PUBLIC_EXPORT HoldingSpaceSection {
  HoldingSpaceSection(HoldingSpaceSectionId id,
                      std::set<HoldingSpaceItem::Type> supported_types,
                      std::optional<size_t> max_visible_item_count);

  HoldingSpaceSection(const HoldingSpaceSection&) = delete;
  HoldingSpaceSection& operator=(const HoldingSpaceSection&) = delete;
  ~HoldingSpaceSection();

  // Unique identifier for the section.
  const HoldingSpaceSectionId id;

  // Types of holding space items to be rendered in the section.
  const std::set<HoldingSpaceItem::Type> supported_types;

  // Maximum count of items to be visible at once for the section in holding
  // space UI. If absent, no maximum count is enforced.
  const std::optional<size_t> max_visible_item_count;
};

// Returns the section to which the specified `type` belongs.
ASH_PUBLIC_EXPORT const HoldingSpaceSection* GetHoldingSpaceSection(
    HoldingSpaceItem::Type type);

// Returns the section uniquely identified by the specified `id`.
ASH_PUBLIC_EXPORT const HoldingSpaceSection* GetHoldingSpaceSection(
    HoldingSpaceSectionId id);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_SECTION_H_
