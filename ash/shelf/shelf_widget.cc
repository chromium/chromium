// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_widget.h"

#include <memory>
#include <utility>

#include "ash/animation/animation_change_type.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/controls/contextual_tooltip.h"
#include "ash/focus_cycler.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/drag_handle.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/hotseat_transition_animator.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/login_shelf_view.h"
#include "ash/shelf/login_shelf_widget.h"
#include "ash/shelf/scrollable_shelf_view.h"
#include "ash/shelf/shelf_background_animator_observer.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/style_util.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/work_area_insets.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/base/window_properties.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_owner.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/highlight_border.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

constexpr int kShelfBlurRadius = 30;
// The maximum size of the opaque layer during an "overshoot" (drag away from
// the screen edge).
constexpr int kShelfMaxOvershootHeight = 60;
constexpr int kDragHandleCornerRadius = 2;

// Sets the shelf opacity to 0 when the shelf is done hiding to avoid getting
// rid of blur.
class HideAnimationObserver : public ui::ImplicitAnimationObserver {
 public:
  explicit HideAnimationObserver(ui::Layer* layer) : layer_(layer) {}

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsScheduled() override {}

  void OnImplicitAnimationsCompleted() override { layer_->SetOpacity(0); }

 private:
  // Unowned.
  const raw_ptr<ui::Layer, DanglingUntriaged> layer_;
};

class ShelfBackgroundLayerDelegate : public ui::LayerOwner,
                                     public ui::LayerDelegate {
 public:
  ShelfBackgroundLayerDelegate(Shelf* shelf, views::View* owner_view)
      : shelf_(shelf), owner_view_(owner_view) {}

  ShelfBackgroundLayerDelegate(const ShelfBackgroundLayerDelegate&) = delete;
  ShelfBackgroundLayerDelegate& operator=(const ShelfBackgroundLayerDelegate&) =
      delete;
  ~ShelfBackgroundLayerDelegate() override {}

  void Initialize() {
    auto layer = std::make_unique<ui::Layer>(ui::LAYER_TEXTURED);
    layer->SetName("shelf/Background");
    layer->set_delegate(this);
    layer->SetFillsBoundsOpaquely(false);
    SetLayer(std::move(layer));
  }

  // Sets the shelf background color.
  void SetBackgroundColor(SkColor color) {
    if (color == background_color_) {
      return;
    }

    background_color_ = color;
    layer()->SchedulePaint(gfx::Rect(layer()->size()));
  }

  void SetBorderType(views::HighlightBorder::Type type) {
    if (type == highlight_border_type_) {
      return;
    }

    highlight_border_type_ = type;
    layer()->SchedulePaint(gfx::Rect(layer()->size()));
  }

  // Sets the rounded corners used by the shelf.
  void SetRoundedCornerRadius(float radius) {
    const bool needs_paint = corner_radius_ != radius;
    corner_radius_ = radius;

    layer()->SetRoundedCornerRadius({
        shelf_->SelectValueForShelfAlignment(radius, 0.0f, radius),
        shelf_->SelectValueForShelfAlignment(radius, radius, 0.0f),
        shelf_->SelectValueForShelfAlignment(0.0f, radius, 0.0f),
        shelf_->SelectValueForShelfAlignment(0.0f, 0.0f, radius),
    });

    if (needs_paint) {
      layer()->SchedulePaint(gfx::Rect(layer()->size()));
    }
  }

  SkColor background_color() const { return background_color_; }

 private:
  // views::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    ui::PaintRecorder recorder(context, layer()->size());
    gfx::Canvas* canvas = recorder.canvas();

    // cc::PaintFlags flags for the background.
    cc::PaintFlags flags;
    flags.setColor(background_color_);
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawRoundRect(gfx::Rect(layer()->size()), corner_radius_, flags);

    // Don't draw highlight border in login screen.
    LoginShelfView* login_shelf_view =
        shelf_->login_shelf_widget()->login_shelf_view();
    if (login_shelf_view && login_shelf_view->GetVisible())
      return;

    if (corner_radius_ > 0) {
      views::HighlightBorder::PaintBorderToCanvas(
          canvas, *owner_view_, gfx::Rect(layer()->size()),
          gfx::RoundedCornersF(corner_radius_), highlight_border_type_);
    } else {
      // If the shelf corners are not rounded, only paint the highlight border
      // on the inner edge of the shelf to separate the shelf and the work area.
      PaintEdgeToCanvas(canvas);
    }
  }

  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {
    layer()->SchedulePaint(gfx::Rect(layer()->size()));
  }

  void PaintEdgeToCanvas(gfx::Canvas* canvas) {
    SkColor inner_color = views::HighlightBorder::GetHighlightColor(
        *owner_view_, highlight_border_type_);
    SkColor outer_color = views::HighlightBorder::GetBorderColor(
        *owner_view_, highlight_border_type_);

    const int border_thickness = views::kHighlightBorderThickness;
    const float half_thickness = border_thickness / 2.0f;

    cc::PaintFlags flags;
    flags.setStrokeWidth(border_thickness);
    flags.setColor(outer_color);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setAntiAlias(true);

    // Scale bounds and corner radius with device scale factor to make sure
    // border bounds match content bounds but keep border stroke width the same.
    gfx::ScopedCanvas scoped_canvas(canvas);
    const float dsf = canvas->UndoDeviceScaleFactor();
    const gfx::RectF pixel_bounds =
        gfx::ConvertRectToPixels(gfx::Rect(layer()->size()), dsf);

    // The points that are used to draw the highlighted edge.
    gfx::PointF start_point, end_point;

    switch (shelf_->alignment()) {
      case ShelfAlignment::kBottom:
      case ShelfAlignment::kBottomLocked:
        start_point = gfx::PointF(pixel_bounds.origin());
        end_point = gfx::PointF(pixel_bounds.top_right());
        start_point.Offset(0, half_thickness);
        end_point.Offset(0, half_thickness);
        break;
      case ShelfAlignment::kLeft:
        start_point = gfx::PointF(pixel_bounds.top_right());
        end_point = gfx::PointF(pixel_bounds.bottom_right());
        start_point.Offset(-half_thickness, 0);
        end_point.Offset(-half_thickness, 0);
        break;
      case ShelfAlignment::kRight:
        start_point = gfx::PointF(pixel_bounds.origin());
        end_point = gfx::PointF(pixel_bounds.bottom_left());
        start_point.Offset(half_thickness, 0);
        end_point.Offset(half_thickness, 0);
        break;
    }

    // Draw the outer line.
    canvas->DrawLine(start_point, end_point, flags);

    switch (shelf_->alignment()) {
      case ShelfAlignment::kBottom:
      case ShelfAlignment::kBottomLocked:
        start_point.Offset(0, border_thickness);
        end_point.Offset(0, border_thickness);
        break;
      case ShelfAlignment::kLeft:
        start_point.Offset(-border_thickness, 0);
        end_point.Offset(-border_thickness, 0);
        break;
      case ShelfAlignment::kRight:
        start_point.Offset(border_thickness, 0);
        end_point.Offset(border_thickness, 0);
        break;
    }

    // Draw the inner line.
    flags.setColor(inner_color);
    canvas->DrawLine(start_point, end_point, flags);
  }

  const raw_ptr<Shelf> shelf_;
  const raw_ptr<views::View> owner_view_;

  SkColor background_color_ = gfx::kPlaceholderColor;
  float corner_radius_ = 0.0f;
  views::HighlightBorder::Type highlight_border_type_ =
      chromeos::features::IsJellyrollEnabled()
          ? views::HighlightBorder::Type::kHighlightBorderNoShadow
          : views::HighlightBorder::Type::kHighlightBorder1;
};

}  // namespace

