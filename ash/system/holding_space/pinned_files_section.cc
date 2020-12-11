// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/pinned_files_section.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/holding_space/holding_space_item_chip_view.h"
#include "ash/system/holding_space/holding_space_item_chips_container.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

// Helpers ---------------------------------------------------------------------

// Returns whether the active user has ever pinned a file to holding space.
bool HasEverPinnedHoldingSpaceItem() {
  PrefService* active_pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  return active_pref_service
             ? holding_space_prefs::GetTimeOfFirstPin(active_pref_service)
                   .has_value()
             : false;
}

}  // namespace

// PinnedFilesSection ----------------------------------------------------------

PinnedFilesSection::PinnedFilesSection(HoldingSpaceItemViewDelegate* delegate)
    : HoldingSpaceItemViewsSection(delegate,
                                   /*supported_types=*/
                                   {HoldingSpaceItem::Type::kPinnedFile},
                                   /*max_count=*/base::nullopt) {}

PinnedFilesSection::~PinnedFilesSection() = default;

const char* PinnedFilesSection::GetClassName() const {
  return "PinnedFilesSection";
}

std::unique_ptr<views::View> PinnedFilesSection::CreateHeader() {
  auto header = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_PINNED_TITLE));
  header->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  header->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  header->SetPaintToLayer();
  header->layer()->SetFillsBoundsOpaquely(false);

  TrayPopupUtils::SetLabelFontList(header.get(),
                                   TrayPopupUtils::FontStyle::kSubHeader);

  return header;
}

std::unique_ptr<views::View> PinnedFilesSection::CreateContainer() {
  return std::make_unique<HoldingSpaceItemChipsContainer>();
}

std::unique_ptr<HoldingSpaceItemView> PinnedFilesSection::CreateView(
    const HoldingSpaceItem* item) {
  // When `PinnedFilesSection::CreateView()` is called it implies that the user
  // has at some point in time pinned a file to holding space. That being the
  // case, the placeholder is no longer relevant and can be destroyed.
  DestroyPlaceholder();
  return std::make_unique<HoldingSpaceItemChipView>(delegate(), item);
}

std::unique_ptr<views::View> PinnedFilesSection::CreatePlaceholder() {
  if (HasEverPinnedHoldingSpaceItem())
    return nullptr;

  auto placeholder = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_PINNED_EMPTY_PROMPT));
  placeholder->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  placeholder->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  placeholder->SetMultiLine(true);
  placeholder->SetPaintToLayer();
  placeholder->layer()->SetFillsBoundsOpaquely(false);

  TrayPopupUtils::SetLabelFontList(
      placeholder.get(), TrayPopupUtils::FontStyle::kDetailedViewLabel);

  return placeholder;
}

}  // namespace ash
