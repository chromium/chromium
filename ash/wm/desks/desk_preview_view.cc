// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_preview_view.h"

#include <algorithm>
#include <functional>
#include <utility>

#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/style_util.h"
#include "ash/wallpaper/views/wallpaper_base_view.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/backdrop_controller.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace_controller.h"
#include "base/containers/adapters.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/trace_event/trace_event.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/views/accessibility/view_accessibility.h"
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

// The rounded corner radii, also in dips.
constexpr float kCornerRadiusInDips = 8;
constexpr gfx::RoundedCornersF kCornerRadius(kCornerRadiusInDips);

// Used for painting the highlight when the context menu is open.
constexpr float kHighlightTransparency = 0.3f * 0xFF;

// Applies rounded corner clipping to the canvas before the wallpaper is
// painted. This is several milliseconds faster on low-end devices than giving
// the wallpaper its own layer and applying rounded corners to the layer.
class WallpaperRoundedCornerView : public WallpaperBaseView {
 public:
  WallpaperRoundedCornerView() = default;
  WallpaperRoundedCornerView(const WallpaperRoundedCornerView&) = delete;
  WallpaperRoundedCornerView& operator=(const WallpaperRoundedCornerView&) =
      delete;
  ~WallpaperRoundedCornerView() override = default;

  void OnPaint(gfx::Canvas* canvas) override {
    canvas->sk_canvas()->clipRRect(
        SkRRect::MakeRectXY(gfx::RectToSkRect(GetContentsBounds()),
                            kCornerRadiusInDips, kCornerRadiusInDips),
        /*do_anti_alias=*/true);
    WallpaperBaseView::OnPaint(canvas);
  }
};

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

// Get the z-order of all-desk `window` in `desk` for `root`. If it does not
// exist, then nullopt is returned. Please note, the z-order information is
// retrieved from the stored stacking data of `desk` for all-desk windows.
std::optional<size_t> GetWindowZOrderForDeskAndRoot(const aura::Window* window,
                                                    const Desk* desk,
                                                    const aura::Window* root) {
  const auto& adw_by_root = desk->all_desk_window_stacking();

  if (auto it = adw_by_root.find(root); it != adw_by_root.end()) {
    for (auto& adw : it->second) {
      if (adw.window == window)
        return adw.order;
    }
  }

  return std::nullopt;
}

// Returns the total number of layers that are descendants of `root`.
size_t GetNumDescendants(ui::Layer* root) {
  size_t num_descendants = root->children().size();
  for (const auto& child : root->children()) {
    num_descendants += GetNumDescendants(child);
  }
  return num_descendants;
}

