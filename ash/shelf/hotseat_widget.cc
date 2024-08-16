// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/hotseat_widget.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/focus_cycler.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/hotseat_transition_animator.h"
#include "ash/shelf/scrollable_shelf_view.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/style/system_shadow.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_observer.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window_targeter.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace {

void DoScopedAnimationSetting(
    ui::ScopedLayerAnimationSettings* animation_setter) {
  animation_setter->SetTransitionDuration(
      ShelfConfig::Get()->shelf_animation_duration());
  animation_setter->SetTweenType(gfx::Tween::EASE_OUT);
  animation_setter->SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
}

// Returns whether there is special hotseat animation for |transition|.
bool HasSpecialAnimation(HotseatWidget::StateTransition transition) {
  switch (transition) {
    case HotseatWidget::StateTransition::kHomeLauncherAndExtended:
    case HotseatWidget::StateTransition::kHomeLauncherAndHidden:
      return true;
    case HotseatWidget::StateTransition::kHiddenAndExtended:
    case HotseatWidget::StateTransition::kOther:
      return false;
  }
}

// Calculates the state transition type for the given previous state and
// the target state.
HotseatWidget::StateTransition CalculateHotseatStateTransition(
    HotseatState previous_state,
    HotseatState target_state) {
  if (previous_state == HotseatState::kNone ||
      target_state == HotseatState::kNone) {
    return HotseatWidget::StateTransition::kOther;
  }

  if (previous_state == target_state)
    return HotseatWidget::StateTransition::kOther;

  const bool related_to_homelauncher =
      (previous_state == HotseatState::kShownHomeLauncher ||
       target_state == HotseatState::kShownHomeLauncher);
  const bool related_to_extended = (previous_state == HotseatState::kExtended ||
                                    target_state == HotseatState::kExtended);
  const bool related_to_hidden = (previous_state == HotseatState::kHidden ||
                                  target_state == HotseatState::kHidden);

  if (related_to_homelauncher && related_to_extended)
    return HotseatWidget::StateTransition::kHomeLauncherAndExtended;

  if (related_to_homelauncher && related_to_hidden)
    return HotseatWidget::StateTransition::kHomeLauncherAndHidden;

  if (related_to_extended && related_to_hidden)
    return HotseatWidget::StateTransition::kHiddenAndExtended;

  return HotseatWidget::StateTransition::kOther;
}

// Base class for hotseat animation transition.
class HotseatStateTransitionAnimation : public ui::LayerAnimationElement {
 public:
  HotseatStateTransitionAnimation(const gfx::Rect& target_bounds_in_screen,
                                  double target_opacity,
                                  ui::Layer* hotseat_layer,
                                  HotseatWidget* hotseat_widget)
      : ui::LayerAnimationElement(
            LayerAnimationElement::BOUNDS | LayerAnimationElement::OPACITY,
            hotseat_layer->GetAnimator()->GetTransitionDuration()),
        target_widget_bounds_(target_bounds_in_screen),
        target_opacity_(target_opacity),
        tween_type_(hotseat_layer->GetAnimator()->tween_type()),
        hotseat_widget_(hotseat_widget) {}

  ~HotseatStateTransitionAnimation() override = default;

  HotseatStateTransitionAnimation(const HotseatStateTransitionAnimation& rhs) =
      delete;
  HotseatStateTransitionAnimation& operator=(
      const HotseatStateTransitionAnimation& rhs) = delete;

 protected:
  // ui::LayerAnimationElement:
  void OnGetTarget(TargetValue* target) const override {
    target->opacity = target_opacity_;
    target->bounds = target_widget_bounds_;
  }

  ScrollableShelfView* GetScrollableShelfView() {
    return hotseat_widget_->scrollable_shelf_view();
  }

  // Hotseat widget's target bounds in screen.
  gfx::Rect target_widget_bounds_;

  // Hotseat widget's initial opacity.
  double start_opacity_ = 0.f;

  // Hotseat widget's target opacity.
  double target_opacity_ = 0.f;

  gfx::Tween::Type tween_type_ = gfx::Tween::LINEAR;

  raw_ptr<HotseatWidget> hotseat_widget_ = nullptr;
};

// Animation implemented specifically for the transition between the home
// launcher state and the extended state.
class HomeAndExtendedTransitionAnimation
    : public HotseatStateTransitionAnimation {
 public:
  HomeAndExtendedTransitionAnimation(const gfx::Rect& target_bounds_in_screen,
                                     double target_opacity,
                                     ui::Layer* hotseat_layer,
                                     HotseatWidget* hotseat_widget)
      : HotseatStateTransitionAnimation(target_bounds_in_screen,
                                        target_opacity,
                                        hotseat_layer,
                                        hotseat_widget) {}
  ~HomeAndExtendedTransitionAnimation() override = default;

  HomeAndExtendedTransitionAnimation(
      const HomeAndExtendedTransitionAnimation& rhs) = delete;
  HomeAndExtendedTransitionAnimation& operator=(
      const HomeAndExtendedTransitionAnimation& rhs) = delete;

 private:
  // HotseatStateTransitionAnimation:
  void OnStart(ui::LayerAnimationDelegate* delegate) override {
    DCHECK(hotseat_widget_->GetShelfView()->shelf()->IsHorizontalAlignment());

    ScrollableShelfView* scrollable_shelf_view = GetScrollableShelfView();
    scrollable_shelf_view->set_is_padding_configured_externally(
        /*is_padding_configured_externally=*/true);

    // Save initial and target padding insets.
    initial_padding_insets_ = scrollable_shelf_view->edge_padding_insets();
    target_padding_insets_ =
        scrollable_shelf_view->CalculateMirroredEdgePadding(
            /*use_target_bounds=*/true);

    // Save initial opacity.
    start_opacity_ = hotseat_widget_->GetNativeView()->layer()->opacity();

    // Save initial hotseat background bounds.
    initial_hotseat_background_in_screen_ =
        hotseat_widget_->GetWindowBoundsInScreen();
    initial_hotseat_background_in_screen_.Inset(initial_padding_insets_);

    // Save target hotseat background bounds.
    target_hotseat_background_in_screen_ = target_widget_bounds_;
    target_hotseat_background_in_screen_.Inset(target_padding_insets_);
  }

  // HotseatStateTransitionAnimation:
  bool OnProgress(double current,
                  ui::LayerAnimationDelegate* delegate) override {
    const double tweened = gfx::Tween::CalculateValue(tween_type_, current);

    // Set scrollable shelf view's padding insets.
    gfx::Insets insets_in_animation_progress;
    insets_in_animation_progress.set_left(gfx::Tween::LinearIntValueBetween(
        tweened, initial_padding_insets_.left(),
        target_padding_insets_.left()));
    insets_in_animation_progress.set_right(gfx::Tween::LinearIntValueBetween(
        tweened, initial_padding_insets_.right(),
        target_padding_insets_.right()));
    ScrollableShelfView* scrollable_shelf_view = GetScrollableShelfView();
    scrollable_shelf_view->SetEdgePaddingInsets(insets_in_animation_progress);

    // Update hotseat widget opacity.
    delegate->SetOpacityFromAnimation(
        gfx::Tween::DoubleValueBetween(tweened, start_opacity_,
                                       target_opacity_),
        ui::PropertyChangeReason::FROM_ANIMATION);

    // Calculate the hotseat widget's bounds.
    const gfx::Rect hotseat_background_in_progress =
        gfx::Tween::RectValueBetween(tweened,
                                     initial_hotseat_background_in_screen_,
                                     target_hotseat_background_in_screen_);
    gfx::Rect widget_bounds_in_progress = hotseat_background_in_progress;
    widget_bounds_in_progress.Inset(
        -scrollable_shelf_view->edge_padding_insets());

    // Update hotseat widget bounds.
    delegate->SetBoundsFromAnimation(widget_bounds_in_progress,
                                     ui::PropertyChangeReason::FROM_ANIMATION);

    // Do recovering when the animation ends.
    if (current == 1.f) {
      scrollable_shelf_view->set_is_padding_configured_externally(
          /*is_padding_configured_externally=*/false);
    }

    return true;
  }

  // HotseatStateTransitionAnimation:
  void OnAbort(ui::LayerAnimationDelegate* delegate) override {
    GetScrollableShelfView()->set_is_padding_configured_externally(
        /*is_padding_configured_externally=*/false);
  }

  // Scrollable shelf's initial padding insets.
  gfx::Insets initial_padding_insets_;

  // Scrollable shelf's target padding insets.
  gfx::Insets target_padding_insets_;

  // Hotseat background's initial bounds in screen.
  gfx::Rect initial_hotseat_background_in_screen_;

  // Hotseat background's target bounds in screen.
  gfx::Rect target_hotseat_background_in_screen_;
};

