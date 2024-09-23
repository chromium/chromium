// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_background_view.h"

#include <algorithm>
#include <memory>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/tray_background_view_catalog.h"
#include "ash/focus_cycler.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/login_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/style/style_util.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/system/tray/tray_event_filter.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/scoped_multi_source_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_abort_handle.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/painter.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"

namespace ash {
namespace {

const int kAnimationDurationForBubblePopupMs = 200;

// Duration of opacity animation for visibility changes.
constexpr base::TimeDelta kAnimationDurationForVisibilityMs =
    base::Milliseconds(250);

// Duration of opacity animation for hide animation.
constexpr base::TimeDelta kAnimationDurationForHideMs = base::Milliseconds(100);

// Bounce animation constants
const base::TimeDelta kAnimationDurationForBounceElement =
    base::Milliseconds(250);
const int kAnimationBounceUpDistance = 16;
const int kAnimationBounceDownDistance = 8;
const float kAnimationBounceScaleFactor = 0.5;

// When becoming visible delay the animation so that StatusAreaWidgetDelegate
// can animate sibling views out of the position to be occupied by the
// TrayBackgroundView.
const base::TimeDelta kShowAnimationDelayMs = base::Milliseconds(100);

// Ripple and pulsing animation constants
const float kNormalScaleFactor = 1.0f;
const float kPulseScaleUpFactor = 1.2f;
const float kRippleScaleUpFactor = 3.0f;
const float kRippleLayerStartingOpacity = 0.5f;
const float kRippleLayerEndOpacity = 0.0f;
constexpr base::TimeDelta kPulseEnlargeAnimationTime = base::Milliseconds(500);
constexpr base::TimeDelta kPulseShrinkAnimationTime = base::Milliseconds(1350);
constexpr base::TimeDelta kRippleAnimationTime = base::Milliseconds(2000);
constexpr base::TimeDelta kPulseAnimationCoolDownTime = base::Seconds(5);

// Number of active requests to disable CloseBubble().
int g_disable_close_bubble_on_window_activated = 0;

constexpr char kFadeInAnimationSmoothnessHistogramName[] =
    "Ash.StatusArea.TrayBackgroundView.FadeIn";
constexpr char kBounceInAnimationSmoothnessHistogramName[] =
    "Ash.StatusArea.TrayBackgroundView.BounceIn";
constexpr char kHideAnimationSmoothnessHistogramName[] =
    "Ash.StatusArea.TrayBackgroundView.Hide";

// Switches left and right insets if RTL mode is active.
void MirrorInsetsIfNecessary(gfx::Insets* insets) {
  if (base::i18n::IsRTL()) {
    *insets = gfx::Insets::TLBR(insets->top(), insets->right(),
                                insets->bottom(), insets->left());
  }
}

// Returns background insets relative to the contents bounds of the view and
// mirrored if RTL mode is active.
gfx::Insets GetMirroredBackgroundInsets(bool is_shelf_horizontal) {
  gfx::Insets insets;
  // "Primary" is the same direction as the shelf, "secondary" is orthogonal.
  const int primary_padding = 0;
  const int secondary_padding =
      -ash::ShelfConfig::Get()->status_area_hit_region_padding();

  if (is_shelf_horizontal) {
    insets = gfx::Insets::VH(secondary_padding, primary_padding);
  } else {
    insets = gfx::Insets::VH(primary_padding, secondary_padding);
  }
  MirrorInsetsIfNecessary(&insets);
  return insets;
}

const gfx::Transform GetScaledTransform(const gfx::PointF center_point,
                                        float scale) {
  gfx::Transform scale_transform;
  scale_transform.Scale3d(scale, scale, 1);
  return gfx::TransformAboutPivot(center_point, scale_transform);
}

class HighlightPathGenerator : public views::HighlightPathGenerator {
 public:
  explicit HighlightPathGenerator(TrayBackgroundView* tray_background_view)
      : tray_background_view_(tray_background_view), insets_(gfx::Insets()) {}

  HighlightPathGenerator(TrayBackgroundView* tray_background_view,
                         gfx::Insets insets)
      : tray_background_view_(tray_background_view), insets_(insets) {}

  HighlightPathGenerator(const HighlightPathGenerator&) = delete;
  HighlightPathGenerator& operator=(const HighlightPathGenerator&) = delete;

  // HighlightPathGenerator:
  std::optional<gfx::RRectF> GetRoundRect(const gfx::RectF& rect) override {
    gfx::RectF bounds(tray_background_view_->GetBackgroundBounds());
    bounds.Inset(gfx::InsetsF(insets_));
    return gfx::RRectF(bounds, tray_background_view_->GetRoundedCorners());
  }

 private:
  const raw_ptr<TrayBackgroundView> tray_background_view_;
  const gfx::Insets insets_;
};

}  // namespace

TrayBackgroundView::TrayButtonControllerDelegate::TrayButtonControllerDelegate(
    views::Button* button,
    TrayBackgroundViewCatalogName catalogue_name)
    : views::Button::DefaultButtonControllerDelegate(button),
      catalog_name_(catalogue_name) {}

void TrayBackgroundView::TrayButtonControllerDelegate::NotifyClick(
    const ui::Event& event) {
  base::UmaHistogramEnumeration("Ash.StatusArea.TrayBackgroundView.Pressed",
                                catalog_name_);
  DefaultButtonControllerDelegate::NotifyClick(event);
}

// Used to track when the anchor widget changes position on screen so that the
// bubble position can be updated.
class TrayBackgroundView::TrayWidgetObserver : public views::WidgetObserver {
 public:
  explicit TrayWidgetObserver(TrayBackgroundView* host) : host_(host) {}

