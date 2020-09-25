// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_widget.h"

#include <utility>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/animation/animation_change_type.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/focus_cycler.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/contextual_tooltip.h"
#include "ash/shelf/drag_handle.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/hotseat_transition_animator.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/login_shelf_gesture_controller.h"
#include "ash/shelf/login_shelf_view.h"
#include "ash/shelf/scrollable_shelf_view.h"
#include "ash/shelf/shelf_background_animator_observer.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/work_area_insets.h"
#include "base/command_line.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_owner.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

constexpr int kShelfBlurRadius = 30;
// The maximum size of the opaque layer during an "overshoot" (drag away from
// the screen edge).
constexpr int kShelfMaxOvershootHeight = 40;
constexpr float kShelfBlurQuality = 0.33f;
constexpr int kDragHandleCornerRadius = 2;

// Return the first or last focusable child of |root|.
views::View* FindFirstOrLastFocusableChild(views::View* root,
                                           bool find_last_child) {
  views::FocusSearch search(root, find_last_child /*cycle*/,
                            false /*accessibility_mode*/);
  views::FocusTraversable* dummy_focus_traversable;
  views::View* dummy_focus_traversable_view;
  return search.FindNextFocusableView(
      root,
      find_last_child ? views::FocusSearch::SearchDirection::kBackwards
                      : views::FocusSearch::SearchDirection::kForwards,
      views::FocusSearch::TraversalDirection::kDown,
      views::FocusSearch::StartingViewPolicy::kSkipStartingView,
      views::FocusSearch::AnchoredDialogPolicy::kCanGoIntoAnchoredDialog,
      &dummy_focus_traversable, &dummy_focus_traversable_view);
}

bool IsHotseatEnabled() {
  return Shell::Get()->IsInTabletMode() &&
         chromeos::switches::ShouldShowShelfHotseat();
}

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
  ui::Layer* layer_;
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
  ~DelegateView() override;

  void set_focus_cycler(FocusCycler* focus_cycler) {
    focus_cycler_ = focus_cycler;
  }

  FocusCycler* focus_cycler() { return focus_cycler_; }

  void SetParentLayer(ui::Layer* layer);

  // Adds the login shelf view as a child of this view.
  // Returns a pointer to the login shelf view passed in as an argument.
  LoginShelfView* AddLoginShelfView(
      std::unique_ptr<LoginShelfView> login_shelf_view) {
    login_shelf_view_ = AddChildView(std::move(login_shelf_view));
    return login_shelf_view_;
  }

  void set_default_last_focusable_child(bool default_last_focusable_child) {
    default_last_focusable_child_ = default_last_focusable_child;
  }

  // Immediately hides the layer used to draw the shelf background.
  void HideOpaqueBackground();

  // Immediately shows the layer used to draw the shelf background.
  void ShowOpaqueBackground();

  // views::WidgetDelegate:
  views::Widget* GetWidget() override { return View::GetWidget(); }
  const views::Widget* GetWidget() const override { return View::GetWidget(); }

  bool CanActivate() const override;
  void ReorderChildLayers(ui::Layer* parent_layer) override;
  void OnWidgetInitialized() override;

  void UpdateBackgroundBlur();
  void UpdateOpaqueBackground();
  void UpdateDragHandle();

  // This will be called when the parent local bounds change.
  void OnBoundsChanged(const gfx::Rect& old_bounds) override;

  // views::AccessiblePaneView:
  views::View* GetDefaultFocusableChild() override;
  void Layout() override;

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

  ui::Layer* opaque_background() { return opaque_background_.layer(); }
  ui::Layer* animating_background() { return &animating_background_; }
  ui::Layer* animating_drag_handle() { return &animating_drag_handle_; }
  DragHandle* drag_handle() { return drag_handle_; }

 private:
  // Whether |opaque_background_| is explicitly hidden during an animation.
  // Prevents calls to UpdateOpaqueBackground from inadvertently showing
  // |opaque_background_| during animations.
  bool hide_background_for_transitions_ = false;
  ShelfWidget* shelf_widget_;
  FocusCycler* focus_cycler_;

  // Pointer to the login shelf view - visible only when the session is
  // inactive. The view is owned by this view's hierarchy.
  LoginShelfView* login_shelf_view_ = nullptr;

  // A background layer that may be visible depending on a
  // ShelfBackgroundAnimator.
  ui::LayerOwner opaque_background_;

  // A background layer used to animate hotseat transitions.
  ui::Layer animating_background_;

  // A layer to animate the drag handle during hotseat transitions.
  ui::Layer animating_drag_handle_;

  // A drag handle shown in tablet mode when we are not on the home screen.
  // Owned by the view hierarchy.
  DragHandle* drag_handle_ = nullptr;

  // When true, the default focus of the shelf is the last focusable child.
  bool default_last_focusable_child_ = false;

  // Cache the state of the background blur so that it can be updated only
  // when necessary.
  bool background_is_currently_blurred_ = false;

  DISALLOW_COPY_AND_ASSIGN(DelegateView);
};

