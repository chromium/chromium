// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/media_controls_header_view.h"

#include "ash/login/ui/non_accessible_view.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

constexpr int kIconViewSize = 20;
constexpr int kIconSize = 14;
constexpr int kHeaderTextFontSize = 12;
constexpr gfx::Insets kAppNamePadding = gfx::Insets(0, 8, 0, 0);
constexpr gfx::Size kAppNamePreferredSize = gfx::Size(200, 10);
constexpr gfx::Size kCloseButtonSize = gfx::Size(20, 20);
constexpr int kCloseButtonIconSize = 18;
constexpr gfx::Size kSpacerPreferredSize = gfx::Size(5, 5);

}  // namespace

MediaControlsHeaderView::MediaControlsHeaderView(
    base::OnceClosure close_button_cb)
    : close_button_cb_(std::move(close_button_cb)) {
  const views::FlexSpecification kAppNameFlex =
      views::FlexSpecification::ForSizeRule(
          views::MinimumFlexSizeRule::kScaleToZero,
          views::MaximumFlexSizeRule::kPreferred)
          .WithOrder(1);

  const views::FlexSpecification kSpacerFlex =
      views::FlexSpecification::ForSizeRule(
          views::MinimumFlexSizeRule::kScaleToMinimum,
          views::MaximumFlexSizeRule::kUnbounded)
          .WithOrder(2);

  SetLayoutManager(std::make_unique<views::FlexLayout>());

  auto app_icon_view = std::make_unique<views::ImageView>();
  app_icon_view->SetPreferredSize(gfx::Size(kIconViewSize, kIconViewSize));
  app_icon_view->SetImageSize(gfx::Size(kIconSize, kIconSize));
  app_icon_view->SetBackground(
      views::CreateRoundedRectBackground(SK_ColorWHITE, kIconViewSize / 2));

  app_icon_view_ = AddChildView(std::move(app_icon_view));

  // Font list for text views.
  gfx::Font default_font;
  int font_size_delta = kHeaderTextFontSize - default_font.GetFontSize();
  gfx::Font font = default_font.Derive(font_size_delta, gfx::Font::NORMAL,
                                       gfx::Font::Weight::NORMAL);
  gfx::FontList font_list(font);

  auto app_name_view = std::make_unique<views::Label>();
  app_name_view->SetFontList(font_list);
  app_name_view->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  app_name_view->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextSecondary,
      AshColorProvider::AshColorMode::kDark));
  app_name_view->SetAutoColorReadabilityEnabled(false);
  app_name_view->SetBorder(views::CreateEmptyBorder(kAppNamePadding));
  app_name_view->SetPreferredSize(kAppNamePreferredSize);
  app_name_view->SetProperty(views::kFlexBehaviorKey, kAppNameFlex);
  app_name_view->SetElideBehavior(gfx::ELIDE_TAIL);
  app_name_view_ = AddChildView(std::move(app_name_view));

  // Space between app name and close button.
  auto spacer = std::make_unique<NonAccessibleView>();
  spacer->SetPreferredSize(kSpacerPreferredSize);
  spacer->SetProperty(views::kFlexBehaviorKey, kSpacerFlex);
  AddChildView(std::move(spacer));

  auto close_button = CreateVectorImageButton(this);
  close_button->SetPreferredSize(kCloseButtonSize);
  close_button->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  base::string16 close_button_label(
      l10n_util::GetStringUTF16(IDS_ASH_LOCK_SCREEN_MEDIA_CONTROLS_CLOSE));
  close_button->SetAccessibleName(close_button_label);
  close_button->set_ink_drop_base_color(
      color_utils::DeriveDefaultIconColor(gfx::kGoogleGrey700));
  close_button_ = AddChildView(std::move(close_button));
}

MediaControlsHeaderView::~MediaControlsHeaderView() = default;

void MediaControlsHeaderView::SetAppIcon(const gfx::ImageSkia& img) {
  app_icon_view_->SetImage(img);
}

void MediaControlsHeaderView::SetAppName(const base::string16& name) {
  app_name_view_->SetText(name);
}

void MediaControlsHeaderView::SetCloseButtonVisibility(bool visible) {
  if (visible) {
    SetImageFromVectorIcon(close_button_, vector_icons::kCloseRoundedIcon,
                           kCloseButtonIconSize, gfx::kGoogleGrey700);
  } else {
    close_button_->SetImage(views::Button::ButtonState::STATE_NORMAL, nullptr);
  }
}

void MediaControlsHeaderView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->SetName(app_name_view_->GetText());
}

void MediaControlsHeaderView::ButtonPressed(views::Button* sender,
                                            const ui::Event& event) {
  std::move(close_button_cb_).Run();
}

const base::string16& MediaControlsHeaderView::app_name_for_testing() const {
  return app_name_view_->GetText();
}

const views::ImageView* MediaControlsHeaderView::app_icon_for_testing() const {
  return app_icon_view_;
}

views::ImageButton* MediaControlsHeaderView::close_button_for_testing() const {
  return close_button_;
}

}  // namespace ash