// The contents view of the Shelf. In an active session, this is used to
// display a semi-opaque background behind the shelf. Outside of an active
// session, this also contains the login shelf view.
class ShelfWidget::DelegateView : public views::WidgetDelegate,
                                  public views::AccessiblePaneView,
                                  public ShelfBackgroundAnimatorObserver,
                                  public HotseatTransitionAnimator::Observer {
 public:
  DelegateView(ShelfWidget* shelf_widget, Shelf* shelf);

  DelegateView(const DelegateView&) = delete;
  DelegateView& operator=(const DelegateView&) = delete;

  ~DelegateView() override;

  void set_focus_cycler(FocusCycler* focus_cycler) {
    focus_cycler_ = focus_cycler;
  }

  FocusCycler* focus_cycler() { return focus_cycler_; }

  void SetParentLayer(ui::Layer* layer);

  // Immediately hides the layer used to draw the shelf background.
  void HideOpaqueBackground();

  // Immediately shows the layer used to draw the shelf background.
  void ShowOpaqueBackground();

  // views::WidgetDelegate:
  views::Widget* GetWidget() override { return View::GetWidget(); }
  const views::Widget* GetWidget() const override { return View::GetWidget(); }

  // views::View:
  void OnThemeChanged() override;

  bool CanActivate() const override;
  void ReorderChildLayers(ui::Layer* parent_layer) override;
  void OnWidgetInitialized() override;

  void UpdateBackgroundBlur();
  void UpdateOpaqueBackground();
  void UpdateDragHandle();

  // This will be called when the parent local bounds change.
  void OnBoundsChanged(const gfx::Rect& old_bounds) override;

  // views::AccessiblePaneView:
  void Layout(PassKey) override;

  // ShelfBackgroundAnimatorObserver:
  void UpdateShelfBackground(SkColor color) override;

  // HotseatBackgroundAnimator::Observer:
  void OnHotseatTransitionAnimationWillStart(HotseatState from_state,
                                             HotseatState to_state) override;
  void OnHotseatTransitionAnimationEnded(HotseatState from_state,
                                         HotseatState to_state) override;

  // Hide or show the the |animating_background_| layer.
  void ShowAnimatingBackground(bool show);

  SkColor GetShelfBackgroundColor() const;

  ui::Layer* opaque_background_layer() { return opaque_background_.layer(); }
  ShelfBackgroundLayerDelegate* opaque_background() {
    return &opaque_background_;
  }

  ui::Layer* animating_background() { return &animating_background_; }
  ui::Layer* animating_drag_handle() { return &animating_drag_handle_; }
  DragHandle* drag_handle() { return drag_handle_; }

 private:
  // Whether |opaque_background_| is explicitly hidden during an animation.
  // Prevents calls to UpdateOpaqueBackground from inadvertently showing
  // |opaque_background_| during animations.
  bool hide_background_for_transitions_ = false;
  const raw_ptr<ShelfWidget> shelf_widget_;
  raw_ptr<FocusCycler> focus_cycler_ = nullptr;

  // A background layer that may be visible depending on a
  // ShelfBackgroundAnimator.
  ShelfBackgroundLayerDelegate opaque_background_;

  // A background layer used to animate hotseat transitions.
  ui::Layer animating_background_;

  // A layer to animate the drag handle during hotseat transitions.
  ui::Layer animating_drag_handle_;

  // A drag handle shown in tablet mode when we are not on the home screen.
  // Owned by the view hierarchy.
  raw_ptr<DragHandle> drag_handle_ = nullptr;

  // Cache the state of the background blur so that it can be updated only
  // when necessary.
  bool background_is_currently_blurred_ = false;
};