ShelfWidget::DelegateView::DelegateView(ShelfWidget* shelf_widget, Shelf* shelf)
    : shelf_widget_(shelf_widget),
      focus_cycler_(nullptr),
      opaque_background_(std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR)),
      animating_background_(ui::LAYER_SOLID_COLOR),
      animating_drag_handle_(ui::LAYER_SOLID_COLOR) {
  opaque_background_.layer()->SetName("shelf/Background");
  animating_background_.SetName("shelf/Animation");
  animating_background_.Add(&animating_drag_handle_);

  DCHECK(shelf_widget_);
  set_owned_by_client();
  SetOwnedByWidget(true);

  set_allow_deactivate_on_esc(true);

  // |animating_background_| will be made visible during hotseat animations.
  ShowAnimatingBackground(false);
  animating_background_.SetColor(ShelfConfig::Get()->GetMaximizedShelfColor());

  drag_handle_ = AddChildView(
      std::make_unique<DragHandle>(kDragHandleCornerRadius, shelf));

  const AshColorProvider::RippleAttributes ripple_attributes =
      AshColorProvider::Get()->GetRippleAttributes();
  animating_drag_handle_.SetColor(ripple_attributes.base_color);
  animating_drag_handle_.SetOpacity(ripple_attributes.inkdrop_opacity + 0.075);
  animating_drag_handle_.SetRoundedCornerRadius(
      {kDragHandleCornerRadius, kDragHandleCornerRadius,
       kDragHandleCornerRadius, kDragHandleCornerRadius});
}

ShelfWidget::DelegateView::~DelegateView() = default;

void ShelfWidget::DelegateView::SetParentLayer(ui::Layer* layer) {
  layer->Add(opaque_background());
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
  opaque_background()->SetVisible(false);
  drag_handle_->SetVisible(false);
}

void ShelfWidget::DelegateView::ShowOpaqueBackground() {
  hide_background_for_transitions_ = false;
  UpdateOpaqueBackground();
  UpdateDragHandle();
  UpdateBackgroundBlur();
}

bool ShelfWidget::DelegateView::CanActivate() const {
  // This widget only contains anything interesting to activate in login/lock
  // screen mode. Only allow activation from the focus cycler, not from mouse
  // events, etc.
  return login_shelf_view_->GetVisible() && focus_cycler_ &&
         focus_cycler_->widget_activating() == GetWidget();
}

void ShelfWidget::DelegateView::ReorderChildLayers(ui::Layer* parent_layer) {
  views::View::ReorderChildLayers(parent_layer);
  parent_layer->StackAtBottom(opaque_background());
}

void ShelfWidget::DelegateView::OnWidgetInitialized() {
  UpdateOpaqueBackground();
}