// Animation implemented specifically for the transition between the home
// launcher state and the hidden state.
class HomeAndHiddenTransitionAnimation
    : public HotseatStateTransitionAnimation {
 public:
  HomeAndHiddenTransitionAnimation(const gfx::Rect& target_bounds_in_screen,
                                   double target_opacity,
                                   ui::Layer* hotseat_layer,
                                   HotseatWidget* hotseat_widget)
      : HotseatStateTransitionAnimation(target_bounds_in_screen,
                                        target_opacity,
                                        hotseat_layer,
                                        hotseat_widget) {}
  ~HomeAndHiddenTransitionAnimation() override = default;

 protected:
  // HotseatStateTransitionAnimation:
  void OnStart(ui::LayerAnimationDelegate* delegate) override {
    DCHECK(hotseat_widget_->GetShelfView()->shelf()->IsHorizontalAlignment());

    start_opacity_ = hotseat_widget_->GetNativeView()->layer()->opacity();

    if (hotseat_widget_->state() == HotseatState::kHidden)
      will_be_hidden_ = true;

    ScrollableShelfView* scrollable_shelf_view = GetScrollableShelfView();
    const gfx::Rect current_widget_bounds(
        hotseat_widget_->GetWindowBoundsInScreen());

    // Ensure that hotseat only has vertical movement during animation.
    if (will_be_hidden_) {
      animation_initial_bounds_ = current_widget_bounds;

      animation_target_bounds_ = current_widget_bounds;
      animation_target_bounds_.set_y(target_widget_bounds_.y());
    } else {
      animation_initial_bounds_ = target_widget_bounds_;
      animation_initial_bounds_.set_y(current_widget_bounds.y());

      // Ensure that hotseat is set with the target bounds at the end of
      // animation when hotseat is going to show in home launcher.
      animation_target_bounds_ = target_widget_bounds_;
      const gfx::Insets target_padding_insets =
          scrollable_shelf_view->CalculateMirroredEdgePadding(
              /*use_target_bounds=*/true);
      scrollable_shelf_view->SetEdgePaddingInsets(target_padding_insets);
      delegate->SetBoundsFromAnimation(
          animation_initial_bounds_, ui::PropertyChangeReason::FROM_ANIMATION);
    }
  }

  // HotseatStateTransitionAnimation:
  bool OnProgress(double current,
                  ui::LayerAnimationDelegate* delegate) override {
    const double tweened = gfx::Tween::CalculateValue(tween_type_, current);
    delegate->SetOpacityFromAnimation(
        gfx::Tween::DoubleValueBetween(tweened, start_opacity_,
                                       target_opacity_),
        ui::PropertyChangeReason::FROM_ANIMATION);

    const gfx::Rect widget_bounds_in_progress = gfx::Tween::RectValueBetween(
        tweened, animation_initial_bounds_, animation_target_bounds_);

    const bool reach_end = current == 1.f;

    // When hotseat is going to be hidden, |animation_target_bounds_| is not
    // equal to |target_widget_bounds_|. So hotseat is set with the target
    // bounds at the end of animation. It does not bring animation regression
    // since hotseat is invisible to the user when setting bounds.
    delegate->SetBoundsFromAnimation(will_be_hidden_ && reach_end
                                         ? target_widget_bounds_
                                         : widget_bounds_in_progress,
                                     ui::PropertyChangeReason::FROM_ANIMATION);

    return true;
  }

  // HotseatStateTransitionAnimation:
  void OnAbort(ui::LayerAnimationDelegate* delegate) override {}

 private:
  // Whether hotseat widget is hidden after state transition animation.
  bool will_be_hidden_ = false;

  // Note that |animation_initial_bounds_| and |animation_target_bounds_| may
  // not be the hotseat's current bounds and |target_widget_bounds_|
  // respectively.
  gfx::Rect animation_initial_bounds_;
  gfx::Rect animation_target_bounds_;
};

// Custom window targeter for the hotseat. Used so the hotseat only processes
// events that land on the visible portion of the hotseat, and only while the
// hotseat is not animating.
class HotseatWindowTargeter : public aura::WindowTargeter {
 public:
  explicit HotseatWindowTargeter(HotseatWidget* hotseat_widget)
      : hotseat_widget_(hotseat_widget) {}
  ~HotseatWindowTargeter() override = default;

  HotseatWindowTargeter(const HotseatWindowTargeter& other) = delete;
  HotseatWindowTargeter& operator=(const HotseatWindowTargeter& rhs) = delete;

  // aura::WindowTargeter:
  bool SubtreeShouldBeExploredForEvent(aura::Window* window,
                                       const ui::LocatedEvent& event) override {
    // Do not handle events if the hotseat window is animating as it may animate
    // over other items which want to process events.
    if (hotseat_widget_->GetLayer()->GetAnimator()->is_animating())
      return false;
    return aura::WindowTargeter::SubtreeShouldBeExploredForEvent(window, event);
  }

  bool GetHitTestRects(aura::Window* target,
                       gfx::Rect* hit_test_rect_mouse,
                       gfx::Rect* hit_test_rect_touch) const override {
    if (target == hotseat_widget_->GetNativeWindow()) {
      // Shrink the hit bounds from the size of the hotseat to the size of the
      // scrollable shelf view.
      auto* const scrollable_shelf_view =
          hotseat_widget_->scrollable_shelf_view();

      gfx::Rect hit_bounds = scrollable_shelf_view->GetLocalBounds();
      hit_bounds.Inset(
          scrollable_shelf_view->CalculateMirroredEdgePadding(true));
      hit_bounds = scrollable_shelf_view->ConvertRectToWidget(hit_bounds);
      hit_bounds.Offset(target->bounds().origin().OffsetFromOrigin());

      *hit_test_rect_mouse = *hit_test_rect_touch = hit_bounds;
      return true;
    }
    return aura::WindowTargeter::GetHitTestRects(target, hit_test_rect_mouse,
                                                 hit_test_rect_touch);
  }

 private:
  // Unowned and guaranteed to be not null for the duration of |this|.
  const raw_ptr<HotseatWidget> hotseat_widget_;
};

}  // namespace

class HotseatWidget::DelegateView : public HotseatTransitionAnimator::Observer,
                                    public views::WidgetDelegateView,
                                    public views::ViewTargeterDelegate,
                                    public OverviewObserver {
 public:
  DelegateView() {
    SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
    SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  }

  DelegateView(const DelegateView&) = delete;
  DelegateView& operator=(const DelegateView&) = delete;

  ~DelegateView() override;

  // views::ViewTargetDelegate:
  View* TargetForRect(View* root, const gfx::Rect& rect) override {
    // If a context menu for a shelf app button is shown, redirect all events to
    // the shelf app button. Context menus generally capture all events, but
    // shelf app buttons' context menu redirect gesture events to the hotseat
    // widget so shelf app button can continue handling drag events.
    // See also HotseatWidget::OnGestureEvent().
    views::View* item_with_context_menu =
        scrollable_shelf_view_->shelf_view()->GetShelfItemViewWithContextMenu();
    if (item_with_context_menu)
      return item_with_context_menu;
    return views::ViewTargeterDelegate::TargetForRect(root, rect);
  }

  // Initializes the view.
  void Init(ScrollableShelfView* scrollable_shelf_view,
            HotseatWidget* hotseat_widget);

  // Updates the hotseat background.
  void UpdateTranslucentBackground();

  // Updates the highlight border rounded corner radius or the type according to
  // the visibility of shadow.
  void UpdateHighlightBorder(bool update_corner_radius);

  // Returns the target background color for the hotseat.
  SkColor GetBackgroundColor();

  void SetTranslucentBackground(const gfx::Rect& translucent_background_bounds);

  // Sets whether the background should be blurred as requested by the argument,
  // unless the feature flag is disabled or |disable_blur_for_animations_| is
  // true, in which case this disables background blur.
  void SetBackgroundBlur(bool enable_blur);

  // HotseatTransitionAnimator::Observer:
  void OnHotseatTransitionAnimationWillStart(HotseatState from_state,
                                             HotseatState to_state) override;
  void OnHotseatTransitionAnimationEnded(HotseatState from_state,
                                         HotseatState to_state) override;
  void OnHotseatTransitionAnimationAborted() override;

  // views::View:
  void OnThemeChanged() override;

  // views::WidgetDelegateView:
  bool CanActivate() const override;

  // OverviewObserver:
  void OnOverviewModeWillStart() override;
  void OnOverviewModeEndingAnimationComplete(bool canceled) override;

  void set_focus_cycler(FocusCycler* focus_cycler) {
    focus_cycler_ = focus_cycler;
  }

  int background_blur() const {
    return translucent_background_->layer()->background_blur();
  }

  bool is_translucent_background_visible_for_test() {
    return translucent_background_->layer()->GetTargetVisibility();
  }

 private:
  raw_ptr<FocusCycler, DanglingUntriaged> focus_cycler_ = nullptr;
  // A background layer that may be visible depending on HotseatState.
  raw_ptr<views::View> translucent_background_ = nullptr;
  raw_ptr<ScrollableShelfView, DanglingUntriaged> scrollable_shelf_view_ =
      nullptr;                                       // unowned.
  raw_ptr<HotseatWidget> hotseat_widget_ = nullptr;  // unowned.
  // Blur is disabled during animations to improve performance.
  int blur_lock_ = 0;

  // The most recent color that the |translucent_background_| has been animated
  // to.
  SkColor target_color_ = SK_ColorTRANSPARENT;

  std::unique_ptr<SystemShadow> shadow_;

  // The type of highlight border.
  views::HighlightBorder::Type border_type_;

  // Tracks whether the forest flag was enabled when entering overview.
  // TODO(sammiequon): This is temporary while the secret key exists. After the
  // secret key is removed, entering/exiting overview should never need to
  // remove/readd blur.
  std::optional<bool> was_forest_on_overview_enter_;
};

HotseatWidget::DelegateView::~DelegateView() {
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (overview_controller)
    overview_controller->RemoveObserver(this);
}

void HotseatWidget::DelegateView::Init(
    ScrollableShelfView* scrollable_shelf_view,
    HotseatWidget* hotseat_widget) {
  hotseat_widget_ = hotseat_widget;
  SetLayoutManager(std::make_unique<views::FillLayout>());

  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (overview_controller) {
    overview_controller->AddObserver(this);
    if (overview_controller->InOverviewSession() &&
        !features::IsForestFeatureEnabled()) {
      ++blur_lock_;
    }
  }
  DCHECK(scrollable_shelf_view);
  scrollable_shelf_view_ = scrollable_shelf_view;

  // A container view added here is to prevent the `translucent_background_`
  // being stretched by the fill layout.
  auto* background_container_view =
      AddChildViewAt(std::make_unique<views::View>(), 0);
  background_container_view->SetEnabled(false);
  translucent_background_ =
      background_container_view->AddChildView(std::make_unique<views::View>());
  translucent_background_->SetPaintToLayer();
  translucent_background_->layer()->SetFillsBoundsOpaquely(false);
  translucent_background_->layer()->SetName("hotseat/Background");

  // Create a shadow and stack at the bottom.
  shadow_ = SystemShadow::CreateShadowOnTextureLayer(
      SystemShadow::Type::kElevation12);
  auto* parent_layer = translucent_background_->layer()->parent();
  auto* shadow_layer = shadow_->GetLayer();
  parent_layer->Add(shadow_layer);
  parent_layer->StackAtBottom(shadow_layer);

  // Make shadow observe the widget theme change.
  shadow_->ObserveColorProviderSource(hotseat_widget);
}