ShelfWidget::DelegateView::DelegateView(ShelfWidget* shelf_widget, Shelf* shelf)
    : shelf_widget_(shelf_widget),
      opaque_background_(shelf, this),
      animating_background_(ui::LAYER_SOLID_COLOR),
      animating_drag_handle_(ui::LAYER_SOLID_COLOR) {
  animating_background_.SetName("shelf/Animation");
  animating_background_.Add(&animating_drag_handle_);

  opaque_background_.Initialize();

  DCHECK(shelf_widget_);
  SetOwnedByWidget(true);

  set_allow_deactivate_on_esc(true);

  // |animating_background_| will be made visible during hotseat animations.
  ShowAnimatingBackground(false);

  drag_handle_ = AddChildView(
      std::make_unique<DragHandle>(kDragHandleCornerRadius, shelf));

  animating_drag_handle_.SetRoundedCornerRadius(
      {kDragHandleCornerRadius, kDragHandleCornerRadius,
       kDragHandleCornerRadius, kDragHandleCornerRadius});
}

ShelfWidget::DelegateView::~DelegateView() = default;

void ShelfWidget::DelegateView::SetParentLayer(ui::Layer* layer) {
  layer->Add(opaque_background_layer());
  ReorderLayers();
  // Animating background is only shown during hotseat state transitions to
  // animate the background from below the shelf. At the same time the shelf
  // widget may be animating between in-app and system shelf. Make animating
  // background the sibling of the shelf widget to avoid shelf widget animation
  // from interfering with the animating background animation.
  layer->parent()->Add(&animating_background_);
  layer->parent()->StackAtBottom(&animating_background_);
}

void ShelfWidget::DelegateView::HideOpaqueBackground() {
  hide_background_for_transitions_ = true;
  opaque_background_layer()->SetVisible(false);
  drag_handle_->SetVisible(false);
}

void ShelfWidget::DelegateView::ShowOpaqueBackground() {
  hide_background_for_transitions_ = false;
  UpdateOpaqueBackground();
  UpdateDragHandle();
  UpdateBackgroundBlur();
}

void ShelfWidget::DelegateView::OnThemeChanged() {
  views::AccessiblePaneView::OnThemeChanged();
  shelf_widget_->background_animator_.PaintBackground(
      shelf_widget_->shelf_layout_manager()->ComputeShelfBackgroundType(),
      AnimationChangeType::IMMEDIATE);
  animating_background_.SetColor(
      GetColorProvider()->GetColor(cros_tokens::kCrosSysSystemBase));
  animating_drag_handle_.SetColor(
      GetColorProvider()->GetColor(cros_tokens::kCrosSysOnSurface));
}

bool ShelfWidget::DelegateView::CanActivate() const {
  return false;
}

void ShelfWidget::DelegateView::ReorderChildLayers(ui::Layer* parent_layer) {
  views::View::ReorderChildLayers(parent_layer);
  parent_layer->StackAtBottom(opaque_background_layer());
}

void ShelfWidget::DelegateView::OnWidgetInitialized() {
  UpdateOpaqueBackground();
}

void ShelfWidget::DelegateView::UpdateBackgroundBlur() {
  if (hide_background_for_transitions_)
    return;
  // Blur only if the background is visible.
  const bool should_blur_background =
      opaque_background_layer()->visible() &&
      shelf_widget_->shelf_layout_manager()->ShouldBlurShelfBackground();
  if (should_blur_background == background_is_currently_blurred_)
    return;

  opaque_background_layer()->SetBackgroundBlur(
      should_blur_background ? kShelfBlurRadius : 0);
  opaque_background_layer()->SetBackdropFilterQuality(
      ColorProvider::kBackgroundBlurQuality);

  background_is_currently_blurred_ = should_blur_background;
}

