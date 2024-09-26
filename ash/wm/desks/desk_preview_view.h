// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_PREVIEW_VIEW_H_
#define ASH_WM_DESKS_DESK_PREVIEW_VIEW_H_

#include <memory>
#include <optional>

#include "ash/ash_export.h"
#include "ash/style/system_shadow.h"
#include "ash/wm/desks/window_occlusion_calculator.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace ui {
class LayerTreeOwner;
}  // namespace ui

namespace ash {

class DeskMiniView;
class WallpaperBaseView;

// A view that shows the contents of the corresponding desk in its mini_view.
// This view has the following layer hierarchy:
//
//                +---------------------------+
//                |             <-------------+------  This view's layer.
//                +---------------------------+
//                       |               \  ----->>>>> Higher in Z-order.
//                       |                \
//                    +-----+               +-----+
//                    |     |               |     |
//                    +-----+               +-----+
//                       ^    \                ^
//                       |     \ +-----+       |
//                       |       |     |       |
//                       |       +-----+       |
//                       |          ^          |
//                       |          |   `highlight_overlay_`'s layer:
//                       |          |   A solid color layer that is
//                       |          |   visible when `mini_view_`'s
//                       |          |   `DeskActionContextMenu` is open.
//                       |          |
//                       |          |
//                       |    The root layer of the desk's mirrored
//                       |    contents layer tree. This tree is owned by
//                       |    `desk_mirrored_contents_layer_tree_owner_`.
//                       |
//                       |
//             `desk_mirrored_contents_view_`'s layer: Will be the
//              parent layer of the desk's contents mirrored layer tree.
//
// Note that `desk_mirrored_contents_view_` and `highlight_overlay_` paint to
// layers with rounded corners. In order to use the fast rounded corners
// implementation we must make them sibling layers, rather than one being a
// descendant of the other. Otherwise, this will trigger a render surface.
class ASH_EXPORT DeskPreviewView : public views::Button,
                                   public WindowOcclusionCalculator::Observer {
  METADATA_HEADER(DeskPreviewView, views::Button)

 public:
  DeskPreviewView(
      PressedCallback callback,
      DeskMiniView* mini_view,
      base::WeakPtr<WindowOcclusionCalculator> window_occlusion_calculator);

  DeskPreviewView(const DeskPreviewView&) = delete;
  DeskPreviewView& operator=(const DeskPreviewView&) = delete;

  ~DeskPreviewView() override;

  static constexpr SystemShadow::Type kDefaultShadowType =
      SystemShadow::Type::kElevation4;
  static constexpr SystemShadow::Type kDraggedShadowType =
      SystemShadow::Type::kElevation12;

  // Returns the height of the DeskPreviewView, which is a function of the
  // |root| window's height.
  static int GetHeight(aura::Window* root);

  SystemShadow* shadow() const { return shadow_.get(); }

  std::optional<ui::ColorId> focus_color_id() { return focus_color_id_; }
  void set_focus_color_id(std::optional<ui::ColorId> focus_color_id) {
    focus_color_id_ = focus_color_id;
  }

  // Sets the visibility of `highlight_overlay_` to `visible`. If `visible` is
  // true, this `DeskPreviewView` becomes highlighted.
  void SetHighlightOverlayVisibility(bool visible);

  // This should be called when there is a change in the desk contents so that
  // we can recreate the mirrored layer tree.
  void RecreateDeskContentsMirrorLayers();

  // Performs close action for this preview. when `primary_action` is true, it's
  // merge-desk action; otherwise it's close-all action.
  void Close(bool primary_action);

  // Performs swap action for this preview. When `right` is true, it swaps with
  // its right preview; otherwise it swaps with its left preview.
  void Swap(bool right);

  // Updates accessible name for this desk preview.
  void UpdateAccessibleName();

  // Called when the user exits overview by using 3-finger vertical trackpad
  // swipes.
  void AcceptSelection();

  // For metrics purposes only. Returns the total number of layers mirrored
  // to preview the desk's windows.
  size_t GetNumLayersMirrored() const;

  // views::Button:
  void Layout(PassKey) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnThemeChanged() override;
  void OnFocus() override;
  void OnBlur() override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

  // WindowOcclusionCalculator::Observer:
  void OnWindowOcclusionChanged(aura::Window* window) override;

 private:
  friend class DesksTestApi;

  const raw_ptr<DeskMiniView, LeakedDanglingUntriaged> mini_view_;
  const base::WeakPtr<WindowOcclusionCalculator> window_occlusion_calculator_;

  // A view that paints the wallpaper in the mini_view. It avoids the dimming
  // and blur overview mode adds to the original wallpaper. Owned by the views
  // hierarchy.
  using DeskWallpaperPreview = WallpaperBaseView;
  raw_ptr<DeskWallpaperPreview> wallpaper_preview_;

  // A view whose layer will act as the parent of desk's mirrored contents layer
  // tree. Owned by the views hierarchy.
  raw_ptr<views::View> desk_mirrored_contents_view_;

  // An overlay that becomes visible on top of the
  // `desk_mirrored_contents_view_` when the `mini_view_`'s
  // `DeskActionContextMenu` is active. Owned by the views hierarchy.
  raw_ptr<views::View> highlight_overlay_ = nullptr;

  // Owns the layer tree of the desk's contents mirrored layers.
  std::unique_ptr<ui::LayerTreeOwner> desk_mirrored_contents_layer_tree_owner_;

  // Forces the occlusion tracker to treat the associated desk's container
  // window to be visible (even though in reality it may not be when the desk is
  // inactive). This is needed since we may be mirroring the contents of an
  // inactive desk which contains a playing video, which would not show at all
  // in the mirrored contents if we didn't ask the occlusion tracker to consider
  // the desk container to be visible. We need a separate scoped force on the
  // floated window, if there is one, as it isn't parented to the desk
  // container.
  aura::WindowOcclusionTracker::ScopedForceVisible
      force_desk_occlusion_tracker_visible_;
  std::optional<aura::WindowOcclusionTracker::ScopedForceVisible>
      force_float_occlusion_tracker_visible_;

  std::unique_ptr<SystemShadow> shadow_;

  std::optional<ui::ColorId> focus_color_id_;

  base::WeakPtrFactory<DeskPreviewView> recreate_mirror_layers_weak_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_PREVIEW_VIEW_H_