void HotseatWidget::DelegateView::UpdateTranslucentBackground() {
  // Update highlight border after updating the visibility of shadow.
  absl::Cleanup update_highlight_border = [this] {
    UpdateHighlightBorder(
        /*update_corner_radius=*/false);
  };

  if (!HotseatWidget::ShouldShowHotseatBackground()) {
    translucent_background_->SetVisible(false);
    SetBackgroundBlur(false);
    shadow_->GetLayer()->SetVisible(false);
    return;
  }

  DCHECK(scrollable_shelf_view_);
  SetTranslucentBackground(
      scrollable_shelf_view_->GetHotseatBackgroundBounds());

  // Hide the shadow when home launcher is showing in tablet mode.
  if (hotseat_widget_->state() == HotseatState::kShownHomeLauncher) {
    shadow_->GetLayer()->SetVisible(false);
    return;
  }

  // Update the shadow content bounds and corner radius.
  shadow_->GetLayer()->SetVisible(true);
  gfx::Rect background_bounds = translucent_background_->bounds();
  shadow_->SetRoundedCornerRadius(background_bounds.height() / 2);
  shadow_->SetContentBounds(background_bounds);
}

void HotseatWidget::DelegateView::UpdateHighlightBorder(
    bool update_corner_radius) {
  const bool is_jelly_enabled = chromeos::features::IsJellyrollEnabled();
  views::HighlightBorder::Type border_type;
  if (!is_jelly_enabled) {
    border_type = views::HighlightBorder::Type::kHighlightBorder1;
  } else {
    border_type = shadow_->GetLayer()->visible()
                      ? views::HighlightBorder::Type::kHighlightBorderOnShadow
                      : views::HighlightBorder::Type::kHighlightBorderNoShadow;
  }

  if (GetBorder() && !update_corner_radius && border_type_ == border_type) {
    return;
  }

  const float radius = hotseat_widget_->GetHotseatSize() / 2.0f;
  border_type_ = border_type;
  auto border = std::make_unique<views::HighlightBorder>(radius, border_type_);
  translucent_background_->SetBorder(std::move(border));
}

SkColor HotseatWidget::DelegateView::GetBackgroundColor() {
  auto* widget = GetWidget();
  CHECK(widget);
  aura::Window* window = widget->GetNativeWindow();
  // A forest session uses system-on-base.
  if (features::IsForestFeatureEnabled() &&
      OverviewController::Get()->InOverviewSession() &&
      !SplitViewController::Get(window)->InSplitViewMode()) {
    return widget->GetColorProvider()->GetColor(
        cros_tokens::kCrosSysSystemOnBase);
  }
  return ShelfConfig::Get()->GetDefaultShelfColor(widget);
}

void HotseatWidget::DelegateView::SetTranslucentBackground(
    const gfx::Rect& background_bounds) {
  DCHECK(HotseatWidget::ShouldShowHotseatBackground());

  translucent_background_->SetVisible(true);
  SetBackgroundBlur(/*enable_blur=*/true);

  auto* animator = translucent_background_->layer()->GetAnimator();

  std::optional<ui::AnimationThroughputReporter> reporter;
  if (hotseat_widget_ && hotseat_widget_->state() != HotseatState::kNone) {
    reporter.emplace(animator,
                     hotseat_widget_->GetTranslucentBackgroundReportCallback());
  }

  SkColor background_color = GetBackgroundColor();
  if (background_color != target_color_) {
    ui::ScopedLayerAnimationSettings color_animation_setter(animator);
    DoScopedAnimationSetting(&color_animation_setter);
    target_color_ = background_color;
    translucent_background_->SetBackground(
        views::CreateSolidBackground(target_color_));
  }

  // Animate the bounds change if there's a change of width (for instance when
  // dragging an app into, or out of, the shelf) and meanwhile scrollable
  // shelf's bounds does not update at the same time.
  const bool animate_bounds =
      background_bounds.width() != translucent_background_->bounds().width() &&
      (scrollable_shelf_view_ &&
       !scrollable_shelf_view_->NeedUpdateToTargetBounds());
  std::optional<ui::ScopedLayerAnimationSettings> bounds_animation_setter;
  if (animate_bounds) {
    bounds_animation_setter.emplace(animator);
    DoScopedAnimationSetting(&bounds_animation_setter.value());
  }

  const float radius = hotseat_widget_->GetHotseatSize() / 2.0f;
  gfx::RoundedCornersF rounded_corners = {radius, radius, radius, radius};
  if (translucent_background_->layer()->rounded_corner_radii() !=
      rounded_corners) {
    translucent_background_->layer()->SetRoundedCornerRadius(rounded_corners);
    UpdateHighlightBorder(/*update_corner_radius=*/true);
  }

  const gfx::Rect mirrored_bounds = GetMirroredRect(background_bounds);
  if (translucent_background_->layer()->GetTargetBounds() != mirrored_bounds)
    translucent_background_->SetBoundsRect(mirrored_bounds);
}

void HotseatWidget::DelegateView::SetBackgroundBlur(bool enable_blur) {
  if (!features::IsBackgroundBlurEnabled() || blur_lock_ > 0)
    return;

  const int blur_radius =
      enable_blur ? ShelfConfig::Get()->shelf_blur_radius() : 0;
  if (translucent_background_->layer()->background_blur() != blur_radius) {
    translucent_background_->layer()->SetBackgroundBlur(blur_radius);
    translucent_background_->layer()->SetBackdropFilterQuality(
        ColorProvider::kBackgroundBlurQuality);
  }
}

void HotseatWidget::DelegateView::OnHotseatTransitionAnimationWillStart(
    HotseatState from_state,
    HotseatState to_state) {
  DCHECK_LE(blur_lock_, 2);

  SetBackgroundBlur(false);
  ++blur_lock_;
}

void HotseatWidget::DelegateView::OnHotseatTransitionAnimationEnded(
    HotseatState from_state,
    HotseatState to_state) {
  DCHECK_GT(blur_lock_, 0);

  --blur_lock_;
  SetBackgroundBlur(true);
}

void HotseatWidget::DelegateView::OnHotseatTransitionAnimationAborted() {
  DCHECK_GT(blur_lock_, 0);

  --blur_lock_;
}

void HotseatWidget::DelegateView::OnThemeChanged() {
  views::WidgetDelegateView::OnThemeChanged();

  // Only update the background when the `scrollable_shelf_view_` is
  // initialized.
  if (scrollable_shelf_view_)
    UpdateTranslucentBackground();
}

