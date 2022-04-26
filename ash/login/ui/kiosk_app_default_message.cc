// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/kiosk_app_default_message.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
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
    : BubbleDialogDelegateView(nullptr, views::BubbleBorder::NONE),
      background_animator_(
          /* Don't pass the Shelf so the translucent color is always used. */
          nullptr,
          Shell::Get()->wallpaper_controller()) {
  auto* layout_provider = views::LayoutProvider::Get();
  set_margins(gfx::Insets(layout_provider->GetDistanceMetric(
      views::DISTANCE_DIALOG_CONTENT_MARGIN_TOP_CONTROL)));
  SetShowCloseButton(false);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  // Bubbles that use transparent colors should not paint their ClientViews to a
  // layer as doing so could result in visual artifacts.
  SetPaintClientToLayer(false);
  background_animator_.Init(ShelfBackgroundType::kDefaultBg);
  background_animator_observation_.Observe(&background_animator_);

  views::FlexLayout* layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal);
  layout->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  // Set up the icon.
  icon_ = AddChildView(std::make_unique<views::ImageView>());
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
  TrayPopupUtils::SetLabelFontList(title_,
                                   TrayPopupUtils::FontStyle::kSmallTitle);

  views::DialogDelegate::CreateDialogWidget(
      this, nullptr /* context */,
      Shell::GetContainer(ash::Shell::GetRootWindowForNewWindows(),
                          kShellWindowId_SettingBubbleContainer) /* parent */);

  GetBubbleFrameView()->SetCornerRadius(
      views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::Emphasis::kHigh));
  GetBubbleFrameView()->SetBackgroundColor(
      AshColorProvider::Get()->GetBaseLayerColor(
          AshColorProvider::BaseLayerType::kTransparent90));
}

KioskAppDefaultMessage::~KioskAppDefaultMessage() = default;

void KioskAppDefaultMessage::OnThemeChanged() {
  views::View::OnThemeChanged();
  auto* color_provider = AshColorProvider::Get();

  SkColor icon_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonIconColor);
  icon_->SetImage(gfx::CreateVectorIcon(vector_icons::kErrorOutlineIcon,
                                        kIconSize, icon_color));

  SkColor label_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
  title_->SetEnabledColor(label_color);
}

}  // namespace ash