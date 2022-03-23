// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_preview_view.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_base_view.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_highlight_item_border.h"
#include "ash/wm/workspace/backdrop_controller.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace_controller.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/cxx17_backports.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/accessibility/accessibility_paint_checks.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"

namespace ash {

namespace {

// In non-compact layouts, the height of the preview is a percentage of the
// total display height, with a max of |kDeskPreviewMaxHeight| dips and a min of
// |kDeskPreviewMinHeight| dips.
constexpr int kRootHeightDivider = 12;
constexpr int kRootHeightDividerForSmallScreen = 8;
constexpr int kDeskPreviewMaxHeight = 140;
constexpr int kDeskPreviewMinHeight = 48;
constexpr int kUseSmallerHeightDividerWidthThreshold = 600;

// The corner radius of the border in dips.
constexpr int kBorderCornerRadius = 6;

// The rounded corner radii, also in dips.
constexpr int kCornerRadius = 4;
constexpr gfx::RoundedCornersF kCornerRadii(kCornerRadius);

// Holds data about the original desk's layers to determine what we should do
// when we attempt to mirror those layers.
struct LayerData {
  // If true, the layer won't be mirrored in the desk's mirrored contents. For
  // example windows created by overview mode to hold the OverviewItemView,
  // or minimized windows' layers, should all be skipped.
  bool should_skip_layer = false;

  // If true, we will force the mirror layers to be visible even if the source
  // layers are not, and we will disable visibility change synchronization
  // between the source and mirror layers.
  // This is used, for example, for the desks container windows whose mirrors
  // should always be visible (even for inactive desks) to be able to see their
  // contents in the mini_views.
  bool should_force_mirror_visible = false;

  // If true, transformations will be cleared for this layer. This is used,
  // for example, for visible on all desk windows to clear their overview
  // transformation since they don't belong to inactive desks.
  bool should_clear_transform = false;
};

// Returns true if |window| can be shown in the desk's preview according to its
// multi-profile ownership status (i.e. can only be shown if it belongs to the
// active user).
bool CanShowWindowForMultiProfile(aura::Window* window) {
  aura::Window* window_to_check = window;
  // If |window| is a backdrop, check the window which has this backdrop
  // instead.
  WorkspaceController* workspace_controller =
      GetWorkspaceControllerForContext(window_to_check);
  if (workspace_controller) {
    BackdropController* backdrop_controller =
        workspace_controller->layout_manager()->backdrop_controller();
    if (backdrop_controller->backdrop_window() == window_to_check)
      window_to_check = backdrop_controller->window_having_backdrop();
  }

  return window_util::ShouldShowForCurrentUser(window_to_check);
}

// Returns the LayerData entry for |target_layer| in |layer_data|. Returns an
// empty LayerData struct if not found.
const LayerData GetLayerDataEntry(
    const base::flat_map<ui::Layer*, LayerData>& layers_data,
    ui::Layer* target_layer) {
  const auto iter = layers_data.find(target_layer);
  return iter == layers_data.end() ? LayerData{} : iter->second;
}

// Appends clones of all the visible on all desks windows' layers to
// |out_desk_container_children|. Should only be called if
// |visible_on_all_desks_windows| is not empty.
void AppendVisibleOnAllDesksWindowsToDeskLayer(
    const base::flat_set<aura::Window*>& visible_on_all_desks_windows,
    const base::flat_map<ui::Layer*, LayerData>& layers_data,
    std::vector<ui::Layer*>* out_desk_container_children) {
  DCHECK(!visible_on_all_desks_windows.empty());
  auto mru_windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kAllDesks);

