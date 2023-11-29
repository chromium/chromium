// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_SUGGESTIONS_SECTION_H_
#define ASH_SYSTEM_HOLDING_SPACE_SUGGESTIONS_SECTION_H_

#include <memory>

#include "ash/system/holding_space/holding_space_item_views_section.h"
#include "ui/base/metadata/metadata_header_macros.h"

class PrefChangeRegistrar;

namespace ash {

// Section for suggestions in the `PinnedFilesBubble`.
class SuggestionsSection : public HoldingSpaceItemViewsSection {
  METADATA_HEADER(SuggestionsSection, HoldingSpaceItemViewsSection)

 public:
  explicit SuggestionsSection(HoldingSpaceViewDelegate* delegate);
  SuggestionsSection(const SuggestionsSection& other) = delete;
  SuggestionsSection& operator=(const SuggestionsSection& other) = delete;
  ~SuggestionsSection() override;

  // HoldingSpaceItemViewsSection:
  std::unique_ptr<views::View> CreateHeader() override;
  std::unique_ptr<views::View> CreateContainer() override;
  std::unique_ptr<HoldingSpaceItemView> CreateView(
      const HoldingSpaceItem* item) override;
  bool IsExpanded() override;

 private:
  // The user can expand and collapse the suggestions section by activating the
  // section header. This registrar is associated with the active user pref
  // service and notifies the section of changes to the user's preference.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_SUGGESTIONS_SECTION_H_