void ShelfWidget::DelegateView::UpdateBackgroundBlur() {
  if (hide_background_for_transitions_)
    return;
  // Blur only if the background is visible.
  const bool should_blur_background =
      opaque_background()->visible() &&
      shelf_widget_->shelf_layout_manager()->ShouldBlurShelfBackground();
  if (should_blur_background == background_is_currently_blurred_)
    return;

  opaque_background()->SetBackgroundBlur(
      should_blur_background ? kShelfBlurRadius : 0);
  opaque_background()->SetBackdropFilterQuality(kShelfBlurQuality);

  background_is_currently_blurred_ = should_blur_background;
}

void ShelfWidget::DelegateView::UpdateOpaqueBackground() {
  if (hide_background_for_transitions_)
    return;
  // Shell could be destroying.
  if (!Shell::Get()->tablet_mode_controller())
    return;

  gfx::Rect opaque_background_bounds = GetLocalBounds();

  const Shelf* shelf = shelf_widget_->shelf();
  const ShelfBackgroundType background_type =
      shelf_widget_->GetBackgroundType();
  const bool tablet_mode = Shell::Get()->IsInTabletMode();
  const bool in_app = ShelfConfig::Get()->is_in_app();

  bool show_opaque_background =
      !tablet_mode || in_app || !chromeos::switches::ShouldShowShelfHotseat();
  if (show_opaque_background != opaque_background()->visible())
    opaque_background()->SetVisible(show_opaque_background);

  // Extend the opaque layer a little bit to handle "overshoot" gestures
  // gracefully (the user drags the shelf further than it can actually go).
  // That way:
  // 1) When the shelf has rounded corners, only two of them are visible,
  // 2) Even when the shelf is squared, it doesn't tear off the screen edge
  // when dragged away.
  // To achieve this, we extend the layer in the same direction where the shelf
  // is aligned (downwards for a bottom shelf, etc.).
  const int radius = ShelfConfig::Get()->shelf_size() / 2;
  // We can easily round only 2 corners out of 4 which means we don't need as
  // much extra shelf height.
  const int safety_margin = kShelfMaxOvershootHeight;
  opaque_background_bounds.Inset(
      -shelf->SelectValueForShelfAlignment(0, safety_margin, 0), 0,
      -shelf->SelectValueForShelfAlignment(0, 0, safety_margin),
      -shelf->SelectValueForShelfAlignment(safety_margin, 0, 0));

  // Show rounded corners except in maximized (which includes split view) mode,
  // or whenever we are "in app".
  if (background_type == ShelfBackgroundType::kMaximized ||
      background_type == ShelfBackgroundType::kInApp ||
      (tablet_mode && in_app && chromeos::switches::ShouldShowShelfHotseat())) {
    opaque_background()->SetRoundedCornerRadius({0, 0, 0, 0});
  } else {
    opaque_background()->SetRoundedCornerRadius({
        shelf->SelectValueForShelfAlignment(radius, 0, radius),
        shelf->SelectValueForShelfAlignment(radius, radius, 0),
        shelf->SelectValueForShelfAlignment(0, radius, 0),
        shelf->SelectValueForShelfAlignment(0, 0, radius),
    });
  }
  opaque_background()->SetBounds(opaque_background_bounds);

  UpdateDragHandle();
  UpdateBackgroundBlur();
  SchedulePaint();
}