  TrayWidgetObserver(const TrayWidgetObserver&) = delete;
  TrayWidgetObserver& operator=(const TrayWidgetObserver&) = delete;

  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override {
    host_->AnchorUpdated();
  }

  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override {
    host_->AnchorUpdated();
  }

  void Add(views::Widget* widget) { observations_.AddObservation(widget); }

 private:
  raw_ptr<TrayBackgroundView> host_;
  base::ScopedMultiSourceObservation<views::Widget, views::WidgetObserver>
      observations_{this};
};

// Handles `TrayBackgroundView`'s animation on session changed.
class TrayBackgroundView::TrayBackgroundViewSessionChangeHandler
    : public SessionObserver {
 public:
  explicit TrayBackgroundViewSessionChangeHandler(
      TrayBackgroundView* tray_background_view)
      : tray_(tray_background_view) {
    DCHECK(tray_);
  }
  TrayBackgroundViewSessionChangeHandler(
      const TrayBackgroundViewSessionChangeHandler&) = delete;
  TrayBackgroundViewSessionChangeHandler& operator=(
      const TrayBackgroundViewSessionChangeHandler&) = delete;
  ~TrayBackgroundViewSessionChangeHandler() override = default;

 private:  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override {
    DisableShowAnimationInSequence();
  }
  void OnActiveUserSessionChanged(const AccountId& account_id) override {
    DisableShowAnimationInSequence();
  }

  // Disables the `TrayBackgroundView`'s show animation until all queued tasks
  // in the current task sequence are run.
  void DisableShowAnimationInSequence() {
    base::ScopedClosureRunner callback = tray_->DisableShowAnimation();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, callback.Release());
  }

  const raw_ptr<TrayBackgroundView> tray_;
  ScopedSessionObserver session_observer_{this};
};

////////////////////////////////////////////////////////////////////////////////
// TrayBackgroundView

TrayBackgroundView::TrayBackgroundView(
    Shelf* shelf,
    const TrayBackgroundViewCatalogName catalog_name,
    RoundedCornerBehavior corner_behavior)
    : shelf_(shelf),
      catalog_name_(catalog_name),
      tray_container_(new TrayContainer(shelf, this)),
      is_active_(false),
      separator_visible_(true),
      visible_preferred_(false),
      show_with_virtual_keyboard_(false),
      show_when_collapsed_(true),
      corner_behavior_(corner_behavior),
      widget_observer_(new TrayWidgetObserver(this)),
      handler_(new TrayBackgroundViewSessionChangeHandler(this)),
      should_close_bubble_on_lock_state_change_(true) {
  DCHECK(shelf_);
  SetButtonController(std::make_unique<views::ButtonController>(
      this,
      std::make_unique<TrayButtonControllerDelegate>(this, catalog_name)));
  SetNotifyEnterExitOnChild(true);

  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetInstallFocusRingOnFocus(true);

  views::FocusRing* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetOutsetFocusRingDisabled(true);
  focus_ring->SetPathGenerator(std::make_unique<HighlightPathGenerator>(
      this, kTrayBackgroundFocusPadding));
  focus_ring->SetColorId(ui::kColorAshFocusRing);
  SetFocusPainter(nullptr);

  views::HighlightPathGenerator::Install(
      this, std::make_unique<HighlightPathGenerator>(this));

  AddChildView(tray_container_.get());

  // Use layer color to provide background color. Note that children views
  // need to have their own layers to be visible.
  SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  layer()->SetFillsBoundsOpaquely(false);

  // Start the tray items not visible, because visibility changes are animated.
  views::View::SetVisible(false);
  layer()->SetOpacity(0.0f);
}

void TrayBackgroundView::AddTrayBackgroundViewObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TrayBackgroundView::RemoveTrayBackgroundViewObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

TrayBackgroundView::~TrayBackgroundView() {
  StopPulseAnimation();
  Shell::Get()->system_tray_model()->virtual_keyboard()->RemoveObserver(this);
  widget_observer_.reset();
  handler_.reset();
}

void TrayBackgroundView::Initialize() {
  widget_observer_->Add(GetWidget());
  Shell::Get()->system_tray_model()->virtual_keyboard()->AddObserver(this);

  UpdateBackground();
}