  for (auto* window : visible_on_all_desks_windows) {
    const LayerData layer_data =
        GetLayerDataEntry(layers_data, window->layer());
    if (layer_data.should_skip_layer)
      continue;

    auto window_iter =
        std::find(mru_windows.begin(), mru_windows.end(), window);
    if (window_iter == mru_windows.end())
      continue;

    auto closest_window_below_iter = std::next(window_iter);
    while (closest_window_below_iter != mru_windows.end() &&
           !base::Contains(*out_desk_container_children,
                           (*closest_window_below_iter)->layer())) {
      // Find the closest window to |window| in the MRU tracker whose layer also
      // is in |out_desk_container_children|. This window will be used to
      // determine the stacking order of the visible on all desks window in the
      // preview view.
      closest_window_below_iter = std::next(closest_window_below_iter);
    }

    auto insertion_point_iter =
        closest_window_below_iter == mru_windows.end()
            ? out_desk_container_children->begin()
            : std::next(std::find(out_desk_container_children->begin(),
                                  out_desk_container_children->end(),
                                  (*closest_window_below_iter)->layer()));
    out_desk_container_children->insert(insertion_point_iter, window->layer());
  }
}

// Recursively mirrors |source_layer| and its children and adds them as children
// of |parent|, taking into account the given |layers_data|. If the layer data
// of |source_layer| has |should_clear_transform| set to true, the transforms of
// its mirror layers will be reset to identity.
void MirrorLayerTree(ui::Layer* source_layer,
                     ui::Layer* parent,
                     const base::flat_map<ui::Layer*, LayerData>& layers_data,
                     const base::flat_set<aura::Window*>&
                         visible_on_all_desks_windows_to_mirror) {
  const LayerData layer_data = GetLayerDataEntry(layers_data, source_layer);
  if (layer_data.should_skip_layer)
    return;

  auto* mirror = source_layer->Mirror().release();
  parent->Add(mirror);

  std::vector<ui::Layer*> children = source_layer->children();
  if (!visible_on_all_desks_windows_to_mirror.empty()) {
    // Windows that are visible on all desks should show up in each desk
    // preview so for inactive desks, we need to append the layers of visible on
    // all desks windows.
    AppendVisibleOnAllDesksWindowsToDeskLayer(
        visible_on_all_desks_windows_to_mirror, layers_data, &children);
  }
  for (auto* child : children) {
    // Visible on all desks windows only needed to be added to the subtree once
    // so use an empty set for subsequent calls.
    MirrorLayerTree(child, mirror, layers_data,
                    base::flat_set<aura::Window*>());
  }

  mirror->set_sync_bounds_with_source(true);
  if (layer_data.should_force_mirror_visible) {
    mirror->SetVisible(true);
    mirror->SetOpacity(1);
    mirror->set_sync_visibility_with_source(false);
  }

  if (layer_data.should_clear_transform)
    mirror->SetTransform(gfx::Transform());
}

// Gathers the needed data about the layers in the subtree rooted at the layer
// of the given |window|, and fills |out_layers_data|.
void GetLayersData(aura::Window* window,
                   base::flat_map<ui::Layer*, LayerData>* out_layers_data) {
  auto& layer_data = (*out_layers_data)[window->layer()];

  // Windows may be explicitly set to be skipped in mini_views such as those
  // created for overview mode purposes.
  // TODO(afakhry): Exclude exo's root surface, since it's a place holder and
  // doesn't have any content. See `exo::SurfaceTreeHost::SetRootSurface()`.
  if (window->GetProperty(kHideInDeskMiniViewKey)) {
    layer_data.should_skip_layer = true;
    return;
  }

  // Minimized windows should not show up in the mini_view.
  auto* window_state = WindowState::Get(window);
  if (window_state && window_state->IsMinimized()) {
    layer_data.should_skip_layer = true;
    return;
  }

  if (!CanShowWindowForMultiProfile(window)) {
    layer_data.should_skip_layer = true;
    return;
  }

  // Windows transformed into position in the overview mode grid should be
  // mirrored and the transforms of the mirrored layers should be reset to
  // identity.
  if (window->GetProperty(kForceVisibleInMiniViewKey))
    layer_data.should_force_mirror_visible = true;

  // Visible on all desks windows aren't children of inactive desk's container
  // so mark them explicitly to clear overview transforms. Additionally, windows
  // in overview mode are transformed into their positions in the grid, but we
  // want to show a preview of the windows in their untransformed state.
  if (desks_util::IsWindowVisibleOnAllWorkspaces(window) ||
      desks_util::IsDeskContainer(window->parent())) {
    layer_data.should_clear_transform = true;
  }

  for (auto* child : window->children())
    GetLayersData(child, out_layers_data);
}

}  // namespace