void ShelfWidget::DelegateView::UpdateDragHandle() {
  if (shelf_widget_->login_shelf_view_->GetVisible()) {
    drag_handle_->SetVisible(
        shelf_widget_->login_shelf_gesture_controller_.get());
    return;
  }

  if (!Shell::Get()->IsInTabletMode() || !ShelfConfig::Get()->is_in_app() ||
      !chromeos::switches::ShouldShowShelfHotseat() ||
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

views::View* ShelfWidget::DelegateView::GetDefaultFocusableChild() {
  if (login_shelf_view_->GetVisible()) {
    return FindFirstOrLastFocusableChild(login_shelf_view_,
                                         default_last_focusable_child_);
  }
  // If the login shelf view is not visible, there is nothing else to focus
  // in this view.
  return nullptr;
}

void ShelfWidget::DelegateView::Layout() {
  login_shelf_view_->SetBoundsRect(GetLocalBounds());

  // Center drag handle within the expected in-app shelf bounds - it's safe to
  // assume bottom shelf, given that the drag handle is only shown within the
  // bottom shelf (either in tablet mode, or on login/lock screen)
  gfx::Rect drag_handle_bounds = GetLocalBounds();
  drag_handle_bounds.Inset(
      0,
      drag_handle_bounds.height() -
          ShelfConfig::Get()->shelf_drag_handle_centering_size(),
      0, 0);
  drag_handle_bounds.ClampToCenteredSize(ShelfConfig::Get()->DragHandleSize());

  drag_handle_->SetBoundsRect(drag_handle_bounds);
}

void ShelfWidget::DelegateView::UpdateShelfBackground(SkColor color) {
  opaque_background()->SetColor(color);
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
  return opaque_background_.layer()->background_color();
}

bool ShelfWidget::GetHitTestRects(aura::Window* target,
                                  gfx::Rect* hit_test_rect_mouse,
                                  gfx::Rect* hit_test_rect_touch) {
  // This should only get called when the login shelf is visible, i.e. not
  // during an active session. In an active session, hit test rects should be
  // calculated higher up in the class hierarchy by |EasyResizeWindowTargeter|.
  // When in OOBE or locked/login screen, let events pass through empty parts of
  // the shelf.
  DCHECK(login_shelf_view_->GetVisible());
  gfx::Rect login_view_button_bounds =
      login_shelf_view_->ConvertRectToWidget(login_shelf_view_->GetMirroredRect(
          login_shelf_view_->get_button_union_bounds()));
  aura::Window* source = login_shelf_view_->GetWidget()->GetNativeWindow();
  aura::Window::ConvertRectToTarget(source, target->parent(),
                                    &login_view_button_bounds);
  *hit_test_rect_mouse = login_view_button_bounds;

  // If login shelf gesture detection is active, consume touch events on the
  // whole shelf, so |login_shelf_gesture_controller_| can receive them.
  if (login_shelf_gesture_controller_) {
    gfx::Rect shelf_view_bounds = login_shelf_view_->GetLocalBounds();
    aura::Window::ConvertRectToTarget(source, target->parent(),
                                      &shelf_view_bounds);
    *hit_test_rect_touch = shelf_view_bounds;
  } else {
    *hit_test_rect_touch = login_view_button_bounds;
  }
  return true;
}

void ShelfWidget::ForceToShowHotseat() {
  if (is_hotseat_forced_to_show_)
    return;

  is_hotseat_forced_to_show_ = true;
  shelf_layout_manager_->UpdateVisibilityState();
}

bool ShelfWidget::SetLoginShelfSwipeHandler(
    const base::string16& nudge_text,
    const base::RepeatingClosure& fling_callback,
    base::OnceClosure exit_callback) {
  if (!login_shelf_view_->GetVisible())
    return false;

  if (!Shell::Get()->IsInTabletMode())
    return false;

  login_shelf_gesture_controller_ =
      std::make_unique<LoginShelfGestureController>(
          shelf_, delegate_view_->drag_handle(), nudge_text, fling_callback,
          std::move(exit_callback));
  delegate_view_->UpdateDragHandle();
  return true;
}

void ShelfWidget::ClearLoginShelfSwipeHandler() {
  login_shelf_gesture_controller_.reset();
  delegate_view_->UpdateDragHandle();
}

ui::Layer* ShelfWidget::GetOpaqueBackground() {
  return delegate_view_->opaque_background();
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
  delegate_view_->drag_handle()->HideDragHandleNudge(context);
}

void ShelfWidget::SetLoginShelfButtonOpacity(float target_opacity) {
  if (login_shelf_view_->GetVisible())
    login_shelf_view_->SetButtonOpacity(target_opacity);
}

void ShelfWidget::ForceToHideHotseat() {
  if (!is_hotseat_forced_to_show_)
    return;

  is_hotseat_forced_to_show_ = false;
  shelf_layout_manager_->UpdateVisibilityState();
}

ShelfWidget::ShelfWidget(Shelf* shelf)
    : shelf_(shelf),
      background_animator_(shelf_, Shell::Get()->wallpaper_controller()),
      shelf_layout_manager_(new ShelfLayoutManager(this, shelf)),
      delegate_view_(new DelegateView(this, shelf_)),
      scoped_session_observer_(this) {
  DCHECK(shelf_);
}

ShelfWidget::~ShelfWidget() {
  Shell::Get()->accessibility_controller()->RemoveObserver(this);

  // Must call Shutdown() before destruction.
  DCHECK(!status_area_widget());
}

void ShelfWidget::Initialize(aura::Window* shelf_container) {
  DCHECK(shelf_container);

  login_shelf_view_ =
      delegate_view_->AddLoginShelfView(std::make_unique<LoginShelfView>(
          RootWindowController::ForWindow(shelf_container)
              ->lock_screen_action_background_controller()));

  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "ShelfWidget";
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.delegate = delegate_view_;
  params.parent = shelf_container;

  Init(std::move(params));

  // The shelf should not take focus when initially shown.
  set_focus_on_creation(false);
  delegate_view_->SetParentLayer(GetLayer());
  SetContentsView(delegate_view_);

  shelf_layout_manager_->AddObserver(this);
  shelf_container->SetLayoutManager(shelf_layout_manager_);
  shelf_layout_manager_->InitObservers();
  background_animator_.Init(ShelfBackgroundType::kDefaultBg);
  background_animator_.PaintBackground(
      shelf_layout_manager_->GetShelfBackgroundType(),
      AnimationChangeType::IMMEDIATE);

  background_animator_.AddObserver(delegate_view_);
  shelf_->AddObserver(this);

  // Sets initial session state to make sure the UI is properly shown.
  OnSessionStateChanged(Shell::Get()->session_controller()->GetSessionState());
  GetFocusManager()->set_arrow_key_traversal_enabled_for_widget(true);

  Shell::Get()->accessibility_controller()->AddObserver(this);
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

  // Don't need to update the shelf background during shutdown.
  background_animator_.RemoveObserver(delegate_view_);
  shelf_->RemoveObserver(this);

  // Don't need to observe focus/activation during shutdown.
  Shell::Get()->focus_cycler()->RemoveWidget(this);
  SetFocusCycler(nullptr);
}

ShelfBackgroundType ShelfWidget::GetBackgroundType() const {
  return background_animator_.target_background_type();
}

int ShelfWidget::GetBackgroundAlphaValue(
    ShelfBackgroundType background_type) const {
  return SkColorGetA(background_animator_.GetBackgroundColor(background_type));
}

void ShelfWidget::RegisterHotseatWidget(HotseatWidget* hotseat_widget) {
  // Show a context menu for right clicks anywhere on the shelf widget.
  delegate_view_->set_context_menu_controller(hotseat_widget->GetShelfView());
  hotseat_transition_animator_.reset(new HotseatTransitionAnimator(this));
  hotseat_transition_animator_->AddObserver(delegate_view_);
  shelf_->hotseat_widget()->OnHotseatTransitionAnimatorCreated(
      hotseat_transition_animator());
}

void ShelfWidget::OnTabletModeChanged() {
  if (!Shell::Get()->IsInTabletMode()) {
    // Resets |is_hotseat_forced_to_show| when leaving the tablet mode.
    is_hotseat_forced_to_show_ = false;

    // Disable login shelf gesture controller, if one is set when leacing tablet
    // mode.
    ClearLoginShelfSwipeHandler();
  }
}

void ShelfWidget::PostCreateShelf() {
  ash::FocusCycler* focus_cycler = Shell::Get()->focus_cycler();
  SetFocusCycler(focus_cycler);

  // Add widgets to |focus_cycler| in the desired focus order in LTR.
  focus_cycler->AddWidget(navigation_widget());
  hotseat_widget()->SetFocusCycler(focus_cycler);
  focus_cycler->AddWidget(status_area_widget());

  shelf_layout_manager_->LayoutShelf();
  shelf_layout_manager_->UpdateAutoHideState();
  ShowIfHidden();
}

bool ShelfWidget::IsShowingAppList() const {
  return navigation_widget()->GetHomeButton() &&
         navigation_widget()->GetHomeButton()->IsShowingAppList();
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

  if (chromeos::switches::ShouldShowShelfHotseat()) {
    return hotseat_widget()
        ->scrollable_shelf_view()
        ->GetTargetScreenBoundsOfItemIcon(id);
  }

  gfx::Rect bounds(
      hotseat_widget()->GetShelfView()->GetIdealBoundsOfItemIcon(id));
  gfx::Point screen_origin;
  views::View::ConvertPointToScreen(hotseat_widget()->GetShelfView(),
                                    &screen_origin);
  return gfx::Rect(screen_origin.x() + bounds.x(),
                   screen_origin.y() + bounds.y(), bounds.width(),
                   bounds.height());
}

gfx::Rect ShelfWidget::GetVisibleShelfBounds() const {
  gfx::Rect shelf_region = GetWindowBoundsInScreen();
  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(GetNativeWindow());
  DCHECK(!display.bounds().IsEmpty());
  shelf_region.Intersect(display.bounds());
  return screen_util::SnapBoundsToDisplayEdge(shelf_region, GetNativeWindow());
}

ApplicationDragAndDropHost* ShelfWidget::GetDragAndDropHostForAppList() {
  return hotseat_widget()->GetShelfView();
}

void ShelfWidget::set_default_last_focusable_child(
    bool default_last_focusable_child) {
  delegate_view_->set_default_last_focusable_child(
      default_last_focusable_child);
}

bool ShelfWidget::OnNativeWidgetActivationChanged(bool active) {
  if (!Widget::OnNativeWidgetActivationChanged(active))
    return false;
  if (active) {
    // This widget should not get activated in an active session.
    DCHECK(login_shelf_view_->GetVisible());
    delegate_view_->SetPaneFocusAndFocusDefault();
  }
  return true;
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
    shelf_in_screen_portion =
        Shell::Get()->app_list_controller()->home_launcher_transition_state() ==
                AppListControllerImpl::HomeLauncherTransitionState::kMostlyShown
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
  const float target_opacity = layout_manager->GetOpacity();
  if (GetLayer()->opacity() != target_opacity) {
    if (target_opacity == 0) {
      // On hide, set the opacity after the animation completes.
      hide_animation_observer_ =
          std::make_unique<HideAnimationObserver>(GetLayer());
    } else {
      // On show, set the opacity before the animation begins to ensure the blur
      // is shown while the shelf moves.
      GetLayer()->SetOpacity(1.0f);
    }
  }

  gfx::Rect current_shelf_bounds = GetWindowBoundsInScreen();

  if (GetNativeView()->layer()->GetAnimator()->is_animating()) {
    // When the |shelf_widget_| needs to reverse the direction of the current
    // animation, we must take into account the transform when calculating the
    // current shelf widget bounds.
    gfx::RectF transformed_bounds(current_shelf_bounds);
    GetLayer()->transform().TransformRect(&transformed_bounds);
    current_shelf_bounds = gfx::ToEnclosedRect(transformed_bounds);
  }

  gfx::Transform shelf_widget_target_transform;
  shelf_widget_target_transform.Translate(current_shelf_bounds.origin() -
                                          GetTargetBounds().origin());
  GetLayer()->SetTransform(shelf_widget_target_transform);
  SetBounds(
      screen_util::SnapBoundsToDisplayEdge(target_bounds_, GetNativeWindow()));

  {
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
    if (!IsHotseatEnabled())
      target_bounds_.set_y(shelf_position);
  } else {
    target_bounds_.set_x(shelf_position);
  }
}

void ShelfWidget::HandleLocaleChange() {
  login_shelf_view_->HandleLocaleChange();
}

gfx::Rect ShelfWidget::GetTargetBounds() const {
  return target_bounds_;
}

void ShelfWidget::OnSessionStateChanged(session_manager::SessionState state) {
  // Do not show shelf widget:
  // * when views based shelf is disabled
  // * in UNKNOWN state - it might be called before shelf was initialized
  // * on secondary screens in states other than ACTIVE
  bool unknown_state = state == session_manager::SessionState::UNKNOWN;
  bool hide_on_secondary_screen = shelf_->ShouldHideOnSecondaryDisplay(state);
  if (unknown_state || hide_on_secondary_screen) {
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
    login_shelf_view()->SetVisible(!show_hotseat);

    if (show_hotseat)
      login_shelf_gesture_controller_.reset();
    ShowIfHidden();

    // The shelf widget can get activated when login shelf view is shown, which
    // would stack it above other widgets in the shelf container, which is an
    // undesirable state for active session shelf (as the shelf background would
    // be painted over the hotseat/navigation buttons/status area). Make sure
    // the shelf widget is restacked at the bottom of the shelf container when
    // the session state changes.
    // TODO(https://crbug.com/1057207): Ideally, the shelf widget position at
    // the bottom of window stack would be maintained using a "stacked at
    // bottom" window property - switch to that approach once it's ready for
    // usage.
    if (show_hotseat)
      shelf_window->parent()->StackChildAtBottom(shelf_window);
  }
  shelf_layout_manager_->SetDimmed(false);
  delegate_view_->UpdateDragHandle();
  // Update drag handle's color on session state changes since the color mode
  // might change on session state changes.
  delegate_view_->drag_handle()->UpdateColor();
  login_shelf_view_->UpdateAfterSessionChange();
}

void ShelfWidget::OnUserSessionAdded(const AccountId& account_id) {
  shelf_layout_manager_->SetDimmed(false);
  login_shelf_view_->UpdateAfterSessionChange();
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

bool ShelfWidget::HandleLoginShelfGestureEvent(
    const ui::GestureEvent& event_in_screen) {
  if (!login_shelf_gesture_controller_)
    return false;

  return login_shelf_gesture_controller_->HandleGestureEvent(event_in_screen);
}

void ShelfWidget::OnMouseEvent(ui::MouseEvent* event) {
  if (event->IsMouseWheelEvent()) {
    ui::MouseWheelEvent* mouse_wheel_event = event->AsMouseWheelEvent();
    shelf_->ProcessMouseWheelEvent(mouse_wheel_event, /*from_touchpad=*/false);
    return;
  }

  if (event->type() == ui::ET_MOUSE_PRESSED) {
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
  if (event->type() == ui::ET_GESTURE_TAP_DOWN)
    keyboard::KeyboardUIController::Get()->HideKeyboardImplicitlyByUser();
  ui::GestureEvent event_in_screen(*event);
  gfx::Point location_in_screen(event->location());
  ::wm::ConvertPointToScreen(GetNativeWindow(), &location_in_screen);
  event_in_screen.set_location(location_in_screen);

  // Tap on in-app shelf should show a contextual nudge for in-app to home
  // gesture.
  if (event->type() == ui::ET_GESTURE_TAP && ShelfConfig::Get()->is_in_app() &&
      features::AreContextualNudgesEnabled()) {
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

void ShelfWidget::OnAccessibilityStatusChanged() {
  is_hotseat_forced_to_show_ =
      Shell::Get()->accessibility_controller()->spoken_feedback().enabled();
  shelf_layout_manager_->UpdateVisibilityState();
}

}  // namespace ash