// static
void TrayBackgroundView::InitializeBubbleAnimations(
    views::Widget* bubble_widget) {
  aura::Window* window = bubble_widget->GetNativeWindow();
  ::wm::SetWindowVisibilityAnimationType(
      window, ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  ::wm::SetWindowVisibilityAnimationTransition(window, ::wm::ANIMATE_HIDE);
  ::wm::SetWindowVisibilityAnimationDuration(
      window, base::Milliseconds(kAnimationDurationForBubblePopupMs));
}

void TrayBackgroundView::SetVisiblePreferred(bool visible_preferred) {
  if (visible_preferred_ == visible_preferred) {
    return;
  }

  visible_preferred_ = visible_preferred;
  for (auto& observer : observers_) {
    observer.OnVisiblePreferredChanged(visible_preferred_);
  }

  TrackVisibilityUMA(visible_preferred);

  // Calling `StartVisibilityAnimation(GetEffectiveVisibility())` doesn't work
  // for the case of a collapsed status area (see b/265165818). Passing
  // `visible_preferred_` is better, but also means that animations happen for
  // all trays, even those that would show/hide in the "hidden" part of a
  // collapsed status area (but note that those animations are not visible until
  // the status area is expanded).
  StartVisibilityAnimation(visible_preferred_);

  // We need to update which trays overflow after showing or hiding a tray.
  // If the hide animation is still playing, we do the `UpdateStatusArea(bool
  // should_log_visible_pod_count)` when the animation is finished.
  if (!layer()->GetAnimator()->is_animating() || visible_preferred_) {
    UpdateStatusArea(true /*should_log_visible_pod_count*/);
  }
}

bool TrayBackgroundView::IsShowingMenu() const {
  return context_menu_runner_ && context_menu_runner_->IsRunning();
}

void TrayBackgroundView::SetRoundedCornerBehavior(
    RoundedCornerBehavior corner_behavior) {
  corner_behavior_ = corner_behavior;
  UpdateBackground();
}

gfx::RoundedCornersF TrayBackgroundView::GetRoundedCorners() {
  const float radius = ShelfConfig::Get()->control_border_radius();
  if (shelf_->IsHorizontalAlignment()) {
    gfx::RoundedCornersF start_rounded = {
        radius, kUnifiedTrayNonRoundedSideRadius,
        kUnifiedTrayNonRoundedSideRadius, radius};
    gfx::RoundedCornersF end_rounded = {kUnifiedTrayNonRoundedSideRadius,
                                        radius, radius,
                                        kUnifiedTrayNonRoundedSideRadius};
    switch (corner_behavior_) {
      case kNotRounded:
        return {
            kUnifiedTrayNonRoundedSideRadius, kUnifiedTrayNonRoundedSideRadius,
            kUnifiedTrayNonRoundedSideRadius, kUnifiedTrayNonRoundedSideRadius};
      case kAllRounded:
        return {radius, radius, radius, radius};
      case kStartRounded:
        return base::i18n::IsRTL() ? end_rounded : start_rounded;
      case kEndRounded:
        return base::i18n::IsRTL() ? start_rounded : end_rounded;
    }
  }

  switch (corner_behavior_) {
    case kNotRounded:
      return {
          kUnifiedTrayNonRoundedSideRadius, kUnifiedTrayNonRoundedSideRadius,
          kUnifiedTrayNonRoundedSideRadius, kUnifiedTrayNonRoundedSideRadius};
    case kAllRounded:
      return {radius, radius, radius, radius};
    case kStartRounded:
      return {radius, radius, kUnifiedTrayNonRoundedSideRadius,
              kUnifiedTrayNonRoundedSideRadius};
    case kEndRounded:
      return {kUnifiedTrayNonRoundedSideRadius,
              kUnifiedTrayNonRoundedSideRadius, radius, radius};
  }
}

base::WeakPtr<TrayBackgroundView> TrayBackgroundView::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool TrayBackgroundView::IsShowAnimationEnabled() {
  return disable_show_animation_count_ == 0u;
}

void TrayBackgroundView::StartVisibilityAnimation(bool visible) {
  if (visible == layer()->GetTargetVisibility()) {
    return;
  }

  base::AutoReset<bool> is_starting_animation(&is_starting_animation_, true);

  if (visible) {
    views::View::SetVisible(true);
    // If SetVisible(true) is called while animating to not visible, then
    // views::View::SetVisible(true) is a no-op. When the previous animation
    // ends layer->SetVisible(false) is called. To prevent this
    // layer->SetVisible(true) immediately interrupts the animation of this
    // property, and keeps the layer visible.
    layer()->SetVisible(true);

    // We only show visible animation when `IsShowAnimationEnabled()`.
    if (IsShowAnimationEnabled()) {
      // We only show default animations when
      // `ShouldUseCustomVisibilityAnimations()` is false.
      if (ShouldUseCustomVisibilityAnimations()) {
        return;
      }
      if (use_bounce_in_animation_) {
        BounceInAnimation();
      } else {
        FadeInAnimation();
      }
    } else {
      // The opacity and scale of the `layer()` may have been manipulated, so
      // reset it before it is shown.
      layer()->SetOpacity(1.0f);
      layer()->SetTransform(gfx::Transform());
      OnVisibilityAnimationFinished(/*should_log_visible_pod_count=*/false,
                                    /*aborted=*/false);
    }
  } else if (!ShouldUseCustomVisibilityAnimations()) {
    // We only show default animations when
    // `ShouldUseCustomVisibilityAnimations()` is false.
    // If the visibility snapped to hidden instead of showing animation first,
    // make sure to call OnVisibilityAnimationFinished
    HideAnimation();
  }
}

base::ScopedClosureRunner TrayBackgroundView::DisableShowAnimation() {
  if (layer()->GetAnimator()->is_animating()) {
    layer()->GetAnimator()->StopAnimating();
  }

  ++disable_show_animation_count_;
  if (disable_show_animation_count_ == 1u) {
    OnShouldShowAnimationChanged(false);
  }

  return base::ScopedClosureRunner(base::BindOnce(
      [](const base::WeakPtr<TrayBackgroundView>& ptr) {
        if (ptr) {
          --ptr->disable_show_animation_count_;
          if (ptr->IsShowAnimationEnabled()) {
            ptr->OnShouldShowAnimationChanged(true);
          }
        }
      },
      weak_factory_.GetWeakPtr()));
}

base::ScopedClosureRunner
TrayBackgroundView::SetUseCustomVisibilityAnimations() {
  ++use_custom_visibility_animation_count_;
  return base::ScopedClosureRunner(base::BindOnce(
      [](const base::WeakPtr<TrayBackgroundView>& ptr) {
        if (ptr) {
          ptr->use_custom_visibility_animation_count_--;
        }
      },
      weak_factory_.GetWeakPtr()));
}

base::ScopedClosureRunner
TrayBackgroundView::DisableCloseBubbleOnWindowActivated() {
  ++g_disable_close_bubble_on_window_activated;
  return base::ScopedClosureRunner(
      base::BindOnce([]() { --g_disable_close_bubble_on_window_activated; }));
}

// static
bool TrayBackgroundView::ShouldCloseBubbleOnWindowActivated() {
  return g_disable_close_bubble_on_window_activated == 0;
}

void TrayBackgroundView::UpdateStatusArea(bool should_log_visible_pod_count) {
  auto* status_area_widget = shelf_->GetStatusAreaWidget();
  if (status_area_widget) {
    status_area_widget->UpdateCollapseState();
    if (should_log_visible_pod_count) {
      status_area_widget->LogVisiblePodCountMetric();
    }
  }
}

void TrayBackgroundView::UpdateAfterLockStateChange(bool locked) {
  if (should_close_bubble_on_lock_state_change_) {
    CloseBubble();
  }
}

void TrayBackgroundView::OnVisibilityAnimationFinished(
    bool should_log_visible_pod_count,
    bool aborted) {
  SetCanProcessEventsWithinSubtree(true);
  if (aborted && is_starting_animation_) {
    return;
  }
  if (!visible_preferred_) {
    views::View::SetVisible(false);
    UpdateStatusArea(should_log_visible_pod_count);
  }
}

void TrayBackgroundView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  context_menu_model_ = CreateContextMenuModel();
  if (!context_menu_model_) {
    return;
  }

  const int run_types = views::MenuRunner::USE_ASH_SYS_UI_LAYOUT |
                        views::MenuRunner::CONTEXT_MENU |
                        views::MenuRunner::FIXED_ANCHOR;
  context_menu_runner_ = std::make_unique<views::MenuRunner>(
      context_menu_model_.get(), run_types,
      base::BindRepeating(&Shelf::UpdateAutoHideState,
                          base::Unretained(shelf_)));
  views::MenuAnchorPosition anchor;
  switch (shelf_->alignment()) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      anchor = views::MenuAnchorPosition::kBubbleTopRight;
      break;
    case ShelfAlignment::kLeft:
      anchor = views::MenuAnchorPosition::kBubbleRight;
      break;
    case ShelfAlignment::kRight:
      anchor = views::MenuAnchorPosition::kBubbleLeft;
      break;
  }

  context_menu_runner_->RunMenuAt(
      source->GetWidget(), /*button_controller=*/nullptr,
      source->GetBoundsInScreen(), anchor, source_type);
}