void ShelfWidget::DelegateView::UpdateOpaqueBackground() {
  if (hide_background_for_transitions_)
    return;
  // Shell could be destroying.
  if (!Shell::Get()->tablet_mode_controller())
    return;

  gfx::Rect opaque_background_bounds = GetLocalBounds();

  // Let the shelf occlude things below it - this helps prevent unnecessary
  // occlusion updates when changing display scale. The shelf widget may have
  // rounded corners and background blur. But, it almost opaque (very low high
  // alpha, and small rounded corners), so we manually make the window opaque
  // so that the window behind it can be marked as occluded.
  shelf_widget_->GetNativeWindow()->SetOpaqueRegionsForOcclusion(
      std::vector<gfx::Rect>{opaque_background_bounds});

  const Shelf* shelf = shelf_widget_->shelf();
  const ShelfBackgroundType background_type =
      shelf_widget_->shelf_layout_manager()->shelf_background_type();
  const bool tablet_mode = Shell::Get()->IsInTabletMode();
  const bool in_app = ShelfConfig::Get()->is_in_app();

  const bool in_overview_mode = ShelfConfig::Get()->in_overview_mode();
  const bool in_forest_session =
      in_overview_mode && features::IsForestFeatureEnabled();
  const bool split_view = ShelfConfig::Get()->in_split_view_with_overview();
  bool show_opaque_background =
      !in_forest_session && (!tablet_mode || in_app || split_view);
  auto* opaque_back_ground_layer = opaque_background_layer();
  if (show_opaque_background != opaque_back_ground_layer->visible()) {
    opaque_back_ground_layer->SetVisible(show_opaque_background);
  }

  // Extend the opaque layer a little bit to handle "overshoot" gestures
  // gracefully (the user drags the shelf further than it can actually go).
  // That way:
  // 1) When the shelf has rounded corners, only two of them are visible,
  // 2) Even when the shelf is squared, it doesn't tear off the screen edge
  // when dragged away.
  // To achieve this, we extend the layer in the same direction where the shelf
  // is aligned (downwards for a bottom shelf, etc.).
  const float radius = ShelfConfig::Get()->shelf_size() / 2.0f;
  // We can easily round only 2 corners out of 4 which means we don't need as
  // much extra shelf height.
  const int safety_margin = kShelfMaxOvershootHeight;
  opaque_background_bounds.Inset(gfx::Insets::TLBR(
      0, -shelf->SelectValueForShelfAlignment(0, safety_margin, 0),
      -shelf->SelectValueForShelfAlignment(safety_margin, 0, 0),
      -shelf->SelectValueForShelfAlignment(0, 0, safety_margin)));
  opaque_back_ground_layer->SetBounds(opaque_background_bounds);

  const bool is_vertical_alignment_in_overview =
      in_overview_mode && !shelf->IsHorizontalAlignment();

  // Show rounded corners except in maximized (which includes split view) mode,
  // or whenever we are "in app", or the shelf is on the vertical alignment in
  // overview mode.
  if (background_type == ShelfBackgroundType::kMaximized ||
      background_type == ShelfBackgroundType::kInApp ||
      (tablet_mode && (in_app || split_view)) ||
      is_vertical_alignment_in_overview) {
    opaque_background_.SetRoundedCornerRadius(0);
  } else {
    opaque_background_.SetRoundedCornerRadius(radius);
  }

  UpdateDragHandle();
  UpdateBackgroundBlur();
  SchedulePaint();
}

void ShelfWidget::DelegateView::UpdateDragHandle() {
  if (!Shell::Get()->IsInTabletMode()) {
    drag_handle_->SetVisible(false);
    return;
  }

  if ((!ShelfConfig::Get()->in_split_view_with_overview() &&
       !ShelfConfig::Get()->is_in_app()) ||
      hide_background_for_transitions_) {
    drag_handle_->SetVisible(false);
    return;
  }

  drag_handle_->SetVisible(true);
}

void ShelfWidget::DelegateView::OnBoundsChanged(const gfx::Rect& old_bounds) {
  UpdateOpaqueBackground();

  // Layout the animating background layer below the shelf bounds (the layer
  // will be transformed up as needed during hotseat state transitions).
  const gfx::Rect widget_bounds = GetWidget()->GetLayer()->bounds();
  animating_background_.SetBounds(
      gfx::Rect(gfx::Point(widget_bounds.x(), widget_bounds.bottom()),
                gfx::Size(widget_bounds.width(),
                          ShelfConfig::Get()->in_app_shelf_size())));

  // The StatusAreaWidget could be gone before this is called during display
  // tear down.
  if (shelf_widget_->status_area_widget())
    shelf_widget_->status_area_widget()->UpdateCollapseState();
}

void ShelfWidget::DelegateView::Layout(PassKey) {
  // Center drag handle within the expected in-app shelf bounds - it's safe to
  // assume bottom shelf, given that the drag handle is only shown within the
  // bottom shelf (either in tablet mode, or on login/lock screen)
  gfx::Rect drag_handle_bounds = GetLocalBounds();
  drag_handle_bounds.Inset(gfx::Insets().set_top(
      drag_handle_bounds.height() -
      ShelfConfig::Get()->shelf_drag_handle_centering_size()));
  drag_handle_bounds.ClampToCenteredSize(ShelfConfig::Get()->DragHandleSize());

  drag_handle_->SetBoundsRect(drag_handle_bounds);
}

