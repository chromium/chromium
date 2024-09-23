// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/window_preview.h"

#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/wm/window_preview_view.h"
#include "ash/wm/window_util.h"
#include "base/functional/bind.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"

namespace ash {

// The margins around window titles.
constexpr int kTitleLineHeight = 20;
constexpr int kTitleMarginTop = 10;
constexpr int kTitleMarginBottom = 10;
constexpr int kTitleMarginRight = 16;

// The width and height of close buttons.
constexpr int kCloseButtonSize = 36;
constexpr int kCloseButtonImageSize = 24;
constexpr int kCloseButtonSideBleed = 8;
constexpr SkColor kCloseButtonColor = SK_ColorWHITE;

constexpr SkColor kPreviewContainerBgColor =
    SkColorSetA(gfx::kGoogleGrey100, 0x24);
constexpr int kPreviewBorderRadius = 4;

WindowPreview::WindowPreview(aura::Window* window, Delegate* delegate)
    : delegate_(delegate) {
  preview_view_ = new WindowPreviewView(window);
  preview_container_view_ = new views::View();
  preview_container_view_->SetBackground(views::CreateRoundedRectBackground(
      kPreviewContainerBgColor, kPreviewBorderRadius));
  title_ = new views::Label(window->GetTitle());
  close_button_ = new views::ImageButton(base::BindRepeating(
      &WindowPreview::CloseButtonPressed, base::Unretained(this)));
  close_button_->SetFocusBehavior(FocusBehavior::NEVER);

  AddChildView(preview_container_view_.get());
  AddChildView(preview_view_.get());
  AddChildView(title_.get());
  AddChildView(close_button_.get());
}

WindowPreview::~WindowPreview() = default;

gfx::Size WindowPreview::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // The preview itself will always be strictly contained within its container,
  // so only the container's size matters to calculate the preferred size.
  const gfx::Size container_size = GetPreviewContainerSize();
  const int title_height_with_padding =
      kTitleLineHeight + kTitleMarginTop + kTitleMarginBottom;
  return gfx::Size(container_size.width(),
                   container_size.height() + title_height_with_padding);
}

void WindowPreview::Layout(PassKey) {
  gfx::Rect content_rect = GetContentsBounds();

  gfx::Size title_size = title_->CalculatePreferredSize({});
  int title_height_with_padding =
      kTitleLineHeight + kTitleMarginTop + kTitleMarginBottom;
  int title_width =
      std::min(title_size.width(),
               content_rect.width() - kCloseButtonSize - kTitleMarginRight);
  title_->SetBoundsRect(gfx::Rect(content_rect.x(),
                                  content_rect.y() + kTitleMarginTop,
                                  title_width, kTitleLineHeight));

  close_button_->SetBoundsRect(
      gfx::Rect(content_rect.right() - kCloseButtonSize + kCloseButtonSideBleed,
                content_rect.y(), kCloseButtonSize, kCloseButtonSize));

  const gfx::Size container_size = GetPreviewContainerSize();
  gfx::Size mirror_size = preview_view_->CalculatePreferredSize({});
  float preview_ratio = static_cast<float>(mirror_size.width()) /
                        static_cast<float>(mirror_size.height());

  int preview_height = ShelfConfig::Get()->shelf_tooltip_preview_height();
  int preview_width = preview_height * preview_ratio;
  if (preview_ratio > ShelfConfig::Get()->shelf_tooltip_preview_max_ratio()) {
    // Very wide window.
    preview_width = ShelfConfig::Get()->shelf_tooltip_preview_max_width();
    preview_height =
        ShelfConfig::Get()->shelf_tooltip_preview_max_width() / preview_ratio;
  }

  // Center the actual preview over the container, horizontally and vertically.
  gfx::Point preview_offset_from_container(
      (container_size.width() - preview_width) / 2,
      (container_size.height() - preview_height) / 2);

  const int preview_container_top =
      content_rect.y() + title_height_with_padding;
  preview_container_view_->SetBoundsRect(
      gfx::Rect(content_rect.x(), preview_container_top, container_size.width(),
                container_size.height()));
  preview_view_->SetBoundsRect(
      gfx::Rect(content_rect.x() + preview_offset_from_container.x(),
                preview_container_top + preview_offset_from_container.y(),
                preview_width, preview_height));
}

bool WindowPreview::OnMousePressed(const ui::MouseEvent& event) {
  if (!preview_view_->bounds().Contains(event.location()))
    return false;

  aura::Window* target = preview_view_->window();
  if (target) {
    // The window might have been closed in the mean time.
    // TODO: Use WindowObserver to listen to when previewed windows are
    // being closed and remove this condition.
    wm::ActivateWindow(target);

    // This will have the effect of deleting this view.
    delegate_->OnPreviewActivated(this);
  }
  return true;
}

void WindowPreview::OnThemeChanged() {
  views::View::OnThemeChanged();
  const auto* color_provider = GetColorProvider();
  SkColor background_color =
      color_provider->GetColor(ui::kColorTooltipBackground);
  title_->SetEnabledColor(
      color_provider->GetColor(ui::kColorTooltipForeground));
  title_->SetBackgroundColor(background_color);

  // The background is not opaque, so we can't do subpixel rendering.
  title_->SetSubpixelRenderingEnabled(false);

  close_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(kOverviewWindowCloseIcon,
                                     kCloseButtonColor));
  close_button_->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  close_button_->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  close_button_->SetMinimumImageSize(
      gfx::Size(kCloseButtonImageSize, kCloseButtonImageSize));
}

gfx::Size WindowPreview::GetPreviewContainerSize() const {
  return gfx::Size(
      std::min(delegate_->GetMaxPreviewRatio() *
                   ShelfConfig::Get()->shelf_tooltip_preview_height(),
               static_cast<float>(
                   ShelfConfig::Get()->shelf_tooltip_preview_max_width())),
      ShelfConfig::Get()->shelf_tooltip_preview_height());
}

void WindowPreview::CloseButtonPressed() {
  // The window might have been closed in the mean time.
  // TODO: Use WindowObserver to listen to when previewed windows are
  // being closed and remove this condition.
  aura::Window* target = preview_view_->window();
  if (!target)
    return;
  window_util::CloseWidgetForWindow(target);

  // This will have the effect of deleting this view.
  delegate_->OnPreviewDismissed(this);
}

BEGIN_METADATA(WindowPreview)
END_METADATA

}  // namespace ash