bool HotseatWidget::DelegateView::CanActivate() const {
  // We don't want mouse clicks to activate us, but we need to allow
  // activation when the user is using the keyboard (FocusCycler).
  return focus_cycler_ && focus_cycler_->widget_activating() == GetWidget();
}

void HotseatWidget::DelegateView::OnOverviewModeWillStart() {
  // Forest uses background blur in overview.
  was_forest_on_overview_enter_ = features::IsForestFeatureEnabled();
  if (*was_forest_on_overview_enter_) {
    return;
  }
  DCHECK_LE(blur_lock_, 2);

  SetBackgroundBlur(false);
  ++blur_lock_;
}

void HotseatWidget::DelegateView::OnOverviewModeEndingAnimationComplete(
    bool canceled) {
  // Forest uses background blur in overview.
  if (was_forest_on_overview_enter_.value_or(true)) {
    was_forest_on_overview_enter_.reset();
    return;
  }
  DCHECK_GT(blur_lock_, 0);

  --blur_lock_;
  SetBackgroundBlur(true);
}

////////////////////////////////////////////////////////////////////////////////
// ScopedInStateTransition

HotseatWidget::ScopedInStateTransition::ScopedInStateTransition(
    HotseatWidget* hotseat_widget,
    HotseatState old_state,
    HotseatState target_state)
    : hotseat_widget_(hotseat_widget) {
  hotseat_widget_->state_transition_in_progress_ =
      CalculateHotseatStateTransition(old_state, target_state);
}

HotseatWidget::ScopedInStateTransition::~ScopedInStateTransition() {
  hotseat_widget_->state_transition_in_progress_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// HotseatWidget

HotseatWidget::HotseatWidget() : delegate_view_(new DelegateView()) {
  ShelfConfig::Get()->AddObserver(this);
}

HotseatWidget::~HotseatWidget() {
  ui::LayerAnimator* hotseat_layer_animator =
      GetNativeView()->layer()->GetAnimator();
  if (hotseat_layer_animator->is_animating())
    hotseat_layer_animator->AbortAllAnimations();

  ShelfConfig::Get()->RemoveObserver(this);
  shelf_->shelf_widget()->hotseat_transition_animator()->RemoveObserver(
      delegate_view_);
  // Remove ScrollableShelfView to avoid any children accessing NativeWidget
  // after its destruction in ~Widget() before RootView clears.
  // TODO(pbos): This is defensive, consider having children observe
  // destruction and/or check the result of GetNativeWidget() and others.
  GetContentsView()->RemoveChildViewT(scrollable_shelf_view_.get());
}

bool HotseatWidget::ShouldShowHotseatBackground() {
  return display::Screen::GetScreen()->InTabletMode();
}

void HotseatWidget::Initialize(aura::Window* container, Shelf* shelf) {
  DCHECK(container);
  DCHECK(shelf);
  shelf_ = shelf;
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "HotseatWidget";
  params.delegate = delegate_view_.get();
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent = container;
  params.layer_type = ui::LAYER_NOT_DRAWN;
  Init(std::move(params));
  set_focus_on_creation(false);

  scrollable_shelf_view_ = GetContentsView()->AddChildView(
      std::make_unique<ScrollableShelfView>(ShelfModel::Get(), shelf));
  delegate_view_->Init(scrollable_shelf_view(), this);
  delegate_view_->SetEnableArrowKeyTraversal(true);
  hotseat_window_targeter_ = std::make_unique<aura::ScopedWindowTargeter>(
      GetNativeWindow(), std::make_unique<HotseatWindowTargeter>(this));

  // The initialization of scrollable shelf should update the translucent
  // background which is stored in |delegate_view_|. So initializes
  // |scrollable_shelf_view_| after |delegate_view_|.
  scrollable_shelf_view_->Init();
}

void HotseatWidget::OnHotseatTransitionAnimatorCreated(
    HotseatTransitionAnimator* animator) {
  shelf_->shelf_widget()->hotseat_transition_animator()->AddObserver(
      delegate_view_);
}

void HotseatWidget::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::EventType::kMousePressed) {
    keyboard::KeyboardUIController::Get()->HideKeyboardImplicitlyByUser();
  }
  views::Widget::OnMouseEvent(event);
}

void HotseatWidget::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::EventType::kGestureTapDown) {
    keyboard::KeyboardUIController::Get()->HideKeyboardImplicitlyByUser();
  }

  // Context menus for shelf app button forward gesture events to hotseat
  // widget, so the shelf app button can continue handling drag even after the
  // context menu starts capturing events. Ignore events not interesting to the
  // shelf app button in this state.
  ShelfAppButton* item_with_context_menu =
      GetShelfView()->GetShelfItemViewWithContextMenu();
  if (item_with_context_menu &&
      !ShelfAppButton::ShouldHandleEventFromContextMenu(event)) {
    event->SetHandled();
    return;
  }

  if (!event->handled())
    views::Widget::OnGestureEvent(event);

  // Ensure that the app button's drag state gets cleared on gesture end even if
  // the event doesn't get delivered to the app button.
  if (item_with_context_menu && event->type() == ui::EventType::kGestureEnd) {
    item_with_context_menu->ClearDragStateOnGestureEnd();
  }
}

bool HotseatWidget::OnNativeWidgetActivationChanged(bool active) {
  if (!Widget::OnNativeWidgetActivationChanged(active))
    return false;

  scrollable_shelf_view_->OnFocusRingActivationChanged(active);
  return true;
}

void HotseatWidget::OnShelfConfigUpdated() {
  set_manually_extended(false);
}

bool HotseatWidget::IsExtended() const {
  DCHECK(GetShelfView()->shelf()->IsHorizontalAlignment());
  const int extended_bottom =
      display::Screen::GetScreen()
          ->GetDisplayNearestView(GetShelfView()->GetWidget()->GetNativeView())
          .bounds()
          .bottom() -
      (ShelfConfig::Get()->shelf_size() +
       ShelfConfig::Get()->hotseat_bottom_padding());
  return GetWindowBoundsInScreen().bottom() == extended_bottom;
}

void HotseatWidget::FocusFirstOrLastFocusableChild(bool last) {
  GetShelfView()->FindFirstOrLastFocusableChild(last)->RequestFocus();
}

void HotseatWidget::OnTabletModeChanged() {
  GetShelfView()->OnTabletModeChanged();
}

float HotseatWidget::CalculateShelfViewOpacity() const {
  const float target_opacity =
      GetShelfView()->shelf()->shelf_layout_manager()->GetOpacity();
  // Hotseat's shelf view should not be dimmed if hotseat is kExtended.
  return (state() == HotseatState::kExtended) ? 1.0f : target_opacity;
}

