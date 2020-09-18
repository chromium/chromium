// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_interstitial_view.h"

#include <memory>

#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/unified/rounded_label_button.h"
#include "base/strings/string16.h"
#include "skia/ext/image_operations.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace ash {

namespace {

// Appearance.
// TODO(meilinw): Update those constants to spec.
constexpr int kImageWidthDip = 330;
constexpr int kImageHeightDip = 200;
constexpr int kDialogContentWidthDip = 330;
constexpr int kHorizontalPaddingDip = 20;
constexpr int kVerticalPaddingDip = 20;
constexpr int kTitleBottomPaddingDip = 10;
constexpr int kButtonSpacingDip = 10;
constexpr int kButtonContainerTopPaddingDip = 45;
constexpr int kProgressBarHeightDip = 2;
constexpr double kInfiniteLoadingProgressValue = -1.0;

// Adds a ColumnSet on |layout| with a single View column and padding columns
// on either side of it with |padding| width.
void AddColumnWithSidePadding(views::GridLayout* layout, int padding, int id) {
  views::ColumnSet* column_set = layout->AddColumnSet(id);
  column_set->AddPaddingColumn(views::GridLayout::kFixedSize, padding);
  column_set->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                        views::GridLayout::kFixedSize,
                        views::GridLayout::ColumnSize::kFixed,
                        kDialogContentWidthDip, 0);
  column_set->AddPaddingColumn(views::GridLayout::kFixedSize, padding);
}

}  // namespace

PhoneHubInterstitialView::PhoneHubInterstitialView(bool show_progress) {
  InitLayout(show_progress);
}

PhoneHubInterstitialView::~PhoneHubInterstitialView() = default;

void PhoneHubInterstitialView::SetImage(const gfx::ImageSkia& image) {
  // Expect a non-empty string for the title.
  DCHECK(!image.isNull());
  image_->SetImage(image);
}

void PhoneHubInterstitialView::SetTitle(const base::string16& title) {
  // Expect a non-empty string for the title.
  DCHECK(!title.empty());
  title_->SetText(title);
}

void PhoneHubInterstitialView::SetDescription(const base::string16& desc) {
  // Expect a non-empty string for the description.
  DCHECK(!desc.empty());
  description_->SetText(desc);
}

void PhoneHubInterstitialView::AddButton(
    std::unique_ptr<views::Button> button) {
  button_container_->AddChildView(std::move(button));
}

void PhoneHubInterstitialView::InitLayout(bool show_progress) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // Set up layout column.
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  const int kFirstColumnSetId = 0;
  // Set up the first column set to layout the progressing bar if needed.
  views::ColumnSet* column_set = layout->AddColumnSet(kFirstColumnSetId);
  column_set->AddColumn(views::GridLayout::Alignment::FILL,
                        views::GridLayout::CENTER, 1,
                        views::GridLayout::ColumnSize::kFixed, 0, 0);
  // Set up the second column set with horizontal paddings to layout the image,
  // text and buttons.
  const int kSecondColumnSetId = 1;
  AddColumnWithSidePadding(layout, kHorizontalPaddingDip, kSecondColumnSetId);

  if (show_progress) {
    // Set up layout row for the progress bar if |show_progess| is true.
    layout->StartRow(views::GridLayout::kFixedSize, kFirstColumnSetId);
    progress_bar_ = layout->AddView(
        std::make_unique<views::ProgressBar>(kProgressBarHeightDip));
    progress_bar_->SetForegroundColor(
        AshColorProvider::Get()->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kIconColorProminent));
    progress_bar_->SetValue(kInfiniteLoadingProgressValue);
  }

  // Set up layout row for the image view.
  layout->StartRow(views::GridLayout::kFixedSize, kSecondColumnSetId);
  image_ = layout->AddView(std::make_unique<views::ImageView>());
  image_->SetImageSize(gfx::Size(kImageWidthDip, kImageHeightDip));

  // Set up layout row for the title view, which should be left-aligned.
  layout->StartRow(views::GridLayout::kFixedSize, kSecondColumnSetId);
  title_ =
      layout->AddView(std::make_unique<views::Label>(), 1, 1,
                      views::GridLayout::LEADING, views::GridLayout::CENTER);
  TrayPopupItemStyle title_style(TrayPopupItemStyle::FontStyle::SUB_HEADER);
  title_style.SetupLabel(title_);

  // Set up layout row for the multi-line description view.
  layout->StartRowWithPadding(views::GridLayout::kFixedSize, kSecondColumnSetId,
                              views::GridLayout::kFixedSize,
                              kTitleBottomPaddingDip);
  description_ = layout->AddView(std::make_unique<views::Label>());
  TrayPopupItemStyle body_style(
      TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL);
  body_style.SetupLabel(description_);
  description_->SetMultiLine(true);
  description_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  // Set up the layout row for the button container view, which should be
  // right-aligned.
  layout->StartRowWithPadding(views::GridLayout::kFixedSize, kSecondColumnSetId,
                              views::GridLayout::kFixedSize,
                              kButtonContainerTopPaddingDip);
  button_container_ =
      layout->AddView(std::make_unique<views::View>(), 1, 1,
                      views::GridLayout::TRAILING, views::GridLayout::CENTER);
  button_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kButtonSpacingDip));

  // Set up the layout row for the bottom spacing.
  layout->AddPaddingRow(views::GridLayout::kFixedSize, kVerticalPaddingDip);
}

BEGIN_METADATA(PhoneHubInterstitialView, views::View)
END_METADATA

}  // namespace ash