// -----------------------------------------------------------------------------
// DeskPreviewView

DeskPreviewView::DeskPreviewView(PressedCallback callback,
                                 DeskMiniView* mini_view)
    : views::Button(std::move(callback)),
      mini_view_(mini_view),
      wallpaper_preview_(new DeskWallpaperPreview),
      desk_mirrored_contents_view_(new views::View),
      force_occlusion_tracker_visible_(
          std::make_unique<aura::WindowOcclusionTracker::ScopedForceVisible>(
              mini_view->GetDeskContainer())),
      shadow_(std::make_unique<SystemShadow>(kDefaultShadowType)) {
  DCHECK(mini_view_);

  SetFocusPainter(nullptr);
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);
  SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);

  // TODO(crbug.com/1218186): Remove this, this is in place temporarily to be
  // able to submit accessibility checks, but this focusable View needs to
  // add a name so that the screen reader knows what to announce.
  SetProperty(views::kSkipAccessibilityPaintChecks, true);

  SetPaintToLayer(ui::LAYER_TEXTURED);
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(false);

  shadow_->SetRoundedCornerRadius(kCornerRadius);
  layer()->Add(shadow_->layer());

  wallpaper_preview_->SetPaintToLayer();
  auto* wallpaper_preview_layer = wallpaper_preview_->layer();
  wallpaper_preview_layer->SetFillsBoundsOpaquely(false);
  wallpaper_preview_layer->SetRoundedCornerRadius(kCornerRadii);
  wallpaper_preview_layer->SetIsFastRoundedCorner(true);
  AddChildView(wallpaper_preview_);

  desk_mirrored_contents_view_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  ui::Layer* contents_view_layer = desk_mirrored_contents_view_->layer();
  contents_view_layer->SetMasksToBounds(true);
  contents_view_layer->SetName("Desk mirrored contents view");
  contents_view_layer->SetRoundedCornerRadius(kCornerRadii);
  contents_view_layer->SetIsFastRoundedCorner(true);
  AddChildView(desk_mirrored_contents_view_);

  auto border = std::make_unique<WmHighlightItemBorder>(kBorderCornerRadius);
  border_ptr_ = border.get();
  SetBorder(std::move(border));

  RecreateDeskContentsMirrorLayers();
}

DeskPreviewView::~DeskPreviewView() = default;

// static
int DeskPreviewView::GetHeight(aura::Window* root) {
  DCHECK(root);
  DCHECK(root->IsRootWindow());

  const int height_divider =
      root->bounds().width() <= kUseSmallerHeightDividerWidthThreshold
          ? kRootHeightDividerForSmallScreen
          : kRootHeightDivider;
  return base::clamp(root->bounds().height() / height_divider,
                     kDeskPreviewMinHeight, kDeskPreviewMaxHeight);
}

void DeskPreviewView::SetBorderColor(SkColor color) {
  border_ptr_->set_color(color);
  SchedulePaint();
}

void DeskPreviewView::OnRemovingDesk() {
  // Since the mini view has a remove animation, we don't want this desk preview
  // to be pressed while it's animating. The desk will have already be removed
  // after this.
  SetCallback(views::Button::PressedCallback());
}

