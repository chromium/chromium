// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/pinned_files_section.h"

#include "ash/bubble/bubble_utils.h"
#include "ash/bubble/simple_grid_layout.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
#include "ash/public/cpp/holding_space/holding_space_section.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/style_util.h"
#include "ash/style/typography.h"
#include "ash/system/holding_space/holding_space_item_chip_view.h"
#include "ash/system/holding_space/holding_space_ui.h"
#include "ash/system/holding_space/holding_space_view_delegate.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "build/branding_buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/color_palette.h"
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
#include "ui/views/layout/box_layout_view.h"

namespace ash {

namespace {

// Appearance.
constexpr int kFilesAppChipChildSpacing = 8;
constexpr int kFilesAppChipHeight = 32;
constexpr int kFilesAppChipIconSize = 20;
constexpr auto kFilesAppChipInsets = gfx::Insets::TLBR(0, 8, 0, 16);
constexpr int kPlaceholderChildSpacing = 16;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr int kPlaceholderGSuiteIconSize = 20;
constexpr int kPlaceholderGSuiteIconSpacing = 8;

// Create a builder for an image view for the given G Suite icon.
views::Builder<views::ImageView> CreateGSuiteIcon(const gfx::VectorIcon& icon) {
  return views::Builder<views::ImageView>().SetImage(gfx::CreateVectorIcon(
      icon, kPlaceholderGSuiteIconSize, gfx::kPlaceholderColor));
}
#endif

// Returns true if the given pref service or currently active features are in a
// state where the placeholder should be shown in the pinned files section.
bool ShouldShowPlaceholder(PrefService* prefs) {
  if (features::IsHoldingSpaceSuggestionsEnabled()) {
    return true;
  }

  // The placeholder should only be shown if:
  // * a holding space item has been added at some point in time,
  // * a holding space item has *never* been pinned, and
  // * the user has never pressed the Files app chip in the placeholder.
  return prefs && holding_space_prefs::GetTimeOfFirstAdd(prefs) &&
         !holding_space_prefs::GetTimeOfFirstPin(prefs) &&
         !holding_space_prefs::GetTimeOfFirstFilesAppChipPress(prefs);
}

// Returns placeholder text given whether or not Google Drive is disabled.
std::u16string GetPlaceholderText(bool drive_disabled) {
  int message_id = IDS_ASH_HOLDING_SPACE_PINNED_EMPTY_PROMPT;
  if (features::IsHoldingSpaceSuggestionsEnabled()) {
    message_id =
        drive_disabled
            ? IDS_ASH_HOLDING_SPACE_PINNED_EMPTY_PROMPT_SUGGESTIONS_DRIVE_DISABLED
            : IDS_ASH_HOLDING_SPACE_PINNED_EMPTY_PROMPT_SUGGESTIONS;
  }
  return l10n_util::GetStringUTF16(message_id);
}

// FilesAppChip ----------------------------------------------------------------

class FilesAppChip : public views::Button {
  METADATA_HEADER(FilesAppChip, views::Button)

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
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    const int width =
        views::Button::CalculatePreferredSize(available_size).width();
    return gfx::Size(width, kFilesAppChipHeight);
  }

  void OnThemeChanged() override {
    views::Button::OnThemeChanged();

    // Ink drop.
    StyleUtil::ConfigureInkDropAttributes(
        this, StyleUtil::kBaseColor | StyleUtil::kInkDropOpacity |
                  StyleUtil::kHighlightOpacity);
  }

  void Init() {
    GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
        IDS_ASH_HOLDING_SPACE_PINNED_FILES_APP_CHIP_TEXT));
    SetID(kHoldingSpaceFilesAppChipId);

    // Ink drop.
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                  kFilesAppChipHeight / 2.f);

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
    auto* label =
        AddChildView(bubble_utils::CreateLabel(TypographyToken::kCrosBody2));
    label->SetText(l10n_util::GetStringUTF16(
        IDS_ASH_HOLDING_SPACE_PINNED_FILES_APP_CHIP_TEXT));
    layout->SetFlexForView(label, 1);

    // Focus ring.
    views::FocusRing::Get(this)->SetColorId(ui::kColorAshFocusRing);

    // Background.
    SetBackground(views::CreateThemedRoundedRectBackground(
        kColorAshControlBackgroundColorInactive, kFilesAppChipHeight / 2.f));
  }
};

