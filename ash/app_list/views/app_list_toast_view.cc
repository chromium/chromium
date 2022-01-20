// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_toast_view.h"

#include <memory>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/bubble/bubble_utils.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {

constexpr int kCornerRadius = 16;
constexpr gfx::Insets kInteriorMargin(7, 8, 7, 16);

constexpr int kIconMargins = 8;
constexpr int kToastHeight = 32;
constexpr int kToastMaximumWidth = 512;
constexpr int kToastMinimumWidth = 288;

AppListToastView::Builder::Builder(const std::u16string title)
    : title_(title) {}

AppListToastView::Builder::~Builder() = default;

std::unique_ptr<AppListToastView> AppListToastView::Builder::Build() {
  std::unique_ptr<AppListToastView> toast =
      std::make_unique<AppListToastView>(title_);

  if (style_for_tablet_mode_)
    toast->StyleForTabletMode();

  if (dark_icon_ && light_icon_)
    toast->SetThemingIcons(dark_icon_, light_icon_);
  else if (icon_)
    toast->SetIcon(icon_);

  if (has_button_)
    toast->SetButton(*button_text_, button_callback_);

  if (subtitle_)
    toast->SetSubtitle(*subtitle_);

  return toast;
}

AppListToastView::Builder& AppListToastView::Builder::SetIcon(
    const gfx::VectorIcon* icon) {
  DCHECK(!dark_icon_);
  DCHECK(!light_icon_);

  icon_ = icon;
  return *this;
}

AppListToastView::Builder& AppListToastView::Builder::SetThemingIcons(
    const gfx::VectorIcon* dark_icon,
    const gfx::VectorIcon* light_icon) {
  DCHECK(!icon_);

  dark_icon_ = dark_icon;
  light_icon_ = light_icon;
  return *this;
}

AppListToastView::Builder& AppListToastView::Builder::SetSubtitle(
    const std::u16string subtitle) {
  subtitle_ = subtitle;
  return *this;
}

AppListToastView::Builder& AppListToastView::Builder::SetButton(
    const std::u16string button_text,
    views::Button::PressedCallback button_callback) {
  DCHECK(button_callback);

  has_button_ = true;
  button_text_ = button_text;
  button_callback_ = button_callback;
  return *this;
}

AppListToastView::Builder& AppListToastView::Builder::SetStyleForTabletMode(
    bool style_for_tablet_mode) {
  style_for_tablet_mode_ = style_for_tablet_mode;
  return *this;
}

AppListToastView::AppListToastView(const std::u16string title) {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, kInteriorMargin));
  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  label_container_ = AddChildView(std::make_unique<views::View>());
  label_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  title_label_ =
      label_container_->AddChildView(std::make_unique<views::Label>(title));
  title_label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);

  layout_manager->SetFlexForView(label_container_, 1);
}

AppListToastView::~AppListToastView() = default;

void AppListToastView::StyleForTabletMode() {
  style_for_tablet_mode_ = true;

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(kCornerRadius));
}

void AppListToastView::OnThemeChanged() {
  views::View::OnThemeChanged();
  if (title_label_)
    bubble_utils::ApplyStyle(title_label_, bubble_utils::LabelStyle::kBody);
  if (subtitle_label_)
    bubble_utils::ApplyStyle(subtitle_label_,
                             bubble_utils::LabelStyle::kSubtitle);

  if (style_for_tablet_mode_) {
    SetBackground(views::CreateRoundedRectBackground(
        ColorProvider::Get()->GetBaseLayerColor(
            ColorProvider::BaseLayerType::kTransparent80),
        kCornerRadius));
  } else {
    SetBackground(views::CreateRoundedRectBackground(
        AshColorProvider::Get()->GetControlsLayerColor(
            AshColorProvider::ControlsLayerType::
                kControlBackgroundColorInactive),
        kCornerRadius));
  }

  UpdateIconImage();
}

void AppListToastView::SetButton(
    std::u16string button_text,
    views::Button::PressedCallback button_callback) {
  DCHECK(button_callback);

  toast_button_ = AddChildView(std::make_unique<PillButton>(
      button_callback, button_text, PillButton::Type::kIconless,
      /*icon=*/nullptr));
}

void AppListToastView::SetTitle(const std::u16string title) {
  title_label_->SetText(title);
}

void AppListToastView::SetSubtitle(const std::u16string subtitle) {
  if (subtitle_label_) {
    subtitle_label_->SetText(subtitle);
    return;
  }

  subtitle_label_ =
      label_container_->AddChildView(std::make_unique<views::Label>(subtitle));
  subtitle_label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
}

void AppListToastView::SetIcon(const gfx::VectorIcon* icon) {
  DCHECK(!dark_icon_);
  DCHECK(!light_icon_);

  CreateIconView();

  default_icon_ = icon;
  UpdateIconImage();
}

void AppListToastView::SetThemingIcons(const gfx::VectorIcon* dark_icon,
                                       const gfx::VectorIcon* light_icon) {
  DCHECK(!default_icon_);

  CreateIconView();

  dark_icon_ = dark_icon;
  light_icon_ = light_icon;
  UpdateIconImage();
}

gfx::Size AppListToastView::GetMaximumSize() const {
  return gfx::Size(kToastMaximumWidth, GetPreferredSize().height());
}

gfx::Size AppListToastView::GetMinimumSize() const {
  return gfx::Size(kToastMinimumWidth, kToastHeight);
}

void AppListToastView::UpdateIconImage() {
  if (!icon_)
    return;

  if (default_icon_) {
    icon_->SetImage(ui::ImageModel::FromVectorIcon(
        *default_icon_,
        AshColorProvider::Get()->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kIconColorPrimary)));
    return;
  }

  // Default to dark_icon_ if dark/light mode feature is not enabled.
  icon_->SetImage(ui::ImageModel::FromVectorIcon(
      !features::IsDarkLightModeEnabled() ||
              AshColorProvider::Get()->IsDarkModeEnabled()
          ? *dark_icon_
          : *light_icon_));
}

void AppListToastView::CreateIconView() {
  if (icon_)
    return;

  icon_ = AddChildViewAt(std::make_unique<views::ImageView>(), 0);
  icon_->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  icon_->SetHorizontalAlignment(views::ImageView::Alignment::kCenter);
  icon_->SetProperty(views::kMarginsKey, gfx::Insets(kIconMargins));
}

}  // namespace ash