// Recursively mirrors `source_layer` and its children and adds them as children
// of `parent`, taking into account the given |layers_data|. If the layer data
// of `source_layer` has `should_clear_transform` set to true, the transforms of
// its mirror layers will be reset to identity.
void MirrorLayerTree(
    ui::Layer* source_layer,
    ui::Layer* parent,
    const base::flat_map<ui::Layer*, LayerData>& layers_data,
    const base::flat_set<aura::Window*>& visible_on_all_desks_windows_to_mirror,
    aura::Window* desk_container) {
  const LayerData layer_data = GetLayerDataEntry(layers_data, source_layer);
  if (layer_data.should_skip_layer)
    return;

  auto* mirror = source_layer->Mirror().release();
  parent->Add(mirror);

  // Calculate child layers.
  std::vector<raw_ptr<ui::Layer, VectorExperimental>> children;
  if (visible_on_all_desks_windows_to_mirror.empty()) {
    // Without all desk windows, there is no need to reorder layers, just use
    // them as is.
    children = source_layer->children();
  } else {
    //
    // With all desk windows, they need to be inserted to the expected place.
    //
    // There are 3 cases.
    //    1. non-adw windows
    //      - These come ordered in `source_layer->children()`.
    //    2. adw windows with existing z-order in target desk
    //      - Target desk z-order should be used to compare between these adw
    //      - windows and other windows in target desk. As z-order values are
    //      - always different, there is no need to think about tie.
    //    3. adw windows without z-order in target desk
    //      - It should be put on top, but to break the tie among these adw
    //      - windows, z-order values from active desk should be considered.
    //
    // In order to do that, we use target desk z-order as the primary key,
    // active desk z-order as secondary key, and sort all windows/layers in a
    // descending order.
    //

    auto mru_windows =
        Shell::Get()->mru_window_tracker()->BuildMruWindowList(kAllDesks);
    const Desk* desk = desks_util::GetDeskForContext(desk_container);
    aura::Window* root = desk_container->GetRootWindow();

    // Define what to use for layer ordering.
    struct LayerOrderData {
      raw_ptr<ui::Layer> layer;
      // z-order in target desk.
      size_t primary_key;
      // z-order in active desk.
      size_t secondary_key;
    };
    std::vector<LayerOrderData> layer_orders;
    base::flat_set<size_t> primary_key_taken;

    // Step 1: Populate child layers from
    // `visible_on_all_desks_windows_to_mirror` with their orders.
    for (auto* window : visible_on_all_desks_windows_to_mirror) {
      if (GetLayerDataEntry(layers_data, window->layer()).should_skip_layer) {
        continue;
      }

      if (base::ranges::find(mru_windows, window) == mru_windows.end()) {
        continue;
      }

      // Find z order of `window`. If `features::IsPerDeskZOrderEnabled()` is
      // not on, default value of zero will be used so `window` would be put
      // on top.
      std::optional<size_t> target_desk_order =
          GetWindowZOrderForDeskAndRoot(window, desk, root);
      std::optional<size_t> active_desk_order = GetWindowZOrderForDeskAndRoot(
          window, DesksController::Get()->active_desk(), root);
      layer_orders.push_back({.layer = window->layer(),
                              .primary_key = target_desk_order.value_or(0),
                              .secondary_key = active_desk_order.value_or(0)});
      primary_key_taken.insert(target_desk_order.value_or(0));
    }

    // Step 2: Populate child layers from `source_layer` with their orders.
    size_t order = 0;
    for (ui::Layer* it : base::Reversed(source_layer->children())) {
      while (primary_key_taken.contains(order)) {
        order++;
      }
      layer_orders.push_back(
          {.layer = it, .primary_key = order++, .secondary_key = SIZE_MAX});
    }

    // Step 3: Sort all child layers based on `LayerOrderData` and write to
    // `children` for further recursion.
    base::ranges::sort(
        layer_orders, [](const LayerOrderData& lhs, const LayerOrderData& rhs) {
          return (lhs.primary_key > rhs.primary_key) ||
                 (lhs.primary_key == rhs.primary_key &&
                  lhs.secondary_key > rhs.secondary_key);
        });
    children.reserve(layer_orders.size());
    for (const auto& lo : layer_orders) {
      children.emplace_back(lo.layer.get());
    }
  }

  for (ui::Layer* child : children) {
    // Visible on all desks windows only needed to be added to the subtree once
    // so use an empty set for subsequent calls.
    MirrorLayerTree(child, mirror, layers_data, base::flat_set<aura::Window*>(),
                    desk_container);
  }

  // Disables rounded corners sync on the mirroring layer. Changes on its source
  // layer's rounded corners shouldn't affect the rounded corners of the
  // mirroring layer.
  // On entering overview, the rounded corners of the windows get updated after
  // the starting animation completes. These rounded corners are added
  // specifically for the visuals of the windows inside overview, whereas the
  // desk previews reflect the windows visuals outside of overview. Hence, these
  // changes of the rounded corners on the source layers should not show up on
  // the mirror layers. See http://b/293946863.
  mirror->set_sync_rounded_corners_with_source(false);
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
// of the given |window|, and fills |out_layers_data|. If
// `window_occlusion_calculator` is null, the window's occlusion state will not
// be considered when deciding whether the layer should be skipped.
void GetLayersData(aura::Window* window,
                   const WindowOcclusionCalculator* window_occlusion_calculator,
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

  if (window_occlusion_calculator) {
    switch (window_occlusion_calculator->GetOcclusionState(window)) {
      case aura::Window::OcclusionState::VISIBLE:
        break;
      case aura::Window::OcclusionState::OCCLUDED:
      case aura::Window::OcclusionState::HIDDEN:
      case aura::Window::OcclusionState::UNKNOWN:
        // Performance optimization. Don't mirror layers of desk windows which
        // aren't visible. Note the occlusion state can be `UNKNOWN` in corner
        // cases for windows just added. The occlusion will become available
        // imminently, at which point
        // `DeskPreviewView::RecreateDeskContentsMirrorLayers()` will be called
        // again.
        layer_data.should_skip_layer = true;
        return;
    }
  }

  // Windows transformed into position in the overview mode grid should be
  // mirrored and the transforms of the mirrored layers should be reset to
  // identity.
  if (window->GetProperty(kForceVisibleInMiniViewKey))
    layer_data.should_force_mirror_visible = true;

  // Since floated window is not stored in desk container and will be hidden
  // when the desk is inactive, or when we switch to the saved desk grid on
  // overview (via `HideForSavedDeskLibrary` etc.). We need to make sure it's
  // visible in the desk mini view at anytime, except when it's minimized (which
  // has been handled above). Currently we force the floated window to be
  // visible at all time, since we don't have a use case where we need to hide
  // floated window for desk preview.
  if (window_state && window_state->IsFloated())
    layer_data.should_force_mirror_visible = true;

  // Visible on all desks windows and floated windows aren't children of
  // inactive desk's container so mark them explicitly to clear overview
  // transforms. Additionally, windows in overview mode are transformed into
  // their positions in the grid, but we want to show a preview of the windows
  // in their untransformed state.
  if (desks_util::IsWindowVisibleOnAllWorkspaces(window) ||
      (window_state && window_state->IsFloated()) ||
      desks_util::IsDeskContainer(window->parent())) {
    layer_data.should_clear_transform = true;
  }

  for (aura::Window* child : window->children()) {
    GetLayersData(child, window_occlusion_calculator, out_layers_data);
  }
}

}  // namespace

