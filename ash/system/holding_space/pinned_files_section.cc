// Copyright 2020 The Chromium Authors
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
constexpr int kPlaceholderChildSpacing = 16;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr int kPlaceholderGSuiteIconSize = 20;
constexpr int kPlaceholderGSuiteIconSpacing = 8;

// Create a builder for an image view for the given G Suite icon.
views::Builder<views::ImageView> CreateGSuiteIcon(const gfx::VectorIcon& icon) {
  return views::Builder<views::ImageView>().SetImage(
      ui::ImageModel::FromVectorIcon(icon, gfx::kPlaceholderColor,
                                     kPlaceholderGSuiteIconSize));
}
#endif

// Returns placeholder text given whether or not Google Drive is disabled.
std::u16string GetPlaceholderText(bool drive_disabled) {
  return l10n_util::GetStringUTF16(
      drive_disabled ? IDS_ASH_HOLDING_SPACE_PINNED_EMPTY_PROMPT_DRIVE_DISABLED
                     : IDS_ASH_HOLDING_SPACE_PINNED_EMPTY_PROMPT);
}

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
  return std::make_unique<HoldingSpaceItemChipView>(delegate(), item);
}

std::unique_ptr<views::View> PinnedFilesSection::CreatePlaceholder() {
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

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // G Suite icons.
  auto icons_builder =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetBetweenChildSpacing(kPlaceholderGSuiteIconSpacing)
          .SetID(kHoldingSpacePinnedFilesSectionPlaceholderGSuiteIconsId);

  if (!drive_disabled) {
    icons_builder.AddChild(CreateGSuiteIcon(vector_icons::kGoogleDriveIcon));
  }

  icons_builder.AddChild(CreateGSuiteIcon(vector_icons::kGoogleSlidesIcon))
      .AddChild(CreateGSuiteIcon(vector_icons::kGoogleDocsIcon))
      .AddChild(CreateGSuiteIcon(vector_icons::kGoogleSheetsIcon));
  placeholder_builder.AddChild(std::move(icons_builder));
#endif

  return std::move(placeholder_builder).Build();
}

BEGIN_METADATA(PinnedFilesSection)
END_METADATA

}  // namespace ash