void ShelfWidget::DelegateView::UpdateShelfBackground(SkColor color) {
  opaque_background_.SetBackgroundColor(color);
  UpdateOpaqueBackground();
}

void ShelfWidget::DelegateView::OnHotseatTransitionAnimationWillStart(
    HotseatState from_state,
    HotseatState to_state) {
  ShowAnimatingBackground(true);
  // If animating from a kShownHomeLauncher hotseat, the animating background
  // will animate from the hotseat background into the in-app shelf, so hide the
  // real shelf background until the animation is complete.
  if (from_state == HotseatState::kShownHomeLauncher)
    HideOpaqueBackground();
}

void ShelfWidget::DelegateView::OnHotseatTransitionAnimationEnded(
    HotseatState from_state,
    HotseatState to_state) {
  ShowAnimatingBackground(false);
  // NOTE: The from and to state may not match the transition states for which
  // the background was hidden (if the original animation got interrupted by
  // another transition, only the later animation end will be reported).
  if (hide_background_for_transitions_)
    ShowOpaqueBackground();
}

void ShelfWidget::DelegateView::ShowAnimatingBackground(bool show) {
  animating_background_.SetVisible(show);
}

SkColor ShelfWidget::DelegateView::GetShelfBackgroundColor() const {
  return opaque_background_.background_color();
}

base::ScopedClosureRunner ShelfWidget::ForceShowHotseatInTabletMode() {
  ++force_show_hotseat_count_;

  if (force_show_hotseat_count_ == 1)
    shelf_layout_manager_->UpdateVisibilityState(/*force_layout=*/false);

  return base::ScopedClosureRunner(base::BindOnce(
      &ShelfWidget::ResetForceShowHotseat, weak_ptr_factory_.GetWeakPtr()));
}

bool ShelfWidget::IsHotseatForcedShowInTabletMode() const {
  return force_show_hotseat_count_ > 0;
}

ui::Layer* ShelfWidget::GetOpaqueBackground() {
  return delegate_view_->opaque_background_layer();
}

ui::Layer* ShelfWidget::GetAnimatingBackground() {
  return delegate_view_->animating_background();
}

ui::Layer* ShelfWidget::GetAnimatingDragHandle() {
  return delegate_view_->animating_drag_handle();
}

DragHandle* ShelfWidget::GetDragHandle() {
  return delegate_view_->drag_handle();
}

void ShelfWidget::ScheduleShowDragHandleNudge() {
  delegate_view_->drag_handle()->ScheduleShowDragHandleNudge();
}

void ShelfWidget::HideDragHandleNudge(
    contextual_tooltip::DismissNudgeReason context) {
  delegate_view_->drag_handle()->HideDragHandleNudge(context, /*animate=*/true);
}

ShelfWidget::ShelfWidget(Shelf* shelf)
    : shelf_(shelf),
      background_animator_(shelf_, Shell::Get()->wallpaper_controller()),
      shelf_layout_manager_owned_(
          std::make_unique<ShelfLayoutManager>(this, shelf)),
      shelf_layout_manager_(shelf_layout_manager_owned_.get()),
      delegate_view_(new DelegateView(this, shelf_)),
      scoped_session_observer_(this) {
  DCHECK(shelf_);
}

ShelfWidget::~ShelfWidget() {
  // Must call Shutdown() before destruction.
  DCHECK(!status_area_widget());
}

void ShelfWidget::Initialize(aura::Window* shelf_container) {
  DCHECK(shelf_container);

  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "ShelfWidget";
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.delegate = delegate_view_.get();
  params.parent = shelf_container;

  Init(std::move(params));

  // The shelf should not take focus when initially shown.
  set_focus_on_creation(false);
  delegate_view_->SetParentLayer(GetLayer());
  SetContentsView(delegate_view_);

  shelf_layout_manager_->AddObserver(this);
  shelf_container->SetLayoutManager(std::move(shelf_layout_manager_owned_));
  shelf_layout_manager_->InitObservers();
  background_animator_.Init(ShelfBackgroundType::kDefaultBg);
  background_animator_.PaintBackground(
      shelf_layout_manager_->ComputeShelfBackgroundType(),
      AnimationChangeType::IMMEDIATE);

  background_animator_.AddObserver(delegate_view_);
  shelf_->AddObserver(this);

  Shell::Get()->overview_controller()->AddObserver(this);

  // Sets initial session state to make sure the UI is properly shown.
  OnSessionStateChanged(Shell::Get()->session_controller()->GetSessionState());
  delegate_view_->SetEnableArrowKeyTraversal(true);
}

