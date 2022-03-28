// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/pinned_files_section.h"

#include "ash/bubble/bubble_utils.h"
#include "ash/bubble/simple_grid_layout.h"
#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/style_util.h"
#include "ash/system/holding_space/holding_space_item_chip_view.h"
#include "ash/system/holding_space/holding_space_view_delegate.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kFilesAppChipChildSpacing = 8;
constexpr int kFilesAppChipHeight = 32;
constexpr int kFilesAppChipIconSize = 20;
constexpr auto kFilesAppChipInsets = gfx::Insets::TLBR(0, 8, 0, 16);
constexpr int kPlaceholderChildSpacing = 16;

// FilesAppChip ----------------------------------------------------------------

class FilesAppChip : public views::Button {
 public:
  explicit FilesAppChip(views::Button::PressedCallback pressed_callback)
      : views::Button(std::move(pressed_callback)) {
    Init();
  }

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

  void OnThemeChanged() override {
    views::Button::OnThemeChanged();
    AshColorProvider* const ash_color_provider = AshColorProvider::Get();

    // Background.
    SetBackground(views::CreateRoundedRectBackground(
        ash_color_provider->GetControlsLayerColor(
            AshColorProvider::ControlsLayerType::
                kControlBackgroundColorInactive),
        kFilesAppChipHeight / 2));

    // Focus ring.
    views::FocusRing::Get(this)->SetColor(
        ash_color_provider->GetControlsLayerColor(
            AshColorProvider::ControlsLayerType::kFocusRingColor));

    // Ink drop.
    StyleUtil::ConfigureInkDropAttributes(
        this, StyleUtil::kBaseColor | StyleUtil::kInkDropOpacity |
                  StyleUtil::kHighlightOpacity);
  }

  void Init() {
    SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_ASH_HOLDING_SPACE_PINNED_FILES_APP_CHIP_TEXT));
    SetID(kHoldingSpaceFilesAppChipId);

    // Ink drop.
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
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
    icon->SetImage(gfx::CreateVectorIcon(kFilesAppIcon, kFilesAppChipIconSize,
                                         gfx::kPlaceholderColor));

    // Label.
    auto* label = AddChildView(
        bubble_utils::CreateLabel(bubble_utils::LabelStyle::kChipTitle));
    label->SetText(l10n_util::GetStringUTF16(
        IDS_ASH_HOLDING_SPACE_PINNED_FILES_APP_CHIP_TEXT));
    layout->SetFlexForView(label, 1);
  }
};

}  // namespace

// PinnedFilesSection ----------------------------------------------------------

PinnedFilesSection::PinnedFilesSection(HoldingSpaceViewDelegate* delegate)
    : HoldingSpaceItemViewsSection(delegate,
                                   /*supported_types=*/
                                   {HoldingSpaceItem::Type::kPinnedFile},
                                   /*max_count=*/absl::nullopt) {}

PinnedFilesSection::~PinnedFilesSection() = default;

// static
bool PinnedFilesSection::ShouldShowPlaceholder(PrefService* prefs) {
  // The placeholder should only be shown if:
  // * a holding space item has been added at some point in time,
  // * a holding space item has *never* been pinned, and
  // * the user has never pressed the Files app chip in the placeholder.
  return holding_space_prefs::GetTimeOfFirstAdd(prefs) &&
         !holding_space_prefs::GetTimeOfFirstPin(prefs) &&
         !holding_space_prefs::GetTimeOfFirstFilesAppChipPress(prefs);
}

const char* PinnedFilesSection::GetClassName() const {
  return "PinnedFilesSection";
}

gfx::Size PinnedFilesSection::GetMinimumSize() const {
  // The pinned files section is scrollable so can be laid out smaller than its
  // preferred size if there is insufficient layout space available.
  return gfx::Size();
}

std::unique_ptr<views::View> PinnedFilesSection::CreateHeader() {
  auto header = bubble_utils::CreateLabel(
      bubble_utils::LabelStyle::kHeader,
      l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_PINNED_TITLE));
  header->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  header->SetPaintToLayer();
  header->layer()->SetFillsBoundsOpaquely(false);
  return header;
}

std::unique_ptr<views::View> PinnedFilesSection::CreateContainer() {
  auto container = std::make_unique<views::View>();
  container->SetLayoutManager(std::make_unique<SimpleGridLayout>(
      kHoldingSpaceChipCountPerRow,
      /*column_spacing=*/kHoldingSpaceSectionContainerChildSpacing,
      /*row_spacing=*/kHoldingSpaceSectionContainerChildSpacing));
  return container;
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
  auto* prefs = Shell::Get()->session_controller()->GetActivePrefService();
  if (!PinnedFilesSection::ShouldShowPlaceholder(prefs))
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
  auto* prompt = placeholder->AddChildView(bubble_utils::CreateLabel(
      bubble_utils::LabelStyle::kBody,
      l10n_util::GetStringUTF16(IDS_ASH_HOLDING_SPACE_PINNED_EMPTY_PROMPT)));
  prompt->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  prompt->SetMultiLine(true);

  // Files app chip.
  placeholder->AddChildView(std::make_unique<FilesAppChip>(base::BindRepeating(
      &PinnedFilesSection::OnFilesAppChipPressed, base::Unretained(this))));

  return placeholder;
}

void PinnedFilesSection::OnFilesAppChipPressed(const ui::Event& event) {
  holding_space_metrics::RecordFilesAppChipAction(
      holding_space_metrics::FilesAppChipAction::kClick);

  // NOTE: This no-ops if the Files app chip was previously pressed.
  holding_space_prefs::MarkTimeOfFirstFilesAppChipPress(
      Shell::Get()->session_controller()->GetActivePrefService());

  HoldingSpaceController::Get()->client()->OpenMyFiles(base::DoNothing());

  // Once the user has pressed the Files app chip, the placeholder should no
  // longer be displayed. This is accomplished by destroying it. If the holding
  // space model is empty, the holding space tray will also need to update its
  // visibility to become hidden.
  DestroyPlaceholder();
  delegate()->UpdateTrayVisibility();
}

}  // namespace ash
