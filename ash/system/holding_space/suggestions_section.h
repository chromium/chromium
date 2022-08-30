// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_SUGGESTIONS_SECTION_H_
#define ASH_SYSTEM_HOLDING_SPACE_SUGGESTIONS_SECTION_H_

#include <memory>

#include "ash/system/holding_space/holding_space_item_views_section.h"

namespace ash {

// Section for suggestions in the `PinnedFilesBubble`.
class SuggestionsSection : public HoldingSpaceItemViewsSection {
 public:
  explicit SuggestionsSection(HoldingSpaceViewDelegate* delegate);
  SuggestionsSection(const SuggestionsSection& other) = delete;
  SuggestionsSection& operator=(const SuggestionsSection& other) = delete;
  ~SuggestionsSection() override;

  // HoldingSpaceItemViewsSection:
  const char* GetClassName() const override;
  std::unique_ptr<views::View> CreateHeader() override;
  std::unique_ptr<views::View> CreateContainer() override;
  std::unique_ptr<HoldingSpaceItemView> CreateView(
      const HoldingSpaceItem* item) override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_SUGGESTIONS_SECTION_H_