void ShelfWidget::Shutdown() {
  hotseat_transition_animator_->RemoveObserver(delegate_view_);
  // Shutting down the status area widget may cause some widgets (e.g. bubbles)
  // to close, so uninstall the ShelfLayoutManager event filters first. Don't
  // reset the pointer until later because other widgets (e.g. app list) may
  // access it later in shutdown.
  shelf_layout_manager_->PrepareForShutdown();

  Shell::Get()->focus_cycler()->RemoveWidget(shelf_->status_area_widget());
  Shell::Get()->focus_cycler()->RemoveWidget(navigation_widget());
  Shell::Get()->focus_cycler()->RemoveWidget(hotseat_widget());
  if (features::IsDeskButtonEnabled()) {
    Shell::Get()->focus_cycler()->RemoveWidget(desk_button_widget());
  }

  // Don't need to update the shelf background during shutdown.
  background_animator_.RemoveObserver(delegate_view_);
  shelf_->RemoveObserver(this);

  // Don't need to observe focus/activation during shutdown.
  Shell::Get()->focus_cycler()->RemoveWidget(this);
  SetFocusCycler(nullptr);

  if (auto* overview_controller = Shell::Get()->overview_controller()) {
    overview_controller->RemoveObserver(this);
  }
}

void ShelfWidget::RegisterHotseatWidget(HotseatWidget* hotseat_widget) {
  // Show a context menu for right clicks anywhere on the shelf widget.
  delegate_view_->set_context_menu_controller(hotseat_widget->GetShelfView());
  hotseat_transition_animator_ =
      std::make_unique<HotseatTransitionAnimator>(this);
  hotseat_transition_animator_->AddObserver(delegate_view_);
  shelf_->hotseat_widget()->OnHotseatTransitionAnimatorCreated(
      hotseat_transition_animator());
}

void ShelfWidget::PostCreateShelf() {
  ash::FocusCycler* focus_cycler = Shell::Get()->focus_cycler();
  SetFocusCycler(focus_cycler);

  // Add widgets to |focus_cycler| in the desired focus order in LTR.
  focus_cycler->AddWidget(navigation_widget());
  if (features::IsDeskButtonEnabled()) {
    focus_cycler->AddWidget(desk_button_widget());
  }
  hotseat_widget()->SetFocusCycler(focus_cycler);
  focus_cycler->AddWidget(status_area_widget());

  shelf_layout_manager_->UpdateAutoHideState();
  ShowIfHidden();
}

bool ShelfWidget::IsShowingMenu() const {
  return hotseat_widget()->GetShelfView()->IsShowingMenu();
}

void ShelfWidget::SetFocusCycler(FocusCycler* focus_cycler) {
  delegate_view_->set_focus_cycler(focus_cycler);
  if (focus_cycler)
    focus_cycler->AddWidget(this);
}

FocusCycler* ShelfWidget::GetFocusCycler() {
  return delegate_view_->focus_cycler();
}

gfx::Rect ShelfWidget::GetScreenBoundsOfItemIconForWindow(
    aura::Window* window) {
  ShelfID id = ShelfID::Deserialize(window->GetProperty(kShelfIDKey));
  if (id.IsNull())
    return gfx::Rect();

  return hotseat_widget()
      ->scrollable_shelf_view()
      ->GetTargetScreenBoundsOfItemIcon(id);
}

gfx::Rect ShelfWidget::GetVisibleShelfBounds() const {
  gfx::Rect shelf_region = GetWindowBoundsInScreen();
  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(GetNativeWindow());
  DCHECK(!display.bounds().IsEmpty());
  shelf_region.Intersect(display.bounds());
  return screen_util::SnapBoundsToDisplayEdge(shelf_region, GetNativeWindow());
}

LoginShelfView* ShelfWidget::GetLoginShelfView() {
  return shelf_->login_shelf_widget()->login_shelf_view();
}

bool ShelfWidget::OnNativeWidgetActivationChanged(bool active) {
  return false;
}

void ShelfWidget::WillDeleteShelfLayoutManager() {
  shelf_layout_manager_->RemoveObserver(this);
  shelf_layout_manager_ = nullptr;
}

void ShelfWidget::OnHotseatStateChanged(HotseatState old_state,
                                        HotseatState new_state) {
  // |hotseat_transition_animator_| could be released when this is
  // called during shutdown.
  if (!hotseat_transition_animator_)
    return;
  hotseat_transition_animator_->OnHotseatStateChanged(old_state, new_state);

  if (chromeos::features::IsJellyrollEnabled()) {
    delegate_view_->opaque_background()->SetBorderType(
        views::HighlightBorder::Type::kHighlightBorderNoShadow);
  } else if (new_state == HotseatState::kExtended) {
    delegate_view_->opaque_background()->SetBorderType(
        views::HighlightBorder::Type::kHighlightBorder2);
  } else {
    delegate_view_->opaque_background()->SetBorderType(
        views::HighlightBorder::Type::kHighlightBorder1);
  }
}

void ShelfWidget::OnBackgroundTypeChanged(ShelfBackgroundType background_type,
                                          AnimationChangeType change_type) {
  delegate_view_->UpdateOpaqueBackground();
}

