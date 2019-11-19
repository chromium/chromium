// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_preview_view.h"

#include <memory>

#include "ash/multi_user/multi_user_window_manager_impl.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/wallpaper/wallpaper_base_view.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_bar_item_border.h"
#include "ash/wm/window_state.h"
#include "base/containers/flat_map.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/border.h"

namespace ash {

namespace {

// The height of the preview view in dips.
constexpr int kDeskPreviewHeight = 64;
constexpr int kDeskPreviewHeightInCompactLayout = 48;

// The corner radius of the border in dips.
constexpr int kBorderCornerRadius = 6;

// The rounded corner radii, also in dips.
constexpr int kCornerRadius = 4;
constexpr gfx::RoundedCornersF kCornerRadii(kCornerRadius);

constexpr int kShadowElevation = 4;

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
};

// Returns true if |window| can be shown in the desk's preview according to its
// multi-profile ownership status (i.e. can only be shown if it belongs to the
// active user).
bool CanShowWindowForMultiProfile(aura::Window* window) {
  MultiUserWindowManager* multi_user_window_manager =
      MultiUserWindowManagerImpl::Get();
  if (!multi_user_window_manager)
    return true;

  const AccountId account_id =
      multi_user_window_manager->GetUserPresentingWindow(window);
  // An empty account ID is returned if the window is presented for all users.
  if (!account_id.is_valid())
    return true;

  return account_id == multi_user_window_manager->CurrentAccountId();
}

// Recursively mirrors |source_layer| and its children and adds them as children
// of |parent|, taking into account the given |layers_data|.
// The transforms of the mirror layers of the direct children of
// |desk_container_layer| will be reset to identity.
void MirrorLayerTree(ui::Layer* desk_container_layer,
                     ui::Layer* source_layer,
                     ui::Layer* parent,
                     const base::flat_map<ui::Layer*, LayerData>& layers_data) {
  const auto iter = layers_data.find(source_layer);
  const LayerData layer_data =
      iter == layers_data.end() ? LayerData{} : iter->second;
  if (layer_data.should_skip_layer)
    return;

  auto* mirror = source_layer->Mirror().release();
  parent->Add(mirror);

  for (auto* child : source_layer->children())
    MirrorLayerTree(desk_container_layer, child, mirror, layers_data);

  mirror->set_sync_bounds_with_source(true);
  if (layer_data.should_force_mirror_visible) {
    mirror->SetVisible(true);
    mirror->SetOpacity(1);
    mirror->set_sync_visibility_with_source(false);
  }

  // Windows in overview mode are transformed into their positions in the grid,
  // but we want to show a preview of the windows in their untransformed state
  // outside of overview mode.
  if (source_layer->parent() == desk_container_layer)
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

  for (auto* child : window->children())
    GetLayersData(child, out_layers_data);
}

}  // namespace

// -----------------------------------------------------------------------------
// DeskPreviewView::ShadowRenderer

// Layer delegate which handles drawing a shadow around DeskPreviewView.
class DeskPreviewView::ShadowRenderer : public ui::LayerDelegate {
 public:
  ShadowRenderer()
      : shadow_values_(gfx::ShadowValue::MakeMdShadowValues(kShadowElevation)) {
  }

  ~ShadowRenderer() override = default;

  gfx::Rect GetPaintedBounds() const {
    gfx::Rect total_rect(bounds_);
    total_rect.Inset(gfx::ShadowValue::GetMargin(shadow_values_));
    return total_rect;
  }

  void set_bounds(const gfx::Rect& bounds) { bounds_ = bounds; }

 private:
  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    ui::PaintRecorder recorder(context, bounds_.size());

    cc::PaintFlags shadow_flags;
    shadow_flags.setAntiAlias(true);
    shadow_flags.setLooper(gfx::CreateShadowDrawLooper(shadow_values_));

