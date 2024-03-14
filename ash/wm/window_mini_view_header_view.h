// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_MINI_VIEW_HEADER_VIEW_H_
#define ASH_WM_WINDOW_MINI_VIEW_HEADER_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/layout/box_layout_view.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class ImageView;
class Label;
class View;
}  // namespace views

namespace ash {

class WindowMiniView;

// A view that represents the header for the window mini view. It contains an
// icon and a title label from the source window of the `window_mini_view_` and
// has a stroke at the bottom of it.
class ASH_EXPORT WindowMiniViewHeaderView : public views::BoxLayoutView {
  METADATA_HEADER(WindowMiniViewHeaderView, views::BoxLayoutView)

 public:
  explicit WindowMiniViewHeaderView(WindowMiniView* window_mini_view);
  WindowMiniViewHeaderView(const WindowMiniViewHeaderView&) = delete;
  WindowMiniViewHeaderView& operator=(const WindowMiniViewHeaderView&) = delete;
  ~WindowMiniViewHeaderView() override;

  views::Label* title_label() { return title_label_; }
  views::View* icon_label_view() { return icon_label_view_; }

  void UpdateIconView(aura::Window* window);
  void UpdateTitleLabel(aura::Window* window);

  // Refreshes the rounded corners on `this` by recreating the background view.
  // Please note that there might be minor pixel difference if the rounded
  // corner is set on the layer of this since the way to draw the rounded
  // corners is different which may fail the pixel test
  // (WmPixelDiffTest.WindowCycleBasic).
  void RefreshHeaderViewRoundedCorners();

  void SetHeaderViewRoundedCornerRadius(
      gfx::RoundedCornersF& header_view_rounded_corners);

  // Resets the preset rounded corners values i.e.
  // `header_view_rounded_corners_`.
  void ResetRoundedCorners();

 private:
  // The parent view of `this`, which is guaranteed not null during the lifetime
  // of `this`.
  raw_ptr<WindowMiniView> window_mini_view_;

  // A view that wraps up the icon and title label. Owned by the views
  // hierarchy.
  raw_ptr<views::BoxLayoutView> icon_label_view_;

  // Views for the icon and title. Owned by the views hierarchy.
  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<views::ImageView> icon_view_ = nullptr;

  std::optional<gfx::RoundedCornersF> header_view_rounded_corners_;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_MINI_VIEW_HEADER_VIEW_H_