void ShelfWidget::CalculateTargetBounds() {
  const ShelfLayoutManager* layout_manager = shelf_->shelf_layout_manager();
  const int shelf_size = ShelfConfig::Get()->shelf_size();

  // By default, show the whole shelf on the screen.
  int shelf_in_screen_portion = shelf_size;
  const WorkAreaInsets* const work_area =
      WorkAreaInsets::ForWindow(GetNativeWindow());

  if (layout_manager->is_shelf_auto_hidden()) {
    const display::Display display =
        display::Screen::GetScreen()->GetDisplayNearestWindow(
            GetNativeWindow());
    shelf_in_screen_portion =
        Shell::Get()->app_list_controller()->GetTargetVisibility(display.id())
            ? shelf_size
            : ShelfConfig::Get()->hidden_shelf_in_screen_portion();
  } else if (layout_manager->visibility_state() == SHELF_HIDDEN ||
             work_area->IsKeyboardShown()) {
    shelf_in_screen_portion = 0;
  }

  gfx::Rect available_bounds =
      screen_util::GetDisplayBoundsWithShelf(GetNativeWindow());
  available_bounds.Inset(work_area->GetAccessibilityInsets());

  int shelf_width =
      shelf_->PrimaryAxisValue(available_bounds.width(), shelf_size);
  int shelf_height =
      shelf_->PrimaryAxisValue(shelf_size, available_bounds.height());
  const int shelf_primary_position = shelf_->SelectValueForShelfAlignment(
      available_bounds.bottom() - shelf_in_screen_portion,
      available_bounds.x() - shelf_size + shelf_in_screen_portion,
      available_bounds.right() - shelf_in_screen_portion);
  gfx::Point shelf_origin = shelf_->SelectValueForShelfAlignment(
      gfx::Point(available_bounds.x(), shelf_primary_position),
      gfx::Point(shelf_primary_position, available_bounds.y()),
      gfx::Point(shelf_primary_position, available_bounds.y()));

  target_bounds_ =
      gfx::Rect(shelf_origin.x(), shelf_origin.y(), shelf_width, shelf_height);
}

void ShelfWidget::UpdateLayout(bool animate) {
  const ShelfLayoutManager* layout_manager = shelf_->shelf_layout_manager();
  hide_animation_observer_.reset();
  gfx::Rect current_shelf_bounds = GetWindowBoundsInScreen();

  const float target_opacity = layout_manager->GetOpacity();
  if (GetLayer()->opacity() != target_opacity) {
    if (target_opacity == 0) {
      if (animate) {
        // On hide, set the opacity after the animation completes if |animate|
        // is true.
        hide_animation_observer_ =
            std::make_unique<HideAnimationObserver>(GetLayer());
      } else {
        // Otherwise, directly set the opacity to 0.
        GetLayer()->SetOpacity(0.0f);
      }
    } else {
      // On show, set the opacity before the animation begins to ensure the blur
      // is shown while the shelf moves.
      GetLayer()->SetOpacity(1.0f);
    }
  }

  if (GetNativeView()->layer()->GetAnimator()->is_animating()) {
    // When the |shelf_widget_| needs to reverse the direction of the current
    // animation, we must take into account the transform when calculating the
    // current shelf widget bounds.
    current_shelf_bounds =
        GetLayer()->transform().MapRect(current_shelf_bounds);
  }

  gfx::Transform shelf_widget_target_transform;
  shelf_widget_target_transform.Translate(current_shelf_bounds.origin() -
                                          GetTargetBounds().origin());
  GetLayer()->SetTransform(shelf_widget_target_transform);
  SetBounds(
      screen_util::SnapBoundsToDisplayEdge(target_bounds_, GetNativeWindow()));

  // There is no need to animate if the shelf already has the desired transform.
  if (!shelf_widget_target_transform.IsIdentity()) {
    ui::ScopedLayerAnimationSettings shelf_animation_setter(
        GetLayer()->GetAnimator());

    if (hide_animation_observer_)
      shelf_animation_setter.AddObserver(hide_animation_observer_.get());

    const base::TimeDelta animation_duration =
        animate ? ShelfConfig::Get()->shelf_animation_duration()
                : base::TimeDelta();
    if (!animate) {
      GetLayer()->GetAnimator()->StopAnimating();
      shelf_->status_area_widget()->GetLayer()->GetAnimator()->StopAnimating();
    }

    shelf_animation_setter.SetTransitionDuration(animation_duration);
    if (animate) {
      shelf_animation_setter.SetTweenType(gfx::Tween::EASE_OUT);
      shelf_animation_setter.SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    }
    GetLayer()->SetTransform(gfx::Transform());
  }

  delegate_view_->UpdateOpaqueBackground();
}

void ShelfWidget::UpdateTargetBoundsForGesture(int shelf_position) {
  if (shelf_->IsHorizontalAlignment()) {
    if (!Shell::Get()->IsInTabletMode())
      target_bounds_.set_y(shelf_position);
  } else {
    target_bounds_.set_x(shelf_position);
  }
}

void ShelfWidget::OnOverviewModeStarting() {
  delegate_view_->UpdateOpaqueBackground();
}

void ShelfWidget::OnOverviewModeEnding(OverviewSession* overview_session) {
  delegate_view_->UpdateOpaqueBackground();
}