void HotseatWidget::UpdateTranslucentBackground() {
  delegate_view_->UpdateTranslucentBackground();
}

int HotseatWidget::CalculateHotseatYInScreen(
    HotseatState hotseat_target_state) const {
  DCHECK(shelf_->IsHorizontalAlignment());
  int hotseat_distance_from_bottom_of_display = 0;
  const int hotseat_size = GetHotseatSize();
  switch (hotseat_target_state) {
    case HotseatState::kShownClamshell:
      hotseat_distance_from_bottom_of_display = hotseat_size;
      break;
    case HotseatState::kShownHomeLauncher:
      // When the hotseat state is HotseatState::kShownHomeLauncher, the home
      // launcher is showing in tablet mode. Elevate the hotseat
      // to be hotseat_bottom_padding() above the bottom of the screen to be
      // in line with the navigation and status area.
      hotseat_distance_from_bottom_of_display =
          hotseat_size + ShelfConfig::Get()->hotseat_bottom_padding();
      // Elevate the hotseat app bar to a second row above the navigation and
      // status area if needed.
      if (ShelfConfig::Get()->elevate_tablet_mode_app_bar()) {
        hotseat_distance_from_bottom_of_display +=
            hotseat_size +
            ShelfConfig::Get()->GetHomecherElevatedAppBarOffset();
      }
      break;
    case HotseatState::kHidden:
      // Show the hotseat offscreen.
      hotseat_distance_from_bottom_of_display = 0;
      break;
    case HotseatState::kExtended:
      // Show the hotseat at its extended position.
      hotseat_distance_from_bottom_of_display =
          ShelfConfig::Get()->in_app_shelf_size() +
          ShelfConfig::Get()->hotseat_bottom_padding() + hotseat_size;
      break;
    case HotseatState::kNone:
      NOTREACHED();
  }
  const int target_shelf_size =
      shelf_->shelf_widget()->GetTargetBounds().size().height();
  const int hotseat_y_in_shelf =
      -(hotseat_distance_from_bottom_of_display - target_shelf_size);
  const int shelf_y = shelf_->shelf_widget()->GetTargetBounds().y();
  return hotseat_y_in_shelf + shelf_y;
}

gfx::Size HotseatWidget::CalculateTargetBoundsSize(
    HotseatState hotseat_target_state) const {
  const gfx::Rect shelf_bounds = shelf_->shelf_widget()->GetTargetBounds();

  // |hotseat_size| is the height in horizontal alignment or the width in
  // vertical alignment.
  const int hotseat_size = GetHotseatSize();

  if (hotseat_target_state != HotseatState::kShownHomeLauncher &&
      hotseat_target_state != HotseatState::kShownClamshell) {
    DCHECK(shelf_->IsHorizontalAlignment());
    // Give the hotseat more space if it is shown outside of the shelf.
    return gfx::Size(shelf_bounds.width(), hotseat_size);
  }

  gfx::Size app_bar_size = CalculateInlineAppBarSize();

  if (hotseat_target_state == HotseatState::kShownHomeLauncher) {
    if (ShelfConfig::Get()->elevate_tablet_mode_app_bar()) {
      return gfx::Size(
          shelf_bounds.width() - ShelfConfig::Get()->button_spacing() * 2,
          hotseat_size);
    }
  }
  return app_bar_size;
}

gfx::Size HotseatWidget::CalculateInlineAppBarSize() const {
  const gfx::Rect shelf_bounds = shelf_->shelf_widget()->GetTargetBounds();
  const gfx::Size status_size =
      shelf_->status_area_widget()->GetTargetBounds().size();
  const gfx::Rect nav_bounds = shelf_->navigation_widget()->GetVisibleBounds();

  // |hotseat_size| is the height in horizontal alignment or the width in
  // vertical alignment.
  const int hotseat_size = GetHotseatSize();

  // The navigation widget has extra padding on the hotseat side, to center the
  // buttons inside of it. Make sure to get the extra nav widget padding and
  // take it into account when calculating the hotseat size.
  const int nav_widget_padding =
      nav_bounds.size().IsEmpty()
          ? 0
          : ShelfConfig::Get()->control_button_edge_spacing(
                true /* is_primary_axis_edge */);

  // The minimum gap between hotseat widget and other shelf components including
  // the status area widget and shelf navigation widget (or the edge of display,
  // if the shelf navigation widget does not show).
  const int group_margin = ShelfConfig::Get()->GetAppIconGroupMargin();

  if (shelf_->IsHorizontalAlignment()) {
    const int width = shelf_bounds.width() - nav_bounds.size().width() +
                      nav_widget_padding - 2 * group_margin -
                      status_size.width();
    return gfx::Size(width, hotseat_size);
  }

  const int height = shelf_bounds.height() - nav_bounds.size().height() +
                     nav_widget_padding - 2 * group_margin -
                     status_size.height();
  return gfx::Size(hotseat_size, height);
}

void HotseatWidget::ReserveSpaceForAdjacentWidgets(const gfx::Insets& space) {
  reserved_space_ = space;
}

void HotseatWidget::CalculateTargetBounds() {
  ShelfLayoutManager* layout_manager = shelf_->shelf_layout_manager();
  const HotseatState hotseat_target_state =
      layout_manager->CalculateHotseatState(layout_manager->visibility_state(),
                                            layout_manager->auto_hide_state());

  gfx::Size app_bar_size = CalculateInlineAppBarSize();
  if (hotseat_target_state == HotseatState::kShownHomeLauncher) {
    ShelfConfig::Get()->UpdateShowElevatedAppBar(app_bar_size);
  }

  const gfx::Size hotseat_target_size =
      CalculateTargetBoundsSize(hotseat_target_state);

  target_size_for_shown_state_ =
      CalculateTargetBoundsSize(HotseatState::kShownHomeLauncher);

  const gfx::Rect shelf_bounds = shelf_->shelf_widget()->GetTargetBounds();
  const gfx::Rect status_area_bounds =
      shelf_->status_area_widget()->GetTargetBounds();

  // The minimum gap between hotseat widget and other shelf components including
  // the status area widget and shelf navigation widget (or the edge of display,
  // if the shelf navigation widget does not show).
  const int group_margin = ShelfConfig::Get()->GetAppIconGroupMargin();

  gfx::Point hotseat_origin;
  if (shelf_->IsHorizontalAlignment()) {
    int hotseat_x;
    if (hotseat_target_state != HotseatState::kShownHomeLauncher &&
        hotseat_target_state != HotseatState::kShownClamshell) {
      hotseat_x = shelf_bounds.x();
    } else {
      if (hotseat_target_state == HotseatState::kShownHomeLauncher &&
          ShelfConfig::Get()->elevate_tablet_mode_app_bar()) {
        hotseat_x = shelf_bounds.x();
      } else {
        hotseat_x = base::i18n::IsRTL()
                        ? status_area_bounds.right() + group_margin
                        : status_area_bounds.x() - group_margin -
                              hotseat_target_size.width();
      }
    }

    hotseat_origin =
        gfx::Point(hotseat_x, CalculateHotseatYInScreen(hotseat_target_state));
  } else {
    hotseat_origin =
        gfx::Point(shelf_bounds.x(), status_area_bounds.y() - group_margin -
                                         hotseat_target_size.height());
  }

  target_bounds_ = gfx::Rect(hotseat_origin, hotseat_target_size);

  // Check whether |target_bounds_| will change the state of app scaling. If
  // so, update |target_bounds_| here to avoid re-layout later.
  MaybeAdjustTargetBoundsForAppScaling(hotseat_target_state);
}