BEGIN_METADATA(FilesAppChip)
END_METADATA

}  // namespace

// PinnedFilesSection ----------------------------------------------------------

PinnedFilesSection::PinnedFilesSection(HoldingSpaceViewDelegate* delegate)
    : HoldingSpaceItemViewsSection(delegate,
                                   HoldingSpaceSectionId::kPinnedFiles) {
  SetID(kHoldingSpacePinnedFilesSectionId);
}

PinnedFilesSection::~PinnedFilesSection() = default;

gfx::Size PinnedFilesSection::GetMinimumSize() const {
  // The pinned files section is scrollable so can be laid out smaller than its
  // preferred size if there is insufficient layout space available.
  return gfx::Size();
}

std::unique_ptr<views::View> PinnedFilesSection::CreateHeader() {
  auto header =
      holding_space_ui::CreateSectionHeaderLabel(
          IDS_ASH_HOLDING_SPACE_PINNED_TITLE)
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
          .SetPaintToLayer()
          .Build();
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
  if (!features::IsHoldingSpaceSuggestionsEnabled()) {
    // When `PinnedFilesSection::CreateView()` is called it implies that the
    // user has at some point in time pinned a file to holding space. That being
    // the case, the placeholder is no longer relevant and can be destroyed.
    DestroyPlaceholder();
  }
  return std::make_unique<HoldingSpaceItemChipView>(delegate(), item);
}

std::unique_ptr<views::View> PinnedFilesSection::CreatePlaceholder() {
  auto* prefs = Shell::Get()->session_controller()->GetActivePrefService();
  if (!ShouldShowPlaceholder(prefs))
    return nullptr;

  bool drive_disabled =
      HoldingSpaceController::Get()->client()->IsDriveDisabled();

  auto placeholder_builder =
      views::Builder<views::BoxLayoutView>()
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart)
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetBetweenChildSpacing(kPlaceholderChildSpacing)
          .AddChild(
              holding_space_ui::CreateSectionPlaceholderLabel(
                  GetPlaceholderText(drive_disabled))
                  .SetID(kHoldingSpacePinnedFilesSectionPlaceholderLabelId)
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetMultiLine(true));

  if (features::IsHoldingSpaceSuggestionsEnabled()) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    // G Suite icons.
    auto icons_builder =
        views::Builder<views::BoxLayoutView>()
            .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
            .SetBetweenChildSpacing(kPlaceholderGSuiteIconSpacing)
            .SetID(kHoldingSpacePinnedFilesSectionPlaceholderGSuiteIconsId);

    if (!drive_disabled)
      icons_builder.AddChild(CreateGSuiteIcon(vector_icons::kGoogleDriveIcon));

    icons_builder.AddChild(CreateGSuiteIcon(vector_icons::kGoogleSlidesIcon))
        .AddChild(CreateGSuiteIcon(vector_icons::kGoogleDocsIcon))
        .AddChild(CreateGSuiteIcon(vector_icons::kGoogleSheetsIcon));
    placeholder_builder.AddChild(std::move(icons_builder));
#endif
  } else {
    // Files app chip.
    placeholder_builder.AddChild(
        views::Builder<views::Button>(std::make_unique<FilesAppChip>(
            base::BindRepeating(&PinnedFilesSection::OnFilesAppChipPressed,
                                base::Unretained(this)))));
  }

  return std::move(placeholder_builder).Build();
}

void PinnedFilesSection::OnFilesAppChipPressed(const ui::Event& event) {
  holding_space_metrics::RecordFilesAppChipAction(
      holding_space_metrics::FilesAppChipAction::kClick);

  // NOTE: This no-ops if the Files app chip was previously pressed.
  holding_space_prefs::MarkTimeOfFirstFilesAppChipPress(
      Shell::Get()->session_controller()->GetActivePrefService());

  HoldingSpaceController::Get()->client()->OpenMyFiles(base::DoNothing());

  if (!features::IsHoldingSpaceSuggestionsEnabled()) {
    // Once the user has pressed the Files app chip, the placeholder should no
    // longer be displayed. This is accomplished by destroying it. If the
    // holding space model is empty, the holding space tray will also need to
    // update its visibility to become hidden.
    DestroyPlaceholder();
    delegate()->UpdateTrayVisibility();
  }
}

BEGIN_METADATA(PinnedFilesSection)
END_METADATA

}  // namespace ash