gfx::Rect ShelfWidget::GetTargetBounds() const {
  return target_bounds_;
}

void ShelfWidget::OnSessionStateChanged(session_manager::SessionState state) {
  // Do not show shelf widget:
  // 1. when views based shelf is disabled; or
  // 2. in UNKNOWN state - it might be called before shelf was initialized; or
  // 3. in RMA state - shelf should be hidden to avoid blocking the RMA app
  // controls or intercepting UI events; or
  // 4. on secondary screens in states other than ACTIVE.
  bool hide_for_session_state =
      state == session_manager::SessionState::UNKNOWN ||
      state == session_manager::SessionState::RMA;
  bool hide_on_secondary_screen = shelf_->ShouldHideOnSecondaryDisplay(state);
  if (hide_for_session_state || hide_on_secondary_screen) {
    HideIfShown();
  } else {
    bool show_hotseat = (state == session_manager::SessionState::ACTIVE);
    hotseat_widget()->GetShelfView()->SetVisible(show_hotseat);
    hotseat_transition_animator_->SetAnimationsEnabledInSessionState(
        show_hotseat);

    // Shelf widget should only be active if login shelf view is visible.
    aura::Window* const shelf_window = GetNativeWindow();
    if (show_hotseat && IsActive())
      wm::DeactivateWindow(shelf_window);

    ShowIfHidden();

    // The shelf widget can get activated when login shelf view is shown, which
    // would stack it above other widgets in the shelf container, which is an
    // undesirable state for active session shelf (as the shelf background would
    // be painted over the hotseat/navigation buttons/status area). Make sure
    // the shelf widget is restacked at the bottom of the shelf container when
    // the session state changes.
    // TODO(crbug.com/40120650): Ideally, the shelf widget position at
    // the bottom of window stack would be maintained using a "stacked at
    // bottom" window property - switch to that approach once it's ready for
    // usage.
    if (show_hotseat)
      shelf_window->parent()->StackChildAtBottom(shelf_window);
  }
  shelf_layout_manager_->SetDimmed(false);
  delegate_view_->UpdateDragHandle();
}

void ShelfWidget::OnUserSessionAdded(const AccountId& account_id) {
  shelf_layout_manager_->SetDimmed(false);
}

SkColor ShelfWidget::GetShelfBackgroundColor() const {
  return delegate_view_->GetShelfBackgroundColor();
}

void ShelfWidget::HideIfShown() {
  if (IsVisible())
    Hide();
}

void ShelfWidget::ShowIfHidden() {
  if (!IsVisible())
    Show();
}

ui::Layer* ShelfWidget::GetDelegateViewOpaqueBackgroundLayerForTesting() {
  return delegate_view_->opaque_background_layer();
}

void ShelfWidget::OnMouseEvent(ui::MouseEvent* event) {
  if (event->IsMouseWheelEvent()) {
    ui::MouseWheelEvent* mouse_wheel_event = event->AsMouseWheelEvent();
    shelf_->ProcessMouseWheelEvent(mouse_wheel_event);
    return;
  }

  if (event->type() == ui::EventType::kMousePressed) {
    keyboard::KeyboardUIController::Get()->HideKeyboardImplicitlyByUser();

    // If the shelf receives the mouse pressing event, the RootView of the shelf
    // will reset the gesture handler. As a result, if the shelf is in drag
    // progress when the mouse is pressed, shelf will not receive the gesture
    // end event. So explicitly cancel the drag in this scenario.
    shelf_layout_manager_->CancelDragOnShelfIfInProgress();
  }

  views::Widget::OnMouseEvent(event);
}

void ShelfWidget::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::EventType::kGestureTapDown) {
    keyboard::KeyboardUIController::Get()->HideKeyboardImplicitlyByUser();
  }
  ui::GestureEvent event_in_screen(*event);
  gfx::Point location_in_screen(event->location());
  ::wm::ConvertPointToScreen(GetNativeWindow(), &location_in_screen);
  event_in_screen.set_location(location_in_screen);

  // Tap on in-app shelf should show a contextual nudge for in-app to home
  // gesture.
  if (event->type() == ui::EventType::kGestureTap &&
      ShelfConfig::Get()->is_in_app() &&
      features::IsHideShelfControlsInTabletModeEnabled()) {
    if (delegate_view_->drag_handle()->MaybeShowDragHandleNudge()) {
      event->StopPropagation();
      return;
    }
  }

  shelf_layout_manager()->ProcessGestureEventFromShelfWidget(&event_in_screen);
  if (!event->handled())
    views::Widget::OnGestureEvent(event);
}

void ShelfWidget::OnScrollEvent(ui::ScrollEvent* event) {
  shelf_->ProcessScrollEvent(event);
  if (!event->handled())
    views::Widget::OnScrollEvent(event);
}

void ShelfWidget::ResetForceShowHotseat() {
  if (force_show_hotseat_count_ == 0)
    return;
  --force_show_hotseat_count_;

  if (force_show_hotseat_count_ == 0)
    shelf_layout_manager_->UpdateVisibilityState(/*force_layout=*/false);
}

}  // namespace ash