void TrayBackgroundView::AboutToRequestFocusFromTabTraversal(bool reverse) {
  Shelf* shelf = Shelf::ForWindow(GetWidget()->GetNativeWindow());
  StatusAreaWidgetDelegate* delegate =
      shelf->GetStatusAreaWidget()->status_area_widget_delegate();
  if (!delegate || !delegate->ShouldFocusOut(reverse)) {
    return;
  }

  shelf_->shelf_focus_cycler()->FocusOut(reverse, SourceView::kStatusAreaView);
}

void TrayBackgroundView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  views::Button::GetAccessibleNodeData(node_data);
  // Override the name set in `LabelButton::SetText`.
  // TODO(crbug.com/325137417): Remove this once the accessible name is set in
  // the cache as soon as the name is updated.
  GetViewAccessibility().SetName(GetAccessibleNameForTray());

  if (LockScreen::HasInstance()) {
    GetViewAccessibility().SetNextFocus(LockScreen::Get()->widget());
  }

  Shelf* shelf = Shelf::ForWindow(GetWidget()->GetNativeWindow());
  ShelfWidget* shelf_widget = shelf->shelf_widget();
  GetViewAccessibility().SetPreviousFocus(shelf_widget->hotseat_widget());
  GetViewAccessibility().SetNextFocus(shelf_widget->navigation_widget());
}

void TrayBackgroundView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

std::unique_ptr<ui::Layer> TrayBackgroundView::RecreateLayer() {
  if (layer()->GetAnimator()->is_animating()) {
    OnVisibilityAnimationFinished(/*should_log_visible_pod_count=*/false,
                                  /*aborted=*/false);
  }

  return views::View::RecreateLayer();
}