// -----------------------------------------------------------------------------
// DeskPreviewView

DeskPreviewView::DeskPreviewView(
    PressedCallback callback,
    DeskMiniView* mini_view,
    base::WeakPtr<WindowOcclusionCalculator> window_occlusion_calculator)
    : views::Button(std::move(callback)),
      mini_view_(mini_view),
      window_occlusion_calculator_(window_occlusion_calculator),
      wallpaper_preview_(new WallpaperRoundedCornerView),
      desk_mirrored_contents_view_(new views::View),
      force_desk_occlusion_tracker_visible_(
          aura::WindowOcclusionTracker::ScopedForceVisible(
              mini_view->GetDeskContainer())) {
  TRACE_EVENT0("ui", "DeskPreviewView::DeskPreviewView");

  DCHECK(mini_view_);

  SetFocusPainter(nullptr);
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  SetPaintToLayer(ui::LAYER_TEXTURED);
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(false);

  AddChildView(wallpaper_preview_.get());

  desk_mirrored_contents_view_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  ui::Layer* contents_view_layer = desk_mirrored_contents_view_->layer();
  contents_view_layer->SetMasksToBounds(true);
  contents_view_layer->SetName("Desk mirrored contents view");
  contents_view_layer->SetRoundedCornerRadius(kCornerRadius);
  contents_view_layer->SetIsFastRoundedCorner(true);
  AddChildView(desk_mirrored_contents_view_.get());

  highlight_overlay_ = AddChildView(std::make_unique<views::View>());
  highlight_overlay_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  highlight_overlay_->SetVisible(false);
  ui::Layer* highlight_overlay_layer = highlight_overlay_->layer();
  highlight_overlay_layer->SetName("DeskPreviewView highlight overlay");
  highlight_overlay_layer->SetRoundedCornerRadius(kCornerRadius);
  highlight_overlay_layer->SetIsFastRoundedCorner(true);

  RecreateDeskContentsMirrorLayers();

  GetViewAccessibility().SetRoleDescription(
      l10n_util::GetStringUTF8(IDS_ASH_DESKS_DESK_PREVIEW_ROLE_DESCRIPTION));
  UpdateAccessibleName();

  AddAccelerator(ui::Accelerator(ui::VKEY_LEFT, ui::EF_CONTROL_DOWN));
  AddAccelerator(ui::Accelerator(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN));
  AddAccelerator(ui::Accelerator(ui::VKEY_W, ui::EF_CONTROL_DOWN));
  AddAccelerator(
      ui::Accelerator(ui::VKEY_W, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN));
}

