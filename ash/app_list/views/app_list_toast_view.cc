// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_toast_view.h"

#include <memory>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/bubble/bubble_utils.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

constexpr int kCornerRadius = 16;
constexpr auto kInteriorMargin = gfx::Insets::TLBR(8, 8, 8, 16);
constexpr auto kTitleContainerMargin = gfx::Insets::TLBR(0, 16, 0, 24);
constexpr auto kCloseButtonMargin = gfx::Insets::TLBR(0, 8, 0, 0);

constexpr int kToastHeight = 32;
constexpr int kToastMaximumWidth = 640;
constexpr int kToastMinimumWidth = 288;

constexpr int kIconCornerRadius = 8;

class IconImageWithBackground : public views::ImageView {
 public:
  IconImageWithBackground() = default;
  IconImageWithBackground(const IconImageWithBackground&) = delete;
  IconImageWithBackground& operator=(const IconImageWithBackground&) = delete;
  ~IconImageWithBackground() override = default;

 private:
  // views::ImageView:
  void OnPaint(gfx::Canvas* canvas) override {
    if (GetImage().isNull())
      return;

    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(AshColorProvider::Get()->GetControlsLayerColor(
        AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive));
    canvas->DrawRoundRect(GetContentsBounds(), kIconCornerRadius, flags);
    SkPath mask;
    mask.addRoundRect(gfx::RectToSkRect(GetContentsBounds()), kIconCornerRadius,
                      kIconCornerRadius);
    canvas->ClipPath(mask, true);
    views::ImageView::OnPaint(canvas);
  }
};

}  // namespace

AppListToastView::Builder::Builder(const std::u16string title)
    : title_(title) {}

AppListToastView::Builder::~Builder() = default;

std::unique_ptr<AppListToastView> AppListToastView::Builder::Build() {
  std::unique_ptr<AppListToastView> toast =
      std::make_unique<AppListToastView>(title_);

  if (view_delegate_)
    toast->SetViewDelegate(view_delegate_);

  if (style_for_tablet_mode_)
    toast->StyleForTabletMode();

  if (dark_icon_ && light_icon_)
    toast->SetThemingIcons(dark_icon_, light_icon_);
  else if (icon_)
    toast->SetIcon(icon_);

  if (icon_size_)
    toast->SetIconSize(*icon_size_);

  if (has_icon_background_)
    toast->AddIconBackground();

  if (button_callback_)
    toast->SetButton(*button_text_, button_callback_);

  if (close_button_callback_)
    toast->SetCloseButton(close_button_callback_);

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

AppListToastView::Builder& AppListToastView::Builder::SetIconSize(
    int icon_size) {
  icon_size_ = icon_size;
  return *this;
}

AppListToastView::Builder& AppListToastView::Builder::SetButton(
    const std::u16string button_text,
    views::Button::PressedCallback button_callback) {
  DCHECK(button_callback);

  button_text_ = button_text;
  button_callback_ = button_callback;
  return *this;
}

AppListToastView::Builder& AppListToastView::Builder::SetCloseButton(
    views::Button::PressedCallback close_button_callback) {
  DCHECK(close_button_callback);

  close_button_callback_ = close_button_callback;
  return *this;
}

AppListToastView::Builder& AppListToastView::Builder::SetStyleForTabletMode(
    bool style_for_tablet_mode) {
  style_for_tablet_mode_ = style_for_tablet_mode;
  return *this;
}

AppListToastView::Builder& AppListToastView::Builder::SetViewDelegate(
    AppListViewDelegate* delegate) {
  view_delegate_ = delegate;
  return *this;
}

void AppListToastView::SetViewDelegate(AppListViewDelegate* delegate) {
  view_delegate_ = delegate;
}

AppListToastView::Builder& AppListToastView::Builder::SetIconBackground(
    bool has_icon_background) {
  has_icon_background_ = has_icon_background;
  return *this;
}

bool AppListToastView::IsToastButton(views::View* view) {
  return views::IsViewClass<ToastPillButton>(view);
}

AppListToastView::AppListToastView(const std::u16string title) {
  layout_manager_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kInteriorMargin));
  layout_manager_->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  label_container_ = AddChildView(std::make_unique<views::View>());
  label_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  label_container_->SetProperty(views::kMarginsKey, kTitleContainerMargin);

  title_label_ =
      label_container_->AddChildView(std::make_unique<views::Label>(title));
  bubble_utils::ApplyStyle(title_label_, bubble_utils::TypographyStyle::kBody2);
  title_label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  title_label_->SetMultiLine(true);
  // TODO(crbug/682266): This is a temporary fix for the issue where the multi
  // line label appears cut-off.
  title_label_->SetMaximumWidth(GetExpandedTitleLabelWidth());

  layout_manager_->SetFlexForView(label_container_, 1);
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

  if (style_for_tablet_mode_) {
    SetBackground(views::CreateRoundedRectBackground(
        ColorProvider::Get()->GetBaseLayerColor(
            ColorProvider::BaseLayerType::kTransparent80),
        kCornerRadius));
    SetBorder(std::make_unique<views::HighlightBorder>(
        kCornerRadius, views::HighlightBorder::Type::kHighlightBorder1,
        /*use_light_colors=*/false));
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

  toast_button_ =
      AddChildView(std::make_unique<AppListToastView::ToastPillButton>(
          view_delegate_, button_callback, button_text,
          PillButton::Type::kDefaultWithoutIcon,
          /*icon=*/nullptr));
  toast_button_->SetBorder(views::NullBorder());
}