void TrayBackgroundView::OnThemeChanged() {
  views::Button::OnThemeChanged();
  UpdateBackground();
}

void TrayBackgroundView::OnVirtualKeyboardVisibilityChanged() {
  // We call the base class' SetVisible to skip animations.
  if (GetVisible() != GetEffectiveVisibility()) {
    views::View::SetVisible(GetEffectiveVisibility());
  }
}

TrayBubbleView* TrayBackgroundView::GetBubbleView() {
  return nullptr;
}

views::Widget* TrayBackgroundView::GetBubbleWidget() const {
  return nullptr;
}

void TrayBackgroundView::ShowBubble() {}

void TrayBackgroundView::CalculateTargetBounds() {
  tray_container_->CalculateTargetBounds();
}

void TrayBackgroundView::UpdateLayout() {
  UpdateBackground();
  tray_container_->UpdateLayout();
}

void TrayBackgroundView::UpdateAfterLoginStatusChange() {
  // Handled in subclasses.
}

void TrayBackgroundView::UpdateAfterStatusAreaCollapseChange() {
  views::View::SetVisible(GetEffectiveVisibility());
}

void TrayBackgroundView::UpdateBackground() {
  layer()->SetRoundedCornerRadius(GetRoundedCorners());
  layer()->SetIsFastRoundedCorner(true);
  layer()->SetBackgroundBlur(
      ShelfConfig::Get()->GetShelfControlButtonBlurRadius());
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
  layer()->SetClipRect(GetBackgroundBounds());

  const views::Widget* widget = GetWidget();
  if (widget) {
    layer()->SetColor(ShelfConfig::Get()->GetShelfControlButtonColor(widget));
  }
  UpdateBackgroundColor(is_active_);
}

void TrayBackgroundView::OnHideAnimationStarted() {
  // Disable event handling while the hide animation is running. It will be
  // re-enabled when the animation is finished or aborted.
  SetCanProcessEventsWithinSubtree(false);
}

void TrayBackgroundView::OnAnimationAborted() {
  OnVisibilityAnimationFinished(/*should_log_visible_pod_count=*/true,
                                /*aborted=*/true);
}
void TrayBackgroundView::OnAnimationEnded() {
  OnVisibilityAnimationFinished(/*should_log_visible_pod_count=*/true,
                                /*aborted=*/false);
}

void TrayBackgroundView::FadeInAnimation() {
  gfx::Transform transform;
  if (shelf_->IsHorizontalAlignment()) {
    transform.Translate(width(), 0.0f);
  } else {
    transform.Translate(0.0f, height());
  }

  ui::AnimationThroughputReporter reporter(
      layer()->GetAnimator(),
      metrics_util::ForSmoothnessV3(base::BindRepeating([](int smoothness) {
        DCHECK(0 <= smoothness && smoothness <= 100);
        base::UmaHistogramPercentage(kFadeInAnimationSmoothnessHistogramName,
                                     smoothness);
      })));

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnAborted(base::BindOnce(
          [](base::WeakPtr<TrayBackgroundView> view) {
            if (view) {
              view->OnAnimationAborted();
            }
          },
          weak_factory_.GetWeakPtr()))
      .OnEnded(base::BindOnce(
          [](base::WeakPtr<TrayBackgroundView> view) {
            if (view) {
              view->OnAnimationEnded();
            }
          },
          weak_factory_.GetWeakPtr()))
      .Once()
      .SetDuration(kShowAnimationDelayMs)
      .Then()
      .SetDuration(base::TimeDelta())
      .SetOpacity(this, 0.0f)
      .SetTransform(this, transform)
      .Then()
      .SetDuration(kAnimationDurationForVisibilityMs)
      .SetOpacity(this, 1.0f)
      .SetTransform(this, gfx::Transform());
}

// Any visibility updates should be called after the hide animation is
// finished, otherwise the view will disappear immediately without animation
// once the view's visibility is set to false.
void TrayBackgroundView::HideAnimation() {
  gfx::Transform scale;
  scale.Scale3d(kAnimationBounceScaleFactor, kAnimationBounceScaleFactor, 1);

  const gfx::Transform scale_about_pivot = gfx::TransformAboutPivot(
      gfx::RectF(GetLocalBounds()).CenterPoint(), scale);

  ui::AnimationThroughputReporter reporter(
      layer()->GetAnimator(),
      metrics_util::ForSmoothnessV3(base::BindRepeating([](int smoothness) {
        DCHECK(0 <= smoothness && smoothness <= 100);
        base::UmaHistogramPercentage(kHideAnimationSmoothnessHistogramName,
                                     smoothness);
      })));

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnStarted(base::BindOnce(
          [](base::WeakPtr<TrayBackgroundView> view) {
            if (view) {
              view->OnHideAnimationStarted();
            }
          },
          weak_factory_.GetWeakPtr()))
      .OnAborted(base::BindOnce(
          [](base::WeakPtr<TrayBackgroundView> view) {
            if (view) {
              view->OnAnimationAborted();
            }
          },
          weak_factory_.GetWeakPtr()))
      .OnEnded(base::BindOnce(
          [](base::WeakPtr<TrayBackgroundView> view) {
            if (view) {
              view->OnAnimationEnded();
            }
          },
          weak_factory_.GetWeakPtr()))
      .Once()
      .SetDuration(kAnimationDurationForHideMs)
      .SetVisibility(this, false)
      .SetTransform(this, std::move(scale_about_pivot))
      .SetOpacity(this, 0.0f);
}