void DeskPreviewView::RecreateDeskContentsMirrorLayers() {
  auto* desk_container = mini_view_->GetDeskContainer();
  DCHECK(desk_container);
  DCHECK(desk_container->layer());

  // Mirror the layer tree of the desk container.
  auto mirrored_content_root_layer =
      std::make_unique<ui::Layer>(ui::LAYER_NOT_DRAWN);
  mirrored_content_root_layer->SetName("mirrored contents root layer");
  base::flat_map<ui::Layer*, LayerData> layers_data;
  GetLayersData(desk_container, &layers_data);

  base::flat_set<aura::Window*> visible_on_all_desks_windows_to_mirror;
  if (!desks_util::IsActiveDeskContainer(desk_container)) {
    // Since visible on all desks windows reside on the active desk, only mirror
    // them in the layer tree if |this| is not the preview view for the active
    // desk.
    visible_on_all_desks_windows_to_mirror =
        Shell::Get()->desks_controller()->GetVisibleOnAllDesksWindowsOnRoot(
            mini_view_->root_window());
    for (auto* window : visible_on_all_desks_windows_to_mirror)
      GetLayersData(window, &layers_data);
  }

  auto* desk_container_layer = desk_container->layer();
  MirrorLayerTree(desk_container_layer, mirrored_content_root_layer.get(),
                  layers_data, visible_on_all_desks_windows_to_mirror);

  // Add the root of the mirrored layer tree as a child of the
  // |desk_mirrored_contents_view_|'s layer.
  ui::Layer* contents_view_layer = desk_mirrored_contents_view_->layer();
  contents_view_layer->Add(mirrored_content_root_layer.get());

  // Take ownership of the mirrored layer tree.
  desk_mirrored_contents_layer_tree_owner_ =
      std::make_unique<ui::LayerTreeOwner>(
          std::move(mirrored_content_root_layer));

  Layout();
}

const char* DeskPreviewView::GetClassName() const {
  return "DeskPreviewView";
}

void DeskPreviewView::Layout() {
  const gfx::Rect bounds = GetContentsBounds();
  shadow_->SetContentBounds(bounds);
  wallpaper_preview_->SetBoundsRect(bounds);
  desk_mirrored_contents_view_->SetBoundsRect(bounds);

  // The desk's contents mirrored layer needs to be scaled down so that it fits
  // exactly in the center of the view.
  const auto root_size = mini_view_->root_window()->layer()->size();
  const gfx::Vector2dF scale{
      static_cast<float>(bounds.width()) / root_size.width(),
      static_cast<float>(bounds.height()) / root_size.height()};
  wallpaper_preview_->set_centered_layout_image_scale(scale);
  gfx::Transform transform;
  transform.Scale(scale.x(), scale.y());
  ui::Layer* desk_mirrored_contents_layer =
      desk_mirrored_contents_layer_tree_owner_->root();
  DCHECK(desk_mirrored_contents_layer);
  desk_mirrored_contents_layer->SetTransform(transform);

  Button::Layout();
}

bool DeskPreviewView::OnMousePressed(const ui::MouseEvent& event) {
  // If we have a right click and the `kDesksCloseAll` feature is enabled, we
  // should open the context menu.
  if (features::IsDesksCloseAllEnabled() && event.IsRightMouseButton()) {
    DeskNameView::CommitChanges(GetWidget());
    mini_view_->OpenContextMenu();
  } else {
    mini_view_->owner_bar()->HandlePressEvent(mini_view_, event);
  }

  return Button::OnMousePressed(event);
}

bool DeskPreviewView::OnMouseDragged(const ui::MouseEvent& event) {
  mini_view_->owner_bar()->HandleDragEvent(mini_view_, event);
  return Button::OnMouseDragged(event);
}

void DeskPreviewView::OnMouseReleased(const ui::MouseEvent& event) {
  if (!mini_view_->owner_bar()->HandleReleaseEvent(mini_view_, event))
    Button::OnMouseReleased(event);
}

void DeskPreviewView::OnGestureEvent(ui::GestureEvent* event) {
  DesksBarView* owner_bar = mini_view_->owner_bar();

  switch (event->type()) {
    // Only long press can trigger drag & drop.
    case ui::ET_GESTURE_LONG_PRESS:
      // TODO(crbug.com/1308780): Need to figure out how we can still maintain
      // drag functionality while allowing long press to open the context menu.
      owner_bar->HandleLongPressEvent(mini_view_, *event);
      event->SetHandled();
      break;
    case ui::ET_GESTURE_SCROLL_BEGIN:
      [[fallthrough]];
    case ui::ET_GESTURE_SCROLL_UPDATE:
      owner_bar->HandleDragEvent(mini_view_, *event);
      event->SetHandled();
      break;
    case ui::ET_GESTURE_END:
      if (owner_bar->HandleReleaseEvent(mini_view_, *event))
        event->SetHandled();
      break;
    default:
      break;
  }

  if (!event->handled())
    Button::OnGestureEvent(event);
}

}  // namespace ash