DeskPreviewView::~DeskPreviewView() {
  if (window_occlusion_calculator_) {
    window_occlusion_calculator_->RemoveObserver(this);
  }
}

// static
int DeskPreviewView::GetHeight(aura::Window* root) {
  DCHECK(root);
  DCHECK(root->IsRootWindow());

  const int height_divider =
      root->bounds().width() <= kUseSmallerHeightDividerWidthThreshold
          ? kRootHeightDividerForSmallScreen
          : kRootHeightDivider;
  return std::clamp(root->bounds().height() / height_divider,
                    kDeskPreviewMinHeight, kDeskPreviewMaxHeight);
}

void DeskPreviewView::SetHighlightOverlayVisibility(bool visible) {
  DCHECK(highlight_overlay_);
  highlight_overlay_->SetVisible(visible);
}

void DeskPreviewView::RecreateDeskContentsMirrorLayers() {
  TRACE_EVENT0("ui", "DeskPreviewView::RecreateDeskContentsMirrorLayers");
  if (!mini_view_->desk()) {
    DVLOG(4) << "Desk has already been deleted. Skipping " << __func__
             << " since this view will be deleted soon anyways.";
    return;
  }
  auto* desk_container = mini_view_->GetDeskContainer();
  DCHECK(desk_container);
  DCHECK(desk_container->layer());

  // For simplicity, clear occlusion observation state and set it up again.
  if (window_occlusion_calculator_) {
    window_occlusion_calculator_->RemoveObserver(this);
  }
  aura::Window::Windows parent_windows_to_mirror = {desk_container};
  // If there is a floated window that belongs to this desk, since it doesn't
  // belong to `desk_container`, we need to add it separately.
  aura::Window* floated_window =
      Shell::Get()->float_controller()->FindFloatedWindowOfDesk(
          mini_view_->desk());
  if (floated_window) {
    parent_windows_to_mirror.push_back(floated_window);
    force_float_occlusion_tracker_visible_.emplace(floated_window);
  } else {
    force_float_occlusion_tracker_visible_.reset();
  }
  if (window_occlusion_calculator_) {
    window_occlusion_calculator_->AddObserver(parent_windows_to_mirror, this);
  }

  // Mirror the layer tree of the desk container.
  auto mirrored_content_root_layer =
      std::make_unique<ui::Layer>(ui::LAYER_NOT_DRAWN);
  mirrored_content_root_layer->SetName("mirrored contents root layer");
  base::flat_map<ui::Layer*, LayerData> layers_data;
  for (const auto& window : parent_windows_to_mirror) {
    GetLayersData(window.get(), window_occlusion_calculator_.get(),
                  &layers_data);
  }

  base::flat_set<aura::Window*> visible_on_all_desks_windows_to_mirror;
  if (!desks_util::IsActiveDeskContainer(desk_container)) {
    // Since visible on all desks windows reside on the active desk, only mirror
    // them in the layer tree if |this| is not the preview view for the active
    // desk.
    visible_on_all_desks_windows_to_mirror =
        Shell::Get()->desks_controller()->GetVisibleOnAllDesksWindowsOnRoot(
            mini_view_->root_window());
    for (auto* window : visible_on_all_desks_windows_to_mirror) {
      // An all-desk-window's occlusion state on the active desk does not
      // necessarily apply when mirroring it in a different inactive desk.
      // The all-desk-window's z-order gets recomputed for the inactive desk
      // (see `MirrorLayerTree()`), so don't use occlusion state to optimize
      // when building the layer data here.
      GetLayersData(window, /*window_occlusion_calsculator=*/nullptr,
                    &layers_data);
    }
  }

  auto* desk_container_layer = desk_container->layer();
  MirrorLayerTree(desk_container_layer, mirrored_content_root_layer.get(),
                  layers_data, visible_on_all_desks_windows_to_mirror,
                  desk_container);

  // Since floated window is not stored in desk container, we need to mirror it
  // separately.
  if (floated_window) {
    auto* floated_window_layer = floated_window->layer();
    MirrorLayerTree(floated_window_layer, mirrored_content_root_layer.get(),
                    layers_data, /*visible_on_all_desks_windows_to_mirror=*/{},
                    desk_container);
  }

  // Add the root of the mirrored layer tree as a child of the
  // |desk_mirrored_contents_view_|'s layer.
  ui::Layer* contents_view_layer = desk_mirrored_contents_view_->layer();
  contents_view_layer->Add(mirrored_content_root_layer.get());

  // Take ownership of the mirrored layer tree.
  desk_mirrored_contents_layer_tree_owner_ =
      std::make_unique<ui::LayerTreeOwner>(
          std::move(mirrored_content_root_layer));

  InvalidateLayout();
}