void TrayBackgroundView::SetIsActive(bool is_active) {
  if (is_active_ == is_active) {
    return;
  }
  is_active_ = is_active;
  UpdateBackgroundColor(is_active);
  UpdateTrayItemColor(is_active);
}

void TrayBackgroundView::CloseBubble() {
  CloseBubbleInternal();

  // If ChromeVox is enabled, focus on the this tray when the bubble is closed.
  if (Shell::Get()->accessibility_controller() &&
      Shell::Get()->accessibility_controller()->spoken_feedback().enabled()) {
    shelf_->shelf_focus_cycler()->FocusStatusArea(false);
    RequestFocus();
  }
}

views::View* TrayBackgroundView::GetBubbleAnchor() const {
  return tray_container_;
}

gfx::Insets TrayBackgroundView::GetBubbleAnchorInsets() const {
  gfx::Insets anchor_insets = GetBubbleAnchor()->GetInsets();
  gfx::Insets tray_bg_insets = GetInsets();
  if (shelf_->alignment() == ShelfAlignment::kBottom ||
      shelf_->alignment() == ShelfAlignment::kBottomLocked) {
    return gfx::Insets::TLBR(-tray_bg_insets.top(), anchor_insets.left(),
                             -tray_bg_insets.bottom(), anchor_insets.right());
  } else {
    return gfx::Insets::TLBR(anchor_insets.top(), -tray_bg_insets.left(),
                             anchor_insets.bottom(), -tray_bg_insets.right());
  }
}

aura::Window* TrayBackgroundView::GetBubbleWindowContainer() {
  return Shell::GetContainer(
      tray_container()->GetWidget()->GetNativeWindow()->GetRootWindow(),
      kShellWindowId_SettingBubbleContainer);
}

gfx::Rect TrayBackgroundView::GetBackgroundBounds() const {
  gfx::Rect bounds = GetLocalBounds();
  bounds.Inset(GetBackgroundInsets());
  return bounds;
}

void TrayBackgroundView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  UpdateBackground();

  views::Button::OnBoundsChanged(previous_bounds);
}

bool TrayBackgroundView::ShouldEnterPushedState(const ui::Event& event) {
  if (is_active_) {
    return false;
  }

  return views::Button::ShouldEnterPushedState(event);
}

std::unique_ptr<ui::SimpleMenuModel>
TrayBackgroundView::CreateContextMenuModel() {
  return nullptr;
}

void TrayBackgroundView::StartPulseAnimation() {
  // Do not start animation when animations are set to ZERO_DURATION (in tests).
  if (ui::ScopedAnimationDurationScaleMode::is_zero()) {
    return;
  }

  // Stop any ongoing pulse animation before starting new a new one.
  StopPulseAnimation();

  AddRippleLayer();

  PlayPulseAnimation();
}

void TrayBackgroundView::StopPulseAnimation() {
  pulse_animation_cool_down_timer_.Stop();
  ripple_and_pulse_animation_abort_handle_.reset();
  const gfx::Transform normal_transform = GetScaledTransform(
      gfx::RectF(GetLocalBounds()).CenterPoint(), kNormalScaleFactor);
  layer()->SetTransform(normal_transform);
  RemoveRippleLayer();
}

