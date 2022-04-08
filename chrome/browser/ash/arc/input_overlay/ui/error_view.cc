// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/error_view.h"

#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"

namespace arc {
namespace input_overlay {

namespace {
// UI specs.
constexpr int kWidthPadding = 10;
constexpr int kMinHeight = 32;
constexpr int kSpaceToActionView = 8;
constexpr char kFontSytle[] = "Google Sans";
constexpr int kFontSize = 14;
constexpr SkColor kTextColor = gfx::kGoogleRed300;

}  // namespace

ErrorView::ErrorView(DisplayOverlayController* controller,
                     ActionView* view,
                     base::StringPiece text)
    : views::Label(base::UTF8ToUTF16(text)),
      display_overlay_controller_(controller) {
  DCHECK(display_overlay_controller_);
  if (display_overlay_controller_)
    display_overlay_controller_->RemoveEditErrorMsg();
  SetBackground(nullptr);
  SetFontList(gfx::FontList({kFontSytle}, gfx::Font::NORMAL, kFontSize,
                            gfx::Font::Weight::MEDIUM));
  SetAutoColorReadabilityEnabled(false);
  SetEnabledColor(kTextColor);

  auto preferred_size = GetPreferredSize();
  auto content_bounds = view->parent()->bounds();
  auto action_view_bounds = view->bounds();
  int x = action_view_bounds.x();
  if (content_bounds.width() - x < preferred_size.width())
    x = content_bounds.width() - preferred_size.width();
  x = std::max(0, x);

  int y = action_view_bounds.bottom() + kSpaceToActionView;
  if (content_bounds.height() - y < preferred_size.height())
    y = action_view_bounds.y() - preferred_size.height() - kSpaceToActionView;
  y = std::max(0, y);

  SetPosition(gfx::Point(x, y));
  SetSize(preferred_size);
}

ErrorView::~ErrorView() = default;

gfx::Size ErrorView::CalculatePreferredSize() const {
  auto size = Label::CalculatePreferredSize();
  size.set_width(size.width() + kWidthPadding);
  size.set_height(std::max(size.height(), kMinHeight));
  return size;
}

}  // namespace input_overlay
}  // namespace arc