void DeskPreviewView::Close(bool primary_action) {
  // The primary action (Ctrl + W) is to remove the desk and not close the
  // windows (combine the desk with one on the right or left). The secondary
  // action (Ctrl + Shift + W) is to close the desk and all its applications.
  mini_view_->OnRemovingDesk(primary_action
                                 ? DeskCloseType::kCombineDesks
                                 : DeskCloseType::kCloseAllWindowsAndWait);
}

void DeskPreviewView::Swap(bool right) {
  const int old_index = mini_view_->owner_bar()->GetMiniViewIndex(mini_view_);
  CHECK_NE(old_index, -1);

  int new_index = right ? old_index + 1 : old_index - 1;
  if (new_index < 0 ||
      new_index ==
          static_cast<int>(mini_view_->owner_bar()->mini_views().size())) {
    return;
  }

  auto* desks_controller = DesksController::Get();
  desks_controller->ReorderDesk(old_index, new_index);
  desks_controller->UpdateDesksDefaultNames();
}

void DeskPreviewView::UpdateAccessibleName() {
  if (Desk* desk = mini_view_->desk()) {
    GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
        desk->is_active() ? IDS_ASH_DESKS_DESK_PREVIEW_ACTIVE
                          : IDS_ASH_DESKS_DESK_PREVIEW_INACTIVE,
        desk->name()));
  }

  // Avoid failing accessibility checks if we don't have a name.
  if (GetViewAccessibility().GetCachedName().empty()) {
    GetViewAccessibility().SetName(
        "", ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
  }
}

void DeskPreviewView::AcceptSelection() {
  DesksController::Get()->ActivateDesk(
      mini_view_->desk(),
      mini_view_->owner_bar()->type() == DeskBarViewBase::Type::kDeskButton
          ? DesksSwitchSource::kDeskButtonMiniViewButton
          : DesksSwitchSource::kMiniViewButton);
}

size_t DeskPreviewView::GetNumLayersMirrored() const {
  return GetNumDescendants(desk_mirrored_contents_layer_tree_owner_->root());
}

void DeskPreviewView::Layout(PassKey) {
  const gfx::Rect bounds = GetContentsBounds();
  wallpaper_preview_->SetBoundsRect(bounds);
  desk_mirrored_contents_view_->SetBoundsRect(bounds);

  highlight_overlay_->SetBoundsRect(bounds);

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

  LayoutSuperclass<Button>(this);
}