void TrayBackgroundView::BounceInAnimation(bool scale_animation) {
  gfx::Vector2dF bounce_up_location;
  gfx::Vector2dF bounce_down_location;

  switch (shelf_->alignment()) {
    case ShelfAlignment::kLeft:
      bounce_up_location = gfx::Vector2dF(kAnimationBounceUpDistance, 0);
      bounce_down_location = gfx::Vector2dF(-kAnimationBounceDownDistance, 0);
      break;
    case ShelfAlignment::kRight:
      bounce_up_location = gfx::Vector2dF(-kAnimationBounceUpDistance, 0);
      bounce_down_location = gfx::Vector2dF(kAnimationBounceDownDistance, 0);
      break;
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
    default:
      bounce_up_location = gfx::Vector2dF(0, -kAnimationBounceUpDistance);
      bounce_down_location = gfx::Vector2dF(0, kAnimationBounceDownDistance);
  }

  gfx::Transform initial_scale;
  const float scale_factor = scale_animation ? kAnimationBounceScaleFactor : 1;
  initial_scale.Scale3d(scale_factor, scale_factor, 1);

  const gfx::Transform initial_state = gfx::TransformAboutPivot(
      gfx::RectF(GetLocalBounds()).CenterPoint(), initial_scale);

  gfx::Transform scale_about_pivot = gfx::TransformAboutPivot(
      gfx::RectF(GetLocalBounds()).CenterPoint(), gfx::Transform());
  scale_about_pivot.Translate(bounce_up_location);

  gfx::Transform move_down;
  move_down.Translate(bounce_down_location);

  ui::AnimationThroughputReporter reporter(
      layer()->GetAnimator(),
      metrics_util::ForSmoothnessV3(base::BindRepeating([](int smoothness) {
        DCHECK(0 <= smoothness && smoothness <= 100);
        base::UmaHistogramPercentage(kBounceInAnimationSmoothnessHistogramName,
                                     smoothness);
      })));

  // Alias to avoid difficult to read line wrapping below.
  using ConstantTransform = ui::InterpolatedConstantTransform;
  using MatrixTransform = ui::InterpolatedMatrixTransform;

  // NOTE: It is intentional that `ui::InterpolatedTransform`s be used below
  // rather than `gfx::Transform`s which could otherwise be used to accomplish
  // the same animation.
  //
  // This is because the latter only informs layer delegates of transform
  // changes on animation completion whereas the former informs layer delegates
  // of transform changes at each animation step [1].
  //
  // Failure to inform layer delegates of transform changes at each animation
  // step can result in the ink drop layer going out of sync if the
  // `TrayBackgroundView`s activation state changes while an animation is in
  // progress.
  //
  // [1] See discussion in https://crrev.com/c/4304899.
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnAborted(base::BindOnce(
          [](base::WeakPtr<TrayBackgroundView> view) {
            if (view) {
              view->OnAnimationAborted();
            }
          },
          weak_factory_.GetWeakPtr()))
      .OnEnded(base::BindOnce(
          [](base::WeakPtr<TrayBackgroundView> view) {
            if (view) {
              view->OnAnimationEnded();
            }
          },
          weak_factory_.GetWeakPtr()))
      .Once()
      .SetDuration(base::TimeDelta())
      .SetOpacity(this, 1.0)
      .SetInterpolatedTransform(
          this, std::make_unique<ConstantTransform>(initial_state))
      .Then()
      .SetDuration(kAnimationDurationForBounceElement)
      .SetInterpolatedTransform(
          this,
          std::make_unique<MatrixTransform>(initial_state, scale_about_pivot),
          gfx::Tween::FAST_OUT_SLOW_IN_3)
      .Then()
      .SetDuration(kAnimationDurationForBounceElement)
      .SetInterpolatedTransform(
          this, std::make_unique<MatrixTransform>(scale_about_pivot, move_down),
          gfx::Tween::EASE_OUT_4)
      .Then()
      .SetDuration(kAnimationDurationForBounceElement)
      .SetInterpolatedTransform(
          this, std::make_unique<MatrixTransform>(move_down, gfx::Transform()),
          gfx::Tween::FAST_OUT_SLOW_IN_3);
}

void TrayBackgroundView::TrackVisibilityUMA(bool visible_preferred) const {
  base::UmaHistogramEnumeration(
      visible_preferred ? "Ash.StatusArea.TrayBackgroundView.Shown"
                        : "Ash.StatusArea.TrayBackgroundView.Hidden",
      catalog_name_);
}

views::PaintInfo::ScaleType TrayBackgroundView::GetPaintScaleType() const {
  return views::PaintInfo::ScaleType::kUniformScaling;
}

gfx::Insets TrayBackgroundView::GetBackgroundInsets() const {
  gfx::Insets insets =
      GetMirroredBackgroundInsets(shelf_->IsHorizontalAlignment());

  // |insets| are relative to contents bounds. Change them to be relative to
  // local bounds.
  gfx::Insets local_contents_insets =
      GetLocalBounds().InsetsFrom(GetContentsBounds());
  MirrorInsetsIfNecessary(&local_contents_insets);
  insets += local_contents_insets;

  if (Shell::Get()->IsInTabletMode() && ShelfConfig::Get()->is_in_app()) {
    insets +=
        gfx::Insets(ShelfConfig::Get()->in_app_control_button_height_inset());
  }

  return insets;
}

bool TrayBackgroundView::GetEffectiveVisibility() {
  // When the virtual keyboard is visible, the effective visibility of the view
  // is solely determined by |show_with_virtual_keyboard_|.
  if (Shell::Get()
          ->system_tray_model()
          ->virtual_keyboard()
          ->arc_keyboard_visible()) {
    return show_with_virtual_keyboard_;
  }

  if (!visible_preferred_) {
    return false;
  }

  DCHECK(GetWidget());

  // When the status area is collapsed, the effective visibility of the view is
  // determined by |show_when_collapsed_|.
  StatusAreaWidget::CollapseState collapse_state =
      Shelf::ForWindow(GetWidget()->GetNativeWindow())
          ->GetStatusAreaWidget()
          ->collapse_state();
  if (collapse_state == StatusAreaWidget::CollapseState::COLLAPSED) {
    return show_when_collapsed_;
  }

  return true;
}

bool TrayBackgroundView::ShouldUseCustomVisibilityAnimations() const {
  return use_custom_visibility_animation_count_ > 0u;
}

bool TrayBackgroundView::CacheBubbleViewForHide() const {
  return false;
}

