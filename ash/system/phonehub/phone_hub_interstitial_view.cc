// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_interstitial_view.h"

#include <memory>
#include <string>

#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/phonehub/ui_constants.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "skia/ext/image_operations.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr gfx::Size kImageSizeDip = {216, 216};
constexpr int kImageRowHeightDip = 256;
constexpr int kButtonSpacingDip = 8;
constexpr int kButtonContainerTopPaddingDip = 16;
constexpr int kProgressBarHeightDip = 2;
constexpr double kInfiniteLoadingProgressValue = -1.0;
constexpr int kTitleLabelLineHeightDip = 48;
constexpr int kDescriptionLabelLineHeightDip = 20;
constexpr auto kTextLabelInsetsDip = gfx::Insets::TLBR(0, 4, 0, 4);

// Adds a ColumnSet on |layout| with a single View column and padding columns
// on either side of it with |padding| width.
void AddColumnWithSidePadding(views::GridLayout* layout, int padding, int id) {
  views::ColumnSet* column_set = layout->AddColumnSet(id);
  column_set->AddPaddingColumn(views::GridLayout::kFixedSize, padding);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                        /*resize_precent=*/1.0,
                        views::GridLayout::ColumnSize::kUsePreferred,
                        /*fixed_width=*/0, /*min_width=*/0);
  column_set->AddPaddingColumn(views::GridLayout::kFixedSize, padding);
}

}  // namespace

PhoneHubInterstitialView::PhoneHubInterstitialView(bool show_progress,
                                                   bool show_image) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // Set up layout column.
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  const int kFirstColumnSetId = 0;
  // Set up the first column set to layout the progressing bar if needed.
  views::ColumnSet* column_set = layout->AddColumnSet(kFirstColumnSetId);
  column_set->AddColumn(views::GridLayout::Alignment::FILL,
                        views::GridLayout::CENTER, /*resize_precent=*/1.0,
                        views::GridLayout::ColumnSize::kFixed,
                        /*fixed_width=*/0, /*min_width=*/0);
  // Set up the second column set with horizontal paddings to layout the image,
  // text and buttons.
  const int kSecondColumnSetId = 1;
  AddColumnWithSidePadding(layout, kBubbleHorizontalSidePaddingDip,
                           kSecondColumnSetId);

  auto* color_provider = AshColorProvider::Get();
  if (show_progress) {
    // Set up layout row for the progress bar if |show_progess| is true.
    layout->StartRow(views::GridLayout::kFixedSize, kFirstColumnSetId);
    progress_bar_ = layout->AddView(
        std::make_unique<views::ProgressBar>(kProgressBarHeightDip));
    progress_bar_->SetForegroundColor(color_provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kIconColorProminent));
    progress_bar_->SetValue(kInfiniteLoadingProgressValue);
  }

  // Set up layout row for the image if any.
  if (show_image) {
    layout->StartRow(views::GridLayout::kFixedSize, kSecondColumnSetId,
                     kImageRowHeightDip);
    image_ =
        layout->AddView(std::make_unique<views::ImageView>(), 1, 1,
                        views::GridLayout::CENTER, views::GridLayout::CENTER);
    image_->SetImageSize(kImageSizeDip);
  }

  // Set up layout row for the title view, which should be left-aligned.
  layout->StartRow(views::GridLayout::kFixedSize, kSecondColumnSetId);
  title_ =
      layout->AddView(std::make_unique<views::Label>(), 1, 1,
                      views::GridLayout::LEADING, views::GridLayout::CENTER);
  title_->SetLineHeight(kTitleLabelLineHeightDip);
  title_->SetBorder(views::CreateEmptyBorder(kTextLabelInsetsDip));
  auto label_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
  title_->SetEnabledColor(label_color);
  TrayPopupUtils::SetLabelFontList(title_,
                                   TrayPopupUtils::FontStyle::kSubHeader);

  // Set up layout row for the multi-line description view.
  layout->StartRow(views::GridLayout::kFixedSize, kSecondColumnSetId);
  description_ = layout->AddView(std::make_unique<views::Label>());
  description_->SetEnabledColor(label_color);
  TrayPopupUtils::SetLabelFontList(
      description_, TrayPopupUtils::FontStyle::kDetailedViewLabel);
  description_->SetBorder(views::CreateEmptyBorder(kTextLabelInsetsDip));
  description_->SetMultiLine(true);
  description_->SetLineHeight(kDescriptionLabelLineHeightDip);
  description_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  layout->AddPaddingRow(views::GridLayout::kFixedSize,
                        kButtonContainerTopPaddingDip);

  // Set up the layout row for the button container view, which should be
  // right-aligned.
  layout->StartRow(views::GridLayout::kFixedSize, kSecondColumnSetId,
                   kTrayItemSize);
  button_container_ =
      layout->AddView(std::make_unique<views::View>(), 1, 1,
                      views::GridLayout::TRAILING, views::GridLayout::CENTER);
  button_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kButtonSpacingDip));
}

PhoneHubInterstitialView::~PhoneHubInterstitialView() = default;

void PhoneHubInterstitialView::SetImage(const ui::ImageModel& image_model) {
  // Expect a non-null |image_| view and a nonempty |image_model|.
  DCHECK(image_);
  DCHECK(!image_model.IsEmpty());
  image_->SetImage(image_model);
}

void PhoneHubInterstitialView::SetTitle(const std::u16string& title) {
  // Expect a non-empty string for the title.
  DCHECK(!title.empty());
  title_->SetText(title);
}

void PhoneHubInterstitialView::SetDescription(const std::u16string& desc) {
  // Expect a non-empty string for the description.
  DCHECK(!desc.empty());
  description_->SetText(desc);
}

void PhoneHubInterstitialView::AddButton(
    std::unique_ptr<views::Button> button) {
  button_container_->AddChildView(std::move(button));
}

BEGIN_METADATA(PhoneHubInterstitialView, views::View)
END_METADATA

}  // namespace ash