bool DeskPreviewView::OnMousePressed(const ui::MouseEvent& event) {
  // If we have a right click we should open the context menu.
  if (event.IsRightMouseButton()) {
    DeskNameView::CommitChanges(GetWidget());
    mini_view_->OpenContextMenu(ui::MENU_SOURCE_MOUSE);
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
  DeskBarViewBase* owner_bar = mini_view_->owner_bar();

  switch (event->type()) {
    // Only long press can trigger drag & drop.
    case ui::EventType::kGestureLongPress:
      owner_bar->HandleLongPressEvent(mini_view_, *event);
      event->SetHandled();
      break;
    case ui::EventType::kGestureScrollBegin:
      [[fallthrough]];
    case ui::EventType::kGestureScrollUpdate:
      owner_bar->HandleDragEvent(mini_view_, *event);
      if (owner_bar->IsDraggingDesk())
        event->SetHandled();
      break;
    case ui::EventType::kGestureEnd:
      if (owner_bar->HandleReleaseEvent(mini_view_, *event))
        event->SetHandled();
      break;
    default:
      break;
  }

  if (!event->handled())
    Button::OnGestureEvent(event);
}

void DeskPreviewView::OnThemeChanged() {
  views::Button::OnThemeChanged();

  highlight_overlay_->layer()->SetColor(SkColorSetA(
      GetColorProvider()->GetColor(ui::kColorHighlightBorderHighlight1),
      kHighlightTransparency));
}

void DeskPreviewView::OnFocus() {
  mini_view_->UpdateDeskButtonVisibility();
  mini_view_->UpdateFocusColor();
  View::OnFocus();
}

void DeskPreviewView::OnBlur() {
  mini_view_->UpdateDeskButtonVisibility();
  mini_view_->UpdateFocusColor();
  View::OnBlur();
}

void DeskPreviewView::AboutToRequestFocusFromTabTraversal(bool reverse) {
  if (reverse) {
    mini_view_->OnPreviewOrProfileAboutToBeFocusedByReverseTab();
  }
}

bool DeskPreviewView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  if (!accelerator.IsCtrlDown()) {
    return views::Button::AcceleratorPressed(accelerator);
  }

  if (accelerator.key_code() == ui::VKEY_LEFT ||
      accelerator.key_code() == ui::VKEY_RIGHT) {
    Swap(/*right=*/accelerator.key_code() == ui::VKEY_RIGHT);
    return true;
  }

  if (accelerator.key_code() == ui::VKEY_W) {
    Close(/*primary_action=*/!accelerator.IsShiftDown());
    return true;
  }
  return views::Button::AcceleratorPressed(accelerator);
}

bool DeskPreviewView::CanHandleAccelerators() const {
  return HasFocus() && views::Button::CanHandleAccelerators();
}

void DeskPreviewView::OnWindowOcclusionChanged(aura::Window* window) {
  // If `window_occlusion_calculator_` finds multiple windows with occlusion
  // changes in one calculation, they can be condensed into one
  // `RecreateDeskContentsMirrorLayers()` call by canceling any pending task
  // already scheduled.
  recreate_mirror_layers_weak_factory_.InvalidateWeakPtrs();

  // `RecreateDeskContentsMirrorLayers()` cannot be called directly. If it is,
  // it creates an infinite loop`:
  // * DeskPreviewView::OnWindowOcclusionChanged()
  //   * DeskPreviewView::RecreateDeskContentsMirrorLayers()
  //     * WindowOcclusionCalculator::RemoveObserver(this)
  //     * WindowOcclusionCalculator::AddObserver(..., this)
  // * Iterate to the next observer in the list (which is `this` again). Go back
  //   to previous step.
  //
  // Posting a task fixes this because it finishes the
  // `WindowOcclusionCalculator::Observer::OnWindowOcclusionChanged()`
  // notification loop before `DeskPreviewView` resets its observation state.
  // It's also just simpler to reason about.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&DeskPreviewView::RecreateDeskContentsMirrorLayers,
                     recreate_mirror_layers_weak_factory_.GetWeakPtr()));
}

BEGIN_METADATA(DeskPreviewView)
END_METADATA

}  // namespace ash