    const gfx::Rect rrect_bounds =
        bounds_ - GetPaintedBounds().OffsetFromOrigin();
    const auto r_rect = SkRRect::MakeRectXY(gfx::RectToSkRect(rrect_bounds),
                                            kCornerRadius, kCornerRadius);
    recorder.canvas()->sk_canvas()->clipRRect(r_rect, SkClipOp::kDifference,
                                              /*do_anti_alias=*/true);
    recorder.canvas()->sk_canvas()->drawRRect(r_rect, shadow_flags);
  }

  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  gfx::Rect bounds_;
  const gfx::ShadowValues shadow_values_;

  DISALLOW_COPY_AND_ASSIGN(ShadowRenderer);
};

// -----------------------------------------------------------------------------
// DeskPreviewView

DeskPreviewView::DeskPreviewView(DeskMiniView* mini_view)
    : mini_view_(mini_view),
      wallpaper_preview_(new DeskWallpaperPreview),
      desk_mirrored_contents_view_(new views::View),
      force_occlusion_tracker_visible_(
          std::make_unique<aura::WindowOcclusionTracker::ScopedForceVisible>(
              mini_view->GetDeskContainer())),
      shadow_delegate_(std::make_unique<ShadowRenderer>()) {
  DCHECK(mini_view_);

  SetPaintToLayer(ui::LAYER_TEXTURED);
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(false);

  shadow_layer_.SetFillsBoundsOpaquely(false);
  layer()->Add(&shadow_layer_);
  shadow_layer_.set_delegate(shadow_delegate_.get());

  wallpaper_preview_->SetPaintToLayer();
  auto* wallpaper_preview_layer = wallpaper_preview_->layer();
  wallpaper_preview_layer->SetFillsBoundsOpaquely(false);
  wallpaper_preview_layer->SetRoundedCornerRadius(kCornerRadii);
  wallpaper_preview_layer->SetIsFastRoundedCorner(true);
  AddChildView(wallpaper_preview_);

  desk_mirrored_contents_view_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  ui::Layer* contents_view_layer = desk_mirrored_contents_view_->layer();
  contents_view_layer->SetMasksToBounds(true);
  contents_view_layer->set_name("Desk mirrored contents view");
  contents_view_layer->SetRoundedCornerRadius(kCornerRadii);
  contents_view_layer->SetIsFastRoundedCorner(true);
  AddChildView(desk_mirrored_contents_view_);

  auto border = std::make_unique<DesksBarItemBorder>(kBorderCornerRadius);
  border_ptr_ = border.get();
  SetBorder(std::move(border));

  RecreateDeskContentsMirrorLayers();
}

DeskPreviewView::~DeskPreviewView() = default;

// static
int DeskPreviewView::GetHeight(bool compact) {
  return compact ? kDeskPreviewHeightInCompactLayout : kDeskPreviewHeight;
}

void DeskPreviewView::SetBorderColor(SkColor color) {
  border_ptr_->set_color(color);
  SchedulePaint();
}

void DeskPreviewView::RecreateDeskContentsMirrorLayers() {
  auto* desk_container = mini_view_->GetDeskContainer();
  DCHECK(desk_container);
  DCHECK(desk_container->layer());

  // Mirror the layer tree of the desk container.
  auto mirrored_content_root_layer =
      std::make_unique<ui::Layer>(ui::LAYER_NOT_DRAWN);
  mirrored_content_root_layer->set_name("mirrored contents root layer");
  base::flat_map<ui::Layer*, LayerData> layers_data;
  GetLayersData(desk_container, &layers_data);
  auto* desk_container_layer = desk_container->layer();
  MirrorLayerTree(desk_container_layer, desk_container_layer,
                  mirrored_content_root_layer.get(), layers_data);

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
  gfx::Rect bounds = GetContentsBounds();
  shadow_delegate_->set_bounds(bounds);
  shadow_layer_.SetBounds(shadow_delegate_->GetPaintedBounds());
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
}

}  // namespace ash
