// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/kiosk_app_default_message.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/typography.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// The icon size of the kiosk app message.
constexpr int kIconSize = 16;

// The line height of the kiosk app message title.
constexpr int kTitleLineHeight = 20;

}  // namespace

KioskAppDefaultMessage::KioskAppDefaultMessage()
    : LoginBaseBubbleView(/*anchor_view=*/nullptr) {
  auto* layout_provider = views::LayoutProvider::Get();
  set_persistent(true);

  views::FlexLayout* layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal);
  layout->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  // Set up the icon.
  icon_ = AddChildView(std::make_unique<views::ImageView>());
  icon_->SetImage(ui::ImageModel::FromVectorIcon(
      vector_icons::kErrorOutlineIcon, kColorAshButtonIconColor, kIconSize));
  icon_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(/*top=*/0, /*left=*/0, /*bottom=*/0,
                        layout_provider->GetDistanceMetric(
                            views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));

  // Set up the title view.
  title_ = AddChildView(std::make_unique<views::Label>());
  title_->SetText(l10n_util::GetStringUTF16(IDS_SHELF_KIOSK_APP_SETUP));
  title_->SetLineHeight(kTitleLineHeight);
  title_->SetMultiLine(true);
  title_->SetEnabledColorId(kColorAshTextColorPrimary);
  title_->SetAutoColorReadabilityEnabled(false);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosBody2, *title_);
}

KioskAppDefaultMessage::~KioskAppDefaultMessage() = default;

gfx::Size KioskAppDefaultMessage::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  auto* layout_provider = views::LayoutProvider::Get();

  // width = left_margin + icon_width + component_distance + title_width +
  // right_margin
  int width =
      icon_->CalculatePreferredSize({}).width() +
      layout_provider->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_HORIZONTAL) +
      title_->CalculatePreferredSize(views::SizeBounds(title_->width(), {}))
          .width() +
      2 * layout_provider->GetDistanceMetric(
              views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL);
  // height = upper_margin + icon_height + lower_margin
  int height =
      kIconSize + 2 * layout_provider->GetDistanceMetric(
                          views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL);

  return gfx::Size(width, height);
}

gfx::Point KioskAppDefaultMessage::CalculatePosition() {
  return gfx::Point(parent()->GetLocalBounds().width() / 2,
                    parent()->GetLocalBounds().height() / 2) -
         gfx::Vector2d(width() / 2, height());
}

BEGIN_METADATA(KioskAppDefaultMessage)
END_METADATA

}  // namespace ash
