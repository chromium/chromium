// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_toast_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/bubble/bubble_utils.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "ash/style/typography.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
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

constexpr int kToastMinimumHeight = 32;
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
    flags.setColor(
        GetColorProvider()->GetColor(cros_tokens::kCrosSysSystemOnBase));
    canvas->DrawRoundRect(GetContentsBounds(), kIconCornerRadius, flags);
    SkPath mask;
    mask.addRoundRect(gfx::RectToSkRect(GetContentsBounds()), kIconCornerRadius,
                      kIconCornerRadius);
    canvas->ClipPath(mask, true);
    views::ImageView::OnPaint(canvas);
  }
};

}  // namespace

AppListToastView::Builder::Builder(std::u16string title)
    : title_(std::move(title)) {}

AppListToastView::Builder::~Builder() = default;

std::unique_ptr<AppListToastView> AppListToastView::Builder::Build() {
  std::unique_ptr<AppListToastView> toast =
      std::make_unique<AppListToastView>(title_, style_for_tablet_mode_);

  if (view_delegate_)
    toast->SetViewDelegate(view_delegate_);

  if (icon_) {
    toast->SetIcon(*icon_);
  }

  if (icon_size_)
    toast->SetIconSize(*icon_size_);

  if (has_icon_background_)
    toast->AddIconBackground();

  if (button_callback_)
    toast->SetButton(*button_text_, std::move(button_callback_));

  if (close_button_callback_)
    toast->SetCloseButton(std::move(close_button_callback_));

  if (subtitle_)
    toast->SetSubtitle(*subtitle_);

  if (subtitle_ && is_subtitle_multiline_) {
    toast->SetSubtitleMultiline(is_subtitle_multiline_);
  }

  return toast;
}

AppListToastView::Builder& AppListToastView::Builder::SetIcon(
    const ui::ImageModel& icon) {
  icon_ = icon;
  return *this;
}

AppListToastView::Builder& AppListToastView::Builder::SetSubtitle(
    const std::u16string& subtitle) {
  subtitle_ = subtitle;
  return *this;
}

AppListToastView::Builder& AppListToastView::Builder::SetSubtitleMultiline(
    bool multiline) {
  is_subtitle_multiline_ = multiline;
  return *this;
}

AppListToastView::Builder& AppListToastView::Builder::SetIconSize(
    int icon_size) {
  icon_size_ = icon_size;
  return *this;
}

AppListToastView::Builder& AppListToastView::Builder::SetButton(
    const std::u16string& button_text,
    views::Button::PressedCallback button_callback) {
  DCHECK(button_callback);

  button_text_ = button_text;
  button_callback_ = std::move(button_callback);
  return *this;
}

AppListToastView::Builder& AppListToastView::Builder::SetCloseButton(
    views::Button::PressedCallback close_button_callback) {
  DCHECK(close_button_callback);

  close_button_callback_ = std::move(close_button_callback);
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

AppListToastView::AppListToastView(const std::u16string& title,
                                   bool style_for_tablet_mode) {
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

  const ui::ColorId title_color_id = cros_tokens::kCrosSysOnSurface;
  bubble_utils::ApplyStyle(title_label_,
                           style_for_tablet_mode ? TypographyToken::kCrosBody1
                                                 : TypographyToken::kCrosBody2,
                           title_color_id);

  title_label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  title_label_->SetMultiLine(true);

  layout_manager_->SetFlexForView(label_container_, 1);

  if (style_for_tablet_mode) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
    layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
    layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(kCornerRadius));

    const ui::ColorId background_color_id =
        cros_tokens::kCrosSysSystemBaseElevated;
    SetBackground(views::CreateThemedRoundedRectBackground(background_color_id,
                                                           kCornerRadius));
    SetBorder(std::make_unique<views::HighlightBorder>(
        kCornerRadius, views::HighlightBorder::Type::kHighlightBorderNoShadow));
  } else {
    const ui::ColorId background_color_id = cros_tokens::kCrosSysSystemOnBase;
    SetBackground(views::CreateThemedRoundedRectBackground(background_color_id,
                                                           kCornerRadius));
  }
}

AppListToastView::~AppListToastView() = default;

void AppListToastView::SetButton(
    std::u16string button_text,
    views::Button::PressedCallback button_callback) {
  DCHECK(button_callback);

  toast_button_ =
      AddChildView(std::make_unique<AppListToastView::ToastPillButton>(
          view_delegate_, std::move(button_callback), button_text,
          PillButton::Type::kDefaultWithoutIcon,
          /*icon=*/nullptr));
  toast_button_->SetBorder(views::NullBorder());
}

void AppListToastView::SetCloseButton(
    views::Button::PressedCallback close_button_callback) {
  DCHECK(close_button_callback);

  close_button_ = AddChildView(std::make_unique<IconButton>(
      std::move(close_button_callback), IconButton::Type::kMediumFloating,
      &vector_icons::kCloseIcon,
      IDS_ASH_LAUNCHER_CLOSE_SORT_TOAST_BUTTON_SPOKEN_TEXT));
  close_button_->SetProperty(views::kMarginsKey, kCloseButtonMargin);
}

void AppListToastView::SetTitle(const std::u16string& title) {
  title_label_->SetText(title);
}

