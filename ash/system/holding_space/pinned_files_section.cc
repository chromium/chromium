// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/pinned_files_section.h"

#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
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
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kFilesAppChipChildSpacing = 16;
constexpr int kFilesAppChipHeight = 32;
constexpr int kFilesAppChipIconHeight = 16;
constexpr gfx::Insets kFilesAppChipInsets(0, 8, 0, 16);
constexpr int kPlaceholderChildSpacing = 16;

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

// FilesAppChip ----------------------------------------------------------------

class FilesAppChip : public views::Button {
 public:
  FilesAppChip() { Init(); }
  FilesAppChip(const FilesAppChip&) = delete;
  FilesAppChip& operator=(const FilesAppChip&) = delete;
  ~FilesAppChip() override = default;

 private:
  // views::Button:
  gfx::Size CalculatePreferredSize() const override {
    const int width = views::Button::CalculatePreferredSize().width();
    return gfx::Size(width, GetHeightForWidth(width));
  }

  int GetHeightForWidth(int width) const override {
    return kFilesAppChipHeight;
  }

  void Init() {
    SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_ASH_HOLDING_SPACE_PINNED_FILES_APP_CHIP_TEXT));
    SetCallback(
        base::BindRepeating(&FilesAppChip::OnPressed, base::Unretained(this)));
    SetID(kHoldingSpaceFilesAppChipId);

    // Background.
    AshColorProvider* const ash_color_provider = AshColorProvider::Get();
    SetBackground(views::CreateRoundedRectBackground(
        ash_color_provider->GetControlsLayerColor(
            AshColorProvider::ControlsLayerType::
                kControlBackgroundColorInactive),
        kFilesAppChipHeight / 2));

    // Focus ring.
    focus_ring()->SetColor(ash_color_provider->GetControlsLayerColor(
        AshColorProvider::ControlsLayerType::kFocusRingColor));

    // Ink drop.
    const AshColorProvider::RippleAttributes ripple_attributes =
        ash_color_provider->GetRippleAttributes();
    SetInkDropMode(InkDropMode::ON);
    SetInkDropBaseColor(ripple_attributes.base_color);
    SetInkDropVisibleOpacity(ripple_attributes.inkdrop_opacity);
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                  kFilesAppChipHeight / 2);

    // Layout.
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, kFilesAppChipInsets,
        kFilesAppChipChildSpacing));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    // Icon.
    auto* icon = AddChildView(std::make_unique<views::ImageView>());
    icon->SetBackground(views::CreateRoundedRectBackground(
        ash_color_provider->GetControlsLayerColor(
            AshColorProvider::ControlsLayerType::
                kControlBackgroundColorInactive),
        kFilesAppChipIconHeight / 2));
    icon->SetPreferredSize(
        gfx::Size(kFilesAppChipIconHeight, kFilesAppChipIconHeight));

    // Label.
    auto* label = AddChildView(std::make_unique<views::Label>());
    label->SetEnabledColor(ash_color_provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));
    label->SetText(l10n_util::GetStringUTF16(
        IDS_ASH_HOLDING_SPACE_PINNED_FILES_APP_CHIP_TEXT));
    layout->SetFlexForView(label, 1);

    TrayPopupUtils::SetLabelFontList(
        label, TrayPopupUtils::FontStyle::kDetailedViewLabel);
  }

  void OnPressed(const ui::Event& event) {
    HoldingSpaceController::Get()->client()->OpenMyFiles(base::DoNothing());
  }
};

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

  auto placeholder = std::make_unique<views::View>();
  placeholder->SetPaintToLayer();
  placeholder->layer()->SetFillsBoundsOpaquely(false);

  auto* layout =
      placeholder->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(),
          kPlaceholderChildSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  // Prompt.
  auto* prompt = placeholder->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_PINNED_EMPTY_PROMPT)));
  prompt->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  prompt->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  prompt->SetMultiLine(true);

  TrayPopupUtils::SetLabelFontList(
      prompt, TrayPopupUtils::FontStyle::kDetailedViewLabel);

  // Files app chip.
  placeholder->AddChildView(std::make_unique<FilesAppChip>());

  return placeholder;
}

}  // namespace ash
