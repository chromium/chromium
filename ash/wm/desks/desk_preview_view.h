// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_PREVIEW_VIEW_H_
#define ASH_WM_DESKS_DESK_PREVIEW_VIEW_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/views/view.h"

namespace ui {
class LayerTreeOwner;
}  // namespace ui

namespace ash {

class DesksBarItemBorder;
class DeskMiniView;
class WallpaperBaseView;

// A view that shows the contents of the corresponding desk in its mini_view.
// This view has the following layer hierarchy:
//
//                +-----+
//                |  <--+------  This view's layer.
//                +-----+
//              /    |    \     ----->>>>> Higher in Z-order.
//             /     |     \
//     +-----+    +-----+    +-----+
//     |     |    |     |    |     |
//     +-----+    +-----+    +-----+
//        ^          ^          ^    \
//        |          |          |     \ +-----+
//        |          |          |       |     |
//        |          |          |       +-----+
//        |          |          |          ^
//        |          |          |          |
//        |          |          |    The root layer of the desk's mirrored
//        |          |          |    contents layer tree. This tree is owned by
//        |          |          |    `desk_mirrored_contents_layer_tree_owner_`.
//        |          |          |
//        |          |          |
//        |          |     `desk_mirrored_contents_view_`'s layer: Will be the
//        |          |      parent layer of the desk's contents mirrored layer
//        |          |      tree.
//        |          |
//        |          |
//        |     `wallpaper_preview_`'s layer: On which the wallpaper is painted
//        |      without the dimming and blur that overview mode adds.
//        |
//        |
//     `shadow_layer_`: A layer that paints a shadow behind this view.
//
// Note that both |desk_mirrored_contents_view_| and |wallpaper_preview_| paint
// to layers with rounded corners. In order to use the fast rounded corners
// implementation we must make them sibling layers, rather than one being a
// descendant of the other. Otherwise, this will trigger a render surface.
class ASH_EXPORT DeskPreviewView : public views::View {
 public:
  explicit DeskPreviewView(DeskMiniView* mini_view);
  ~DeskPreviewView() override;

  // Returns the height of the DeskPreviewView based on whether the |compact|
  // small screens layout is used or not.
  static int GetHeight(bool compact);

  void SetBorderColor(SkColor color);

  // This should be called when there is a change in the desk contents so that
  // we can recreate the mirrored layer tree.
  void RecreateDeskContentsMirrorLayers();

  // views::View:
  const char* GetClassName() const override;
  void Layout() override;

 private:
  class ShadowRenderer;

  DeskMiniView* const mini_view_;

  // A view that paints the wallpaper in the mini_view. It avoids the dimming
  // and blur overview mode adds to the original wallpaper. Owned by the views
  // hierarchy.
  using DeskWallpaperPreview = WallpaperBaseView;
  DeskWallpaperPreview* wallpaper_preview_;

  // A view whose layer will act as the parent of desk's mirrored contents layer
  // tree. Owned by the views hierarchy.
  views::View* desk_mirrored_contents_view_;

  // Owned by this View via `View::border_`. This is just a convenient pointer
  // to it.
  DesksBarItemBorder* border_ptr_;

  // Owns the layer tree of the desk's contents mirrored layers.
  std::unique_ptr<ui::LayerTreeOwner> desk_mirrored_contents_layer_tree_owner_;

  // Forces the occlusion tracker to treat the associated desk's container
  // window to be visible (even though in reality it may not be when the desk is
  // inactive). This is needed since we may be mirroring the contents of an
  // inactive desk which contains a playing video, which would not show at all
  // in the mirrored contents if we didn't ask the occlusion tracker to consider
  // the desk container to be visible.
  std::unique_ptr<aura::WindowOcclusionTracker::ScopedForceVisible>
      force_occlusion_tracker_visible_;

  ui::Layer shadow_layer_;
  std::unique_ptr<ShadowRenderer> shadow_delegate_;

  DISALLOW_COPY_AND_ASSIGN(DeskPreviewView);
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_PREVIEW_VIEW_H_
