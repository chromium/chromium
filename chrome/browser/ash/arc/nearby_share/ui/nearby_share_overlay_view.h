// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_UI_NEARBY_SHARE_OVERLAY_VIEW_H_
#define CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_UI_NEARBY_SHARE_OVERLAY_VIEW_H_

#include "ui/views/layout/flex_layout_view.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class View;
}  // namespace views

namespace arc {

class NearbyShareOverlayView : public views::FlexLayoutView {
  METADATA_HEADER(NearbyShareOverlayView, views::FlexLayoutView)

 public:
  NearbyShareOverlayView(const NearbyShareOverlayView&) = delete;
  NearbyShareOverlayView& operator=(const NearbyShareOverlayView&) = delete;
  ~NearbyShareOverlayView() override;

  // Closes any existing overlay for |base_window|, then shows |child_view|.
  // Shrinks the |child_view| from its preferred width, if necessary, to
  // ensure there is sufficient horizontal margins when fitting inside
  // of |base_window|. Otherwise, |child_view|'s width is fixed at its
  // preferred width.
  static void Show(aura::Window* base_window, views::View* child_view);

  // Closes any overlay view on |base_window|.
  static void CloseOverlayOn(aura::Window* base_window);

  // views::View:
  void AddedToWidget() override;

 private:
  friend class NearbyShareOverlayViewTest;

  explicit NearbyShareOverlayView(views::View* child_view);

  const bool has_child_view_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_UI_NEARBY_SHARE_OVERLAY_VIEW_H_