gfx::Rect HotseatWidget::GetTargetBounds() const {
  gfx::Rect inset_bounds = target_bounds_;
  inset_bounds.Inset(reserved_space_);
  return inset_bounds;
}

void HotseatWidget::UpdateLayout(bool animate) {
  const LayoutInputs new_layout_inputs = GetLayoutInputs();
  if (layout_inputs_ == new_layout_inputs)
    return;

  // The cached `layout_inputs_` should always be up-to-date, thus it is updated
  // here before all other potential shelf layout invocations.
  layout_inputs_ = new_layout_inputs;

  // Never show this widget outside of an active session.
  if (!new_layout_inputs.is_active_session_state)
    Hide();

  ui::Layer* shelf_view_layer = GetShelfView()->layer();
  {
    ui::ScopedLayerAnimationSettings animation_setter(
        shelf_view_layer->GetAnimator());
    animation_setter.SetTransitionDuration(
        animate ? ShelfConfig::Get()->shelf_animation_duration()
                : base::Milliseconds(0));
    animation_setter.SetTweenType(gfx::Tween::EASE_OUT);
    animation_setter.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

    std::optional<ui::AnimationThroughputReporter> reporter;
    if (animate && state_ != HotseatState::kNone) {
      reporter.emplace(animation_setter.GetAnimator(),
                       shelf_->GetHotseatTransitionReportCallback(state_));
    }

    shelf_view_layer->SetOpacity(new_layout_inputs.shelf_view_opacity);
  }

  // If shelf view is invisible, the hotseat should be as well. Otherwise the
  // hotseat opacity should be 1.0f to preserve background blur.
  const double target_opacity =
      (new_layout_inputs.shelf_view_opacity == 0.f ? 0.f : 1.f);
  const gfx::Rect& target_bounds = new_layout_inputs.bounds;

  if (animate) {
    LayoutHotseatByAnimation(target_opacity, target_bounds);
  } else {
    ui::Layer* hotseat_layer = GetNativeView()->layer();

    // If the running bounds animation is not aborted, it will be interrupted
    // and set hotseat widget with the old target bounds which may differ from
    // |target_bounds| greatly and bring DCHECK errors. For example,
    // if hotseat animation is interrupted by the bounds setting triggered by
    // shelf alignment update, hotseat will be caught in an intermediate state
    // where the shelf alignment is new and the hotseat bounds are old.
    hotseat_layer->GetAnimator()->AbortAllAnimations();

    hotseat_layer->SetOpacity(target_opacity);
    hotseat_layer->SetTransform(gfx::Transform());
    SetBounds(target_bounds);
  }

  delegate_view_->UpdateTranslucentBackground();

  // Setting visibility during an animation causes the visibility property to
  // animate. Set the visibility property without an animation.
  if (new_layout_inputs.shelf_view_opacity != 0.0f &&
      new_layout_inputs.is_active_session_state) {
    ShowInactive();
  }
}

void HotseatWidget::UpdateTargetBoundsForGesture(int shelf_position) {
  if (shelf_->IsHorizontalAlignment())
    target_bounds_.set_y(shelf_position);
  else
    target_bounds_.set_x(shelf_position);
}

gfx::Size HotseatWidget::GetTranslucentBackgroundSize() const {
  DCHECK(scrollable_shelf_view_);
  return scrollable_shelf_view_->GetHotseatBackgroundBounds().size();
}

void HotseatWidget::SetFocusCycler(FocusCycler* focus_cycler) {
  delegate_view_->set_focus_cycler(focus_cycler);
  if (focus_cycler)
    focus_cycler->AddWidget(this);
}

ShelfView* HotseatWidget::GetShelfView() {
  DCHECK(scrollable_shelf_view_);
  return scrollable_shelf_view_->shelf_view();
}

int HotseatWidget::GetHotseatSize() const {
  return ShelfConfig::Get()->GetShelfButtonSize(target_hotseat_density_);
}

int HotseatWidget::GetHotseatFullDragAmount() const {
  ShelfConfig* shelf_config = ShelfConfig::Get();
  return shelf_config->shelf_size() + shelf_config->hotseat_bottom_padding() +
         GetHotseatSize();
}

bool HotseatWidget::UpdateTargetHotseatDensityIfNeeded() {
  if (CalculateTargetHotseatDensity() == target_hotseat_density_) {
    return false;
  }

  shelf_->shelf_layout_manager()->LayoutShelf(/*animate=*/true);
  return true;
}

int HotseatWidget::GetHotseatBackgroundBlurForTest() const {
  return delegate_view_->background_blur();
}

bool HotseatWidget::GetIsTranslucentBackgroundVisibleForTest() const {
  return delegate_view_->is_translucent_background_visible_for_test();
}

bool HotseatWidget::IsShowingShelfMenu() const {
  return GetShelfView()->IsShowingMenu();
}

bool HotseatWidget::EventTargetsShelfView(const ui::LocatedEvent& event) const {
  DCHECK_EQ(event.target(), GetNativeWindow());
  gfx::Point location_in_shelf_view = event.location();
  views::View::ConvertPointFromWidget(scrollable_shelf_view_,
                                      &location_in_shelf_view);
  return scrollable_shelf_view_->GetHotseatBackgroundBounds().Contains(
      location_in_shelf_view);
}

const ShelfView* HotseatWidget::GetShelfView() const {
  return const_cast<const ShelfView*>(
      const_cast<HotseatWidget*>(this)->GetShelfView());
}

metrics_util::ReportCallback
HotseatWidget::GetTranslucentBackgroundReportCallback() {
  return shelf_->GetTranslucentBackgroundReportCallback(state_);
}

void HotseatWidget::SetState(HotseatState state) {
  if (state_ == state)
    return;

  state_ = state;
}

ui::Layer* HotseatWidget::GetLayerForNudgeAnimation() {
  return delegate_view_->layer();
}

