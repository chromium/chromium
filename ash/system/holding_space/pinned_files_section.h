// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_PINNED_FILES_SECTION_H_
#define ASH_SYSTEM_HOLDING_SPACE_PINNED_FILES_SECTION_H_

#include <memory>

#include "ash/system/holding_space/holding_space_item_views_section.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

// Section for pinned files in the `PinnedFilesBubble`.
class PinnedFilesSection : public HoldingSpaceItemViewsSection {
  METADATA_HEADER(PinnedFilesSection, HoldingSpaceItemViewsSection)

 public:
  explicit PinnedFilesSection(HoldingSpaceViewDelegate* delegate);
  PinnedFilesSection(const PinnedFilesSection& other) = delete;
  PinnedFilesSection& operator=(const PinnedFilesSection& other) = delete;
  ~PinnedFilesSection() override;

 private:
  // HoldingSpaceItemViewsSection:
  gfx::Size GetMinimumSize() const override;
  std::unique_ptr<views::View> CreateHeader() override;
  std::unique_ptr<views::View> CreateContainer() override;
  std::unique_ptr<HoldingSpaceItemView> CreateView(
      const HoldingSpaceItem* item) override;
  std::unique_ptr<views::View> CreatePlaceholder() override;

  // Invoked when the Files app chip in the placeholder is pressed.
  void OnFilesAppChipPressed(const ui::Event& event);
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_PINNED_FILES_SECTION_H_