void TrayBackgroundView::UpdateBackgroundColor(bool active) {
  auto* widget = GetWidget();
  if (!widget) {
    return;
  }

  // The shelf is not transparent when 1)the shelf is in app mode OR 2) the
  // shelf is in the regular logged in page (not session blocked).
  bool is_shelf_opaque =
      (!Shell::Get()->IsInTabletMode() || ShelfConfig::Get()->is_in_app()) &&
      !Shell::Get()->session_controller()->IsUserSessionBlocked();
  ui::ColorId non_active_color_id =
      is_shelf_opaque ? cros_tokens::kCrosSysSystemOnBase
                      : cros_tokens::kCrosSysSystemBaseElevated;
  layer()->SetColor(widget->GetColorProvider()->GetColor(
      active ? cros_tokens::kCrosSysSystemPrimaryContainer
             : non_active_color_id));
}

void TrayBackgroundView::AddRippleLayer() {
  ripple_layer_ = std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
  ripple_layer_->SetColor(
      GetColorProvider()->GetColor(cros_tokens::kCrosSysOnPrimaryContainer));
  layer()->parent()->Add(ripple_layer_.get());
}

void TrayBackgroundView::RemoveRippleLayer() {
  CHECK(!pulse_animation_cool_down_timer_.IsRunning());
  if (ripple_layer_) {
    // The `parent_layer` will be null during `StatusAreaWidgetDelegate`
    // shutdown (ie. after display disconnect).
    // `views::view::RemoveAllChildViews()` is called, which recursively orphans
    // layers prior to destroying the view.
    auto* parent_layer = layer()->parent();
    if (parent_layer) {
      parent_layer->Remove(ripple_layer_.get());
    }
    ripple_layer_.reset();
  }
}

void TrayBackgroundView::PlayPulseAnimation() {
  // `ripple_layer_` must exist when calling `PlayPulseAnimation()`.
  CHECK(ripple_layer_);

  using ConstantTransform = ui::InterpolatedConstantTransform;
  using MatrixTransform = ui::InterpolatedMatrixTransform;

  const gfx::Rect background_bounds = GetBackgroundBounds();
  // `ripple_layer_` is at the same hierarchy of TrayBackgroundView so we need
  // to calculate the origin point using offset from both the tray and tray's
  // actual content.
  gfx::Rect ripple_layer_bound(
      gfx::PointAtOffsetFromOrigin(bounds().OffsetFromOrigin() +
                                   background_bounds.OffsetFromOrigin()),
      background_bounds.size());
  const gfx::RoundedCornersF rounded_corners = GetRoundedCorners();

  ripple_layer_->SetBounds(ripple_layer_bound);
  ripple_layer_->SetRoundedCornerRadius(rounded_corners);

  const gfx::Transform ripple_normal_transform = GetScaledTransform(
      gfx::RectF(gfx::SizeF(ripple_layer_->size())).CenterPoint(),
      kNormalScaleFactor);

  const gfx::Transform ripple_scale_up_transform = GetScaledTransform(
      gfx::RectF(gfx::SizeF(ripple_layer_->size())).CenterPoint(),
      kRippleScaleUpFactor);

  const gfx::Transform button_normal_transform = GetScaledTransform(
      gfx::RectF(GetLocalBounds()).CenterPoint(), kNormalScaleFactor);

  const gfx::Transform button_scale_up_transform = GetScaledTransform(
      gfx::RectF(GetLocalBounds()).CenterPoint(), kPulseScaleUpFactor);

  views::AnimationBuilder builder;
  ripple_and_pulse_animation_abort_handle_ = builder.GetAbortHandle();
  builder
      .OnEnded(
          base::BindOnce(&TrayBackgroundView::StartPulseAnimationCoolDownTimer,
                         base::Unretained(this)))
      .Once()
      .At(base::TimeDelta())
      .SetOpacity(ripple_layer_.get(), kRippleLayerStartingOpacity)
      .SetInterpolatedTransform(
          ripple_layer_.get(),
          std::make_unique<ConstantTransform>(ripple_normal_transform))
      .Then()
      .SetDuration(kRippleAnimationTime)
      .SetInterpolatedTransform(
          ripple_layer_.get(),
          std::make_unique<MatrixTransform>(ripple_normal_transform,
                                            ripple_scale_up_transform),
          gfx::Tween::ACCEL_0_40_DECEL_100)
      .SetOpacity(ripple_layer_.get(), kRippleLayerEndOpacity,
                  gfx::Tween::ACCEL_0_80_DECEL_80)
      .Offset(base::TimeDelta())
      .SetDuration(kPulseEnlargeAnimationTime)
      .SetInterpolatedTransform(
          /*target=*/this,
          std::make_unique<MatrixTransform>(button_normal_transform,
                                            button_scale_up_transform),
          gfx::Tween::ACCEL_40_DECEL_20)
      .Then()
      .SetDuration(kPulseShrinkAnimationTime)
      .SetInterpolatedTransform(
          /*target=*/this,
          std::make_unique<MatrixTransform>(button_scale_up_transform,
                                            button_normal_transform),
          gfx::Tween::ACCEL_20_DECEL_100);
}

void TrayBackgroundView::StartPulseAnimationCoolDownTimer() {
  pulse_animation_cool_down_timer_.Start(
      FROM_HERE, kPulseAnimationCoolDownTime,
      base::BindOnce(&TrayBackgroundView::PlayPulseAnimation,
                     base::Unretained(this)));
}

BEGIN_METADATA(TrayBackgroundView)
END_METADATA

}  // namespace ash
