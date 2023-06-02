// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_MINI_VIEW_HEADER_VIEW_H_
#define ASH_WM_WINDOW_MINI_VIEW_HEADER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/wm/window_mini_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"

namespace views {
class Label;
class ImageView;
}  // namespace views

namespace ash {

// A view that represents the header for the window mini view. It contains an
// icon and a title label from the source window of the `window_mini_view_` and
// has a stroke at the bottom of it.
class ASH_EXPORT WindowMiniViewHeaderView : public views::BoxLayoutView {
 public:
  METADATA_HEADER(WindowMiniViewHeaderView);

  explicit WindowMiniViewHeaderView(WindowMiniView* window_mini_view);
  WindowMiniViewHeaderView(const WindowMiniViewHeaderView&) = delete;
  WindowMiniViewHeaderView& operator=(const WindowMiniViewHeaderView&) = delete;
  ~WindowMiniViewHeaderView() override;

  views::Label* title_label() { return title_label_; }
  views::View* icon_label_view() { return icon_label_view_; }

  void UpdateIconView(aura::Window* window);
  void UpdateTitleLabel(aura::Window* window);

 private:
  // The parent view of `this`, which is guaranteed not null during the lifetime
  // of `this`.
  raw_ptr<WindowMiniView, ExperimentalAsh> window_mini_view_;

  // A view that wraps up the icon and title label. Owned by the views
  // hierarchy.
  raw_ptr<views::BoxLayoutView, ExperimentalAsh> icon_label_view_;

  // Views for the icon and title. Owned by the views hierarchy.
  raw_ptr<views::Label, ExperimentalAsh> title_label_ = nullptr;
  raw_ptr<views::ImageView, ExperimentalAsh> icon_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_MINI_VIEW_HEADER_VIEW_H_