void AppListToastView::SetCloseButton(
    views::Button::PressedCallback close_button_callback) {
  DCHECK(close_button_callback);

  close_button_ = AddChildView(std::make_unique<IconButton>(
      close_button_callback, IconButton::Type::kMediumFloating,
      &vector_icons::kCloseIcon,
      IDS_ASH_LAUNCHER_CLOSE_SORT_TOAST_BUTTON_SPOKEN_TEXT));
  close_button_->SetProperty(views::kMarginsKey, kCloseButtonMargin);
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
  bubble_utils::ApplyStyle(subtitle_label_,
                           bubble_utils::TypographyStyle::kAnnotation1,
                           kColorAshTextColorSecondary);
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

void AppListToastView::SetIconSize(int icon_size) {
  icon_size_ = icon_size;
  if (icon_)
    UpdateIconImage();
}

void AppListToastView::AddIconBackground() {
  if (icon_) {
    RemoveChildViewT(icon_);
    icon_ = nullptr;
  }

  has_icon_background_ = true;
  CreateIconView();
  UpdateIconImage();
}

gfx::Size AppListToastView::GetMaximumSize() const {
  return gfx::Size(kToastMaximumWidth,
                   GetLayoutManager()->GetPreferredSize(this).height());
}

gfx::Size AppListToastView::GetMinimumSize() const {
  return gfx::Size(kToastMinimumWidth, kToastHeight);
}

gfx::Size AppListToastView::CalculatePreferredSize() const {
  gfx::Size preferred_size = GetLayoutManager()->GetPreferredSize(this);
  preferred_size.SetToMax(GetMinimumSize());
  preferred_size.SetToMin(GetMaximumSize());
  return preferred_size;
}

void AppListToastView::UpdateInteriorMargins(const gfx::Insets& margin) {
  layout_manager_->set_inside_border_insets(margin);
}

AppListToastView::ToastPillButton::ToastPillButton(
    AppListViewDelegate* view_delegate,
    PressedCallback callback,
    const std::u16string& text,
    Type type,
    const gfx::VectorIcon* icon)
    : PillButton(callback, text, type, icon), view_delegate_(view_delegate) {
  views::FocusRing::Get(this)->SetHasFocusPredicate([&](View* view) -> bool {
    // With a `view_delegate_` present, focus ring should only show when
    // button is focused and keyboard traversal is engaged.
    if (view_delegate_ && !view_delegate_->KeyboardTraversalEngaged())
      return false;

    return view->HasFocus();
  });
}

void AppListToastView::ToastPillButton::OnFocus() {
  PillButton::OnFocus();
  views::FocusRing::Get(this)->SchedulePaint();
}

void AppListToastView::ToastPillButton::OnBlur() {
  PillButton::OnBlur();
  views::FocusRing::Get(this)->SchedulePaint();
}

void AppListToastView::UpdateIconImage() {
  if (!icon_)
    return;

  if (default_icon_) {
    icon_->SetImage(ui::ImageModel::FromVectorIcon(
        *default_icon_,
        AshColorProvider::Get()->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kIconColorPrimary),
        icon_size_.value_or(gfx::GetDefaultSizeOfVectorIcon(*default_icon_))));
    return;
  }

  // Default to dark_icon_ if dark/light mode feature is not enabled.
  const gfx::VectorIcon* themed_icon =
      !features::IsDarkLightModeEnabled() ||
              DarkLightModeControllerImpl::Get()->IsDarkModeEnabled()
          ? dark_icon_
          : light_icon_;
  icon_->SetImage(ui::ImageModel::FromVectorIcon(
      *themed_icon, ui::kColorAshSystemUIMenuIcon,
      icon_size_.value_or(gfx::GetDefaultSizeOfVectorIcon(*themed_icon))));
}

void AppListToastView::CreateIconView() {
  if (icon_)
    return;

  icon_ = AddChildViewAt(has_icon_background_
                             ? std::make_unique<IconImageWithBackground>()
                             : std::make_unique<views::ImageView>(),
                         0);
  icon_->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  icon_->SetHorizontalAlignment(views::ImageView::Alignment::kCenter);
}

int AppListToastView::GetExpandedTitleLabelWidth() {
  const int icon_width = icon_ ? icon_->size().width() : 0;
  const int button_width = toast_button_ ? toast_button_->size().width() : 0;
  return GetPreferredSize().width() - kInteriorMargin.width() - icon_width -
         button_width - kTitleContainerMargin.width();
}

}  // namespace ash