void AppListToastView::SetSubtitle(const std::u16string& subtitle) {
  if (subtitle_label_) {
    subtitle_label_->SetText(subtitle);
    return;
  }

  subtitle_label_ =
      label_container_->AddChildView(std::make_unique<views::Label>(subtitle));
  const ui::ColorId label_color_id = cros_tokens::kCrosSysOnSurfaceVariant;
  bubble_utils::ApplyStyle(subtitle_label_, TypographyToken::kCrosAnnotation1,
                           label_color_id);
  subtitle_label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
}

void AppListToastView::SetSubtitleMultiline(bool multiline) {
  if (!subtitle_label_) {
    return;
  }

  subtitle_label_->SetMultiLine(multiline);
}

void AppListToastView::SetIcon(const ui::ImageModel& icon) {
  CreateIconView();

  default_icon_ = icon;
  UpdateIconImage();
}

void AppListToastView::SetIconSize(int icon_size) {
  icon_size_ = icon_size;
  if (icon_)
    UpdateIconImage();
}

void AppListToastView::AddIconBackground() {
  if (icon_) {
    RemoveChildViewT(icon_.get());
    icon_ = nullptr;
  }

  has_icon_background_ = true;
  CreateIconView();
  UpdateIconImage();
}

void AppListToastView::SetAvailableWidth(int width) {
  if (available_width_ == width) {
    return;
  }
  available_width_ = width;
}

gfx::Size AppListToastView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int available_width = std::min(
      kToastMaximumWidth, available_width_.value_or(kToastMaximumWidth));

  // Ensure that the toast can accommodate text in the label container.
  const int available_label_container_width =
      GetLabelWidthForToastWidth(available_width);
  // Adjust the label container width so it fits as much of the text per line,
  // but still fits within the available width for the toast labels.
  const int preferred_label_container_width =
      GetMaxLabelContainerWidth(available_label_container_width);
  const int min_height_for_labels =
      layout_manager_->inside_border_insets().height() +
      kTitleContainerMargin.height() +
      label_container_->GetHeightForWidth(preferred_label_container_width);
  // `available_width` would leave `available_label_container_width` space for
  // labels. Reduce `available_width` by the difference in available and
  // preferred width for label container to get ideal width for the toast.
  const int ideal_width = available_width - (available_label_container_width -
                                             preferred_label_container_width);
  return gfx::Size(
      std::max(kToastMinimumWidth, ideal_width),
      std::max(std::max(kToastMinimumHeight, min_height_for_labels),
               GetLayoutManager()->GetPreferredSize(this).height()));
}

void AppListToastView::Layout(PassKey) {
  // Make sure that labels are sized so the text fits the available width, logic
  // in `GetPreferredSize()` should ensure the toast is large enough for the
  // text to be visible within the UI.
  const int label_width = GetLabelWidthForToastWidth(width());
  title_label_->SetSize(
      gfx::Size(label_width, title_label_->GetHeightForWidth(label_width)));
  if (subtitle_label_) {
    subtitle_label_->SetSize(gfx::Size(
        label_width, subtitle_label_->GetHeightForWidth(label_width)));
  }

  LayoutSuperclass<views::View>(this);
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
    : PillButton(std::move(callback), text, type, icon),
      view_delegate_(view_delegate) {
  views::FocusRing::Get(this)->SetHasFocusPredicate(
      base::BindRepeating([](const View* view) {
        const auto* v = views::AsViewClass<ToastPillButton>(view);
        CHECK(v);
        // With a `view_delegate_` present, focus ring should only show when
        // button is focused and keyboard traversal is engaged.
        return (!v->view_delegate_ ||
                v->view_delegate_->KeyboardTraversalEngaged()) &&
               v->HasFocus();
      }));
}

void AppListToastView::ToastPillButton::OnFocus() {
  PillButton::OnFocus();
  views::FocusRing::Get(this)->SchedulePaint();
}

void AppListToastView::ToastPillButton::OnBlur() {
  PillButton::OnBlur();
  views::FocusRing::Get(this)->SchedulePaint();
}

BEGIN_METADATA(AppListToastView, ToastPillButton)
END_METADATA

void AppListToastView::UpdateIconImage() {
  if (!icon_)
    return;

  if (!default_icon_) {
    return;
  }

  icon_->SetImage(*default_icon_);
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

int AppListToastView::GetLabelWidthForToastWidth(int toast_width) const {
  int available_space = toast_width -
                        layout_manager_->inside_border_insets().width() -
                        kTitleContainerMargin.width();
  for (const auto& child : children()) {
    if (child->GetVisible() && child != label_container_) {
      // Reserve space for a label container sibling.
      available_space -= child->GetPreferredSize().width();

      // Reserve space for a label container siblings' margins.
      // NOTE: This assumes that the children margins are not collapsed,
      // otherwise margin overlaps would potentially get counted twice.
      CHECK(!layout_manager_->GetCollapseMarginsSpacing());
      const gfx::Insets* margins = child->GetProperty(views::kMarginsKey);
      if (margins) {
        available_space -= margins->width();
      }
    }
  }

  return available_space;
}

int AppListToastView::GetMaxLabelContainerWidth(int available_width) const {
  int labels_width =
      title_label_->CalculatePreferredSize({available_width, 0}).width();
  if (subtitle_label_) {
    labels_width = std::max(
        labels_width,
        subtitle_label_->CalculatePreferredSize({available_width, 0}).width());
  }
  return labels_width;
}

BEGIN_METADATA(AppListToastView)
END_METADATA

}  // namespace ash