bool HotseatWidget::CalculateShelfOverflow(bool use_target_bounds) const {
  return scrollable_shelf_view_->CalculateMirroredEdgePadding(use_target_bounds)
      .IsEmpty();
}

HotseatWidget::LayoutInputs HotseatWidget::GetLayoutInputs() const {
  const ShelfLayoutManager* layout_manager = shelf_->shelf_layout_manager();
  gfx::Rect inset_bounds = target_bounds_;
  inset_bounds.Inset(reserved_space_);
  return {inset_bounds, CalculateShelfViewOpacity(),
          layout_manager->is_active_session_state()};
}

void HotseatWidget::MaybeAdjustTargetBoundsForAppScaling(
    HotseatState hotseat_target_state) {
  // Return early if app scaling state does not change.
  HotseatDensity new_target_hotseat_density = CalculateTargetHotseatDensity();
  if (new_target_hotseat_density == target_hotseat_density_)
    return;

  target_hotseat_density_ = new_target_hotseat_density;

  // Update app icons of shelf view.
  scrollable_shelf_view_->shelf_view()->OnShelfConfigUpdated();

  const gfx::Point adjusted_hotseat_origin = gfx::Point(
      target_bounds_.x(), CalculateHotseatYInScreen(hotseat_target_state));
  target_bounds_ =
      gfx::Rect(adjusted_hotseat_origin,
                gfx::Size(target_bounds_.width(), GetHotseatSize()));
}

HotseatDensity HotseatWidget::CalculateTargetHotseatDensity() const {
  // App scaling is only applied to the standard shelf. So the hotseat density
  // should not update in dense shelf.
  if (ShelfConfig::Get()->is_dense() || !shelf_->IsHorizontalAlignment()) {
    return HotseatDensity::kNormal;
  }

  // TODO(crbug.com/1081476): Currently the scaling animation of hotseat bounds
  // and that of shelf icons do not synchronize due to performance issue. As a
  // result, shelf scaling is not applied to the hotseat state transition, such
  // as the transition from the home launcher state to the extended state.
  // Hotseat density relies on the hotseat bounds in the home launcher state
  // instead of the current hotseat state.

  // Try candidate button sizes in decreasing order. If shelf buttons in one
  // size can show without scrolling, return the density type corresponding to
  // that particular size; if no candidate size can make it, return
  // HotseatDensity::kDense.
  const std::vector<HotseatDensity> kCandidates = {HotseatDensity::kNormal,
                                                   HotseatDensity::kSemiDense};
  for (const auto& candidate : kCandidates) {
    if (!scrollable_shelf_view_->RequiresScrollingForItemSize(
            target_size_for_shown_state_,
            ShelfConfig::Get()->GetShelfButtonSize(candidate))) {
      return candidate;
    }
  }
  return HotseatDensity::kDense;
}

void HotseatWidget::LayoutHotseatByAnimation(double target_opacity,
                                             const gfx::Rect& target_bounds) {
  ui::Layer* hotseat_layer = GetNativeView()->layer();

  // Bounds animations do not take transforms into account, but animations
  // between hidden and extended state use transform to animate. Clear any
  // transform that may have been set by the previous animation, and update
  // current bounds to match it.
  gfx::Rect current_bounds =
      hotseat_layer->transform().MapRect(GetNativeView()->GetBoundsInScreen());

  // If the bounds size has not changed, set the target bounds immediately, and
  // animate using transform.
  // Avoid transforms if the transition has a horizontal component - logic for
  // setting scrollable shelf padding depends on the horizontal in screen
  // bounds, and setting horizontal translation would interfere with that. Note
  // that generally, if horizontal widget position changes, so will its width.
  const bool animate_transform =
      target_bounds.size() == current_bounds.size() &&
      target_bounds.x() == current_bounds.x();
  if (animate_transform) {
    SetBounds(target_bounds);

    gfx::Transform initial_transform;
    initial_transform.Translate(current_bounds.origin() -
                                target_bounds.origin());
    hotseat_layer->SetTransform(initial_transform);
  } else {
    hotseat_layer->SetTransform(gfx::Transform());
    SetBounds(current_bounds);
  }

  ui::ScopedLayerAnimationSettings animation_setter(
      hotseat_layer->GetAnimator());
  animation_setter.SetTransitionDuration(
      ShelfConfig::Get()->shelf_animation_duration());
  animation_setter.SetTweenType(gfx::Tween::EASE_OUT);
  animation_setter.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  std::optional<ui::AnimationThroughputReporter> reporter;
  if (state_ != HotseatState::kNone) {
    reporter.emplace(animation_setter.GetAnimator(),
                     shelf_->GetHotseatTransitionReportCallback(state_));
  }

  if (animate_transform) {
    hotseat_layer->SetOpacity(target_opacity);
    hotseat_layer->SetTransform(gfx::Transform());
    return;
  }

  if (!state_transition_in_progress_.has_value()) {
    // Hotseat animation is not triggered by the update in |state_|. So apply
    // the normal bounds animation.
    StartNormalBoundsAnimation(target_opacity, target_bounds);
    return;
  }

  if (HasSpecialAnimation(*state_transition_in_progress_)) {
    StartHotseatTransitionAnimation(*state_transition_in_progress_,
                                    target_opacity, target_bounds);
  } else {
    StartNormalBoundsAnimation(target_opacity, target_bounds);
  }
}

void HotseatWidget::StartHotseatTransitionAnimation(
    StateTransition state_transition,
    double target_opacity,
    const gfx::Rect& target_bounds) {
  ui::Layer* hotseat_layer = GetNativeView()->layer();
  std::unique_ptr<ui::LayerAnimationElement> animation_elements;
  switch (state_transition) {
    case StateTransition::kHomeLauncherAndExtended:
      animation_elements = std::make_unique<HomeAndExtendedTransitionAnimation>(
          target_bounds, target_opacity, hotseat_layer,
          /*hotseat_widget=*/this);
      break;
    case StateTransition::kHomeLauncherAndHidden:
      animation_elements = std::make_unique<HomeAndHiddenTransitionAnimation>(
          target_bounds, target_opacity, hotseat_layer,
          /*hotseat_widget=*/this);
      break;
    case StateTransition::kHiddenAndExtended:
    case StateTransition::kOther:
      NOTREACHED();
  }

  auto* sequence =
      new ui::LayerAnimationSequence(std::move(animation_elements));
  hotseat_layer->GetAnimator()->StartAnimation(sequence);
}

void HotseatWidget::StartNormalBoundsAnimation(double target_opacity,
                                               const gfx::Rect& target_bounds) {
  GetNativeView()->layer()->SetOpacity(target_opacity);
  SetBounds(target_bounds);
}

}  // namespace ash
