// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_widget.h"

#include <utility>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/animation/animation_change_type.h"
#include "ash/focus_cycler.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/hotseat_transition_animator.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/login_shelf_view.h"
#include "ash/shelf/overflow_bubble.h"
#include "ash/shelf/overflow_bubble_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_background_animator_observer.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/status_area_layout_manager.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

constexpr int kShelfBlurRadius = 30;
// The maximum size of the opaque layer during an "overshoot" (drag away from
// the screen edge).
constexpr int kShelfMaxOvershootHeight = 40;
constexpr float kShelfBlurQuality = 0.33f;
constexpr gfx::Size kDragHandleSize(80, 4);
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

bool IsInTabletMode() {
  return Shell::Get()->tablet_mode_controller() &&
         Shell::Get()->tablet_mode_controller()->InTabletMode();
}

}  // namespace

// The contents view of the Shelf. In an active session, this is used to
// display a semi-opaque background behind the shelf. Outside of an active
// session, this also contains the login shelf view.
class ShelfWidget::DelegateView : public views::WidgetDelegate,
                                  public views::AccessiblePaneView,
                                  public ShelfBackgroundAnimatorObserver,
                                  public HotseatTransitionAnimator::Observer {
 public:
  explicit DelegateView(ShelfWidget* shelf);
  ~DelegateView() override;

  void set_focus_cycler(FocusCycler* focus_cycler) {
    focus_cycler_ = focus_cycler;
  }

  FocusCycler* focus_cycler() { return focus_cycler_; }

  void SetParentLayer(ui::Layer* layer);

  void set_default_last_focusable_child(bool default_last_focusable_child) {
    default_last_focusable_child_ = default_last_focusable_child;
  }

  // Immediately hides the layer used to draw the shelf background.
  void HideOpaqueBackground();

  // Immediately shows the layer used to draw the shelf background.
  void ShowOpaqueBackground();

  // views::WidgetDelegate:
  void DeleteDelegate() override { delete this; }
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

  // ShelfBackgroundAnimatorObserver:
  void UpdateShelfBackground(SkColor color) override;

  // HotseatBackgroundAnimator::Observer:
  void OnHotseatTransitionAnimationStarted(HotseatState from_state,
                                           HotseatState to_state) override;
  void OnHotseatTransitionAnimationEnded(HotseatState from_state,
                                         HotseatState to_state) override;

  // Hide or show the the |animating_background_| layer.
  void ShowAnimatingBackground(bool show);

  SkColor GetShelfBackgroundColor() const;

  ui::Layer* opaque_background() { return &opaque_background_; }
  ui::Layer* animating_background() { return &animating_background_; }

 private:
  // Whether |opaque_background_| is explicitly hidden during an animation.
  // Prevents calls to UpdateOpaqueBackground from inadvertently showing
  // |opaque_background_| during animations.
  bool hide_background_for_transitions_ = false;
  ShelfWidget* shelf_widget_;
  FocusCycler* focus_cycler_;
  // A background layer that may be visible depending on a
  // ShelfBackgroundAnimator.
  ui::Layer opaque_background_;

  // A background layer used to animate hotseat transitions.
  ui::Layer animating_background_;

  // A drag handle shown in tablet mode when we are not on the home screen.
  // Owned by the view hierarchy.
  views::View* drag_handle_ = nullptr;

  // When true, the default focus of the shelf is the last focusable child.
  bool default_last_focusable_child_ = false;

  // Cache the state of the background blur so that it can be updated only
  // when necessary.
  bool background_is_currently_blurred_ = false;

  DISALLOW_COPY_AND_ASSIGN(DelegateView);
};

ShelfWidget::DelegateView::DelegateView(ShelfWidget* shelf_widget)
    : shelf_widget_(shelf_widget),
      focus_cycler_(nullptr),
      opaque_background_(ui::LAYER_SOLID_COLOR),
      animating_background_(ui::LAYER_SOLID_COLOR) {
  DCHECK(shelf_widget_);
  set_owned_by_client();  // Deleted by DeleteDelegate().

  SetLayoutManager(std::make_unique<views::FillLayout>());
  set_allow_deactivate_on_esc(true);

  // |animating_background_| will be made visible during hotseat animations.
  ShowAnimatingBackground(false);
  if (features::IsBackgroundBlurEnabled())
    animating_background_.SetBackdropFilterQuality(0.33f);

  std::unique_ptr<views::View> drag_handle_ptr =
      std::make_unique<views::View>();
  const int radius = kDragHandleCornerRadius;
  const AshColorProvider::RippleAttributes ripple_attributes =
      AshColorProvider::Get()->GetRippleAttributes(
          ShelfConfig::Get()->GetDefaultShelfColor());
  drag_handle_ = AddChildView(std::move(drag_handle_ptr));
  drag_handle_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  drag_handle_->layer()->SetColor(ripple_attributes.base_color);
  // TODO(manucornet): Figure out why we need a manual opacity adjustment
  // to make this color look the same as the status area highlight.
  drag_handle_->layer()->SetOpacity(ripple_attributes.inkdrop_opacity + 0.075);
  drag_handle_->layer()->SetRoundedCornerRadius(
      {radius, radius, radius, radius});
  drag_handle_->SetSize(kDragHandleSize);
}

ShelfWidget::DelegateView::~DelegateView() = default;

// static
bool ShelfWidget::IsUsingViewsShelf() {
  switch (Shell::Get()->session_controller()->GetSessionState()) {
    case session_manager::SessionState::ACTIVE:
      return true;
    // See https://crbug.com/798869.
    case session_manager::SessionState::OOBE:
    case session_manager::SessionState::LOGIN_PRIMARY:
      return true;
    case session_manager::SessionState::LOCKED:
    case session_manager::SessionState::LOGIN_SECONDARY:
      return switches::IsUsingViewsLock();
    case session_manager::SessionState::UNKNOWN:
    case session_manager::SessionState::LOGGED_IN_NOT_ACTIVE:
      return features::IsViewsLoginEnabled();
  }
}

void ShelfWidget::DelegateView::SetParentLayer(ui::Layer* layer) {
  layer->Add(&opaque_background_);
  layer->Add(&animating_background_);
  ReorderLayers();
}

void ShelfWidget::DelegateView::HideOpaqueBackground() {
  hide_background_for_transitions_ = true;
  opaque_background_.SetVisible(false);
}

void ShelfWidget::DelegateView::ShowOpaqueBackground() {
  hide_background_for_transitions_ = false;
  UpdateOpaqueBackground();
  UpdateBackgroundBlur();
}

bool ShelfWidget::DelegateView::CanActivate() const {
  // This widget only contains anything interesting to activate in login/lock
  // screen mode. Only allow activation from the focus cycler, not from mouse
  // events, etc.
  return shelf_widget_->login_shelf_view_->GetVisible() && focus_cycler_ &&
         focus_cycler_->widget_activating() == GetWidget();
}

void ShelfWidget::DelegateView::ReorderChildLayers(ui::Layer* parent_layer) {
  views::View::ReorderChildLayers(parent_layer);
  parent_layer->StackAtBottom(&opaque_background_);
  parent_layer->StackAtBottom(&animating_background_);
}

void ShelfWidget::DelegateView::OnWidgetInitialized() {
  UpdateOpaqueBackground();
}

void ShelfWidget::DelegateView::UpdateBackgroundBlur() {
  if (hide_background_for_transitions_)
    return;
  // Blur only if the background is visible.
  const bool should_blur_background =
      opaque_background_.visible() &&
      shelf_widget_->shelf_layout_manager()->ShouldBlurShelfBackground();
  if (should_blur_background == background_is_currently_blurred_)
    return;

  opaque_background_.SetBackgroundBlur(should_blur_background ? kShelfBlurRadius
                                                              : 0);
  opaque_background_.SetBackdropFilterQuality(kShelfBlurQuality);

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
  const bool tablet_mode = IsInTabletMode();
  const bool in_app = ShelfConfig::Get()->is_in_app();

  bool show_opaque_background =
      !tablet_mode || in_app || !chromeos::switches::ShouldShowShelfHotseat();
  if (show_opaque_background != opaque_background_.visible())
    opaque_background_.SetVisible(show_opaque_background);

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
  if (background_type == SHELF_BACKGROUND_MAXIMIZED ||
      (tablet_mode && in_app)) {
    opaque_background_.SetRoundedCornerRadius({0, 0, 0, 0});
  } else {
    opaque_background_.SetRoundedCornerRadius({
        shelf->SelectValueForShelfAlignment(radius, 0, radius),
        shelf->SelectValueForShelfAlignment(radius, radius, 0),
        shelf->SelectValueForShelfAlignment(0, radius, 0),
        shelf->SelectValueForShelfAlignment(0, 0, radius),
    });
  }
  opaque_background_.SetBounds(opaque_background_bounds);
  UpdateDragHandle();
  UpdateBackgroundBlur();
  SchedulePaint();
}

void ShelfWidget::DelegateView::UpdateDragHandle() {
  if (!IsInTabletMode() || !ShelfConfig::Get()->is_in_app() ||
      !chromeos::switches::ShouldShowShelfHotseat()) {
    drag_handle_->SetVisible(false);
    return;
  }
  drag_handle_->SetVisible(true);

  const int x = (shelf_widget_->GetClientAreaBoundsInScreen().width() -
                 kDragHandleSize.width()) /
                2;
  const int y = (shelf_widget_->GetClientAreaBoundsInScreen().height() -
                 kDragHandleSize.height()) /
                2;
  drag_handle_->SetBounds(x, y, kDragHandleSize.width(),
                          kDragHandleSize.height());
}

void ShelfWidget::DelegateView::OnBoundsChanged(const gfx::Rect& old_bounds) {
  UpdateOpaqueBackground();
  shelf_widget_->status_area_widget()->UpdateCollapseState();
}

views::View* ShelfWidget::DelegateView::GetDefaultFocusableChild() {
  if (!IsUsingViewsShelf())
    return GetFirstFocusableChild();

  if (shelf_widget_->login_shelf_view_->GetVisible()) {
    return FindFirstOrLastFocusableChild(shelf_widget_->login_shelf_view_,
                                         default_last_focusable_child_);
  }
  // If the login shelf view is not visible, there is nothing else to focus
  // in this view.
  return nullptr;
}

void ShelfWidget::DelegateView::UpdateShelfBackground(SkColor color) {
  opaque_background_.SetColor(color);
  UpdateOpaqueBackground();
}

void ShelfWidget::DelegateView::OnHotseatTransitionAnimationStarted(
    HotseatState from_state,
    HotseatState to_state) {
  ShowAnimatingBackground(true);
  // If animating from a kShown hotseat, the animating background will
  // animate from the hotseat background into the in-app shelf, so hide the
  // real shelf background until the animation is complete.
  if (from_state == HotseatState::kShown)
    HideOpaqueBackground();
}

void ShelfWidget::DelegateView::OnHotseatTransitionAnimationEnded(
    HotseatState from_state,
    HotseatState to_state) {
  ShowAnimatingBackground(false);
  if (from_state == HotseatState::kShown)
    ShowOpaqueBackground();
}

void ShelfWidget::DelegateView::ShowAnimatingBackground(bool show) {
  animating_background_.SetVisible(show);

  // To ensure smooth scrollable shelf animations, we disable blur when the
  // |animating_background_| is not visible.
  if (features::IsBackgroundBlurEnabled())
    animating_background_.SetBackgroundBlur(show ? 30 : 0);
}

SkColor ShelfWidget::DelegateView::GetShelfBackgroundColor() const {
  return opaque_background_.background_color();
}

bool ShelfWidget::GetHitTestRects(aura::Window* target,
                                  gfx::Rect* hit_test_rect_mouse,
                                  gfx::Rect* hit_test_rect_touch) {
  // This should only get called when the login shelf is visible, i.e. not
  // during an active session. In an active session, hit test rects should be
  // calculated higher up in the class hierarchy by |EasyResizeWindowTargeter|.
  // When in OOBE or locked/login screen, let events pass through empty parts
  // of the shelf.
  DCHECK(login_shelf_view_->GetVisible());
  gfx::Rect login_view_button_bounds =
      login_shelf_view_->ConvertRectToWidget(login_shelf_view_->GetMirroredRect(
          login_shelf_view_->get_button_union_bounds()));
  aura::Window* source = login_shelf_view_->GetWidget()->GetNativeWindow();
  aura::Window::ConvertRectToTarget(source, target->parent(),
                                    &login_view_button_bounds);
  *hit_test_rect_mouse = login_view_button_bounds;
  *hit_test_rect_touch = login_view_button_bounds;
  return true;
}

void ShelfWidget::ForceToShowHotseat() {
  if (is_hotseat_forced_to_show_)
    return;

  is_hotseat_forced_to_show_ = true;
  shelf_layout_manager_->UpdateVisibilityState();
}

ui::Layer* ShelfWidget::GetOpaqueBackground() {
  return delegate_view_->opaque_background();
}

ui::Layer* ShelfWidget::GetAnimatingBackground() {
  return delegate_view_->animating_background();
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
      delegate_view_(new DelegateView(this)),
      scoped_session_observer_(this) {
  DCHECK(shelf_);
}

ShelfWidget::~ShelfWidget() {
  Shell::Get()->accessibility_controller()->RemoveObserver(this);

  // Must call Shutdown() before destruction.
  DCHECK(!status_area_widget_);
}

void ShelfWidget::Initialize(aura::Window* shelf_container) {
  DCHECK(shelf_container);

  login_shelf_view_ =
      new LoginShelfView(RootWindowController::ForWindow(shelf_container)
                             ->lock_screen_action_background_controller());

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
  SetContentsView(delegate_view_);
  delegate_view_->SetParentLayer(GetLayer());

  GetContentsView()->AddChildView(login_shelf_view_);

  shelf_layout_manager_->AddObserver(this);
  shelf_container->SetLayoutManager(shelf_layout_manager_);
  shelf_layout_manager_->InitObservers();
  background_animator_.Init(SHELF_BACKGROUND_DEFAULT);
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
  hotseat_transition_animator_->RemoveObserver(hotseat_widget_.get());
  hotseat_transition_animator_.reset();
  // Shutting down the status area widget may cause some widgets (e.g. bubbles)
  // to close, so uninstall the ShelfLayoutManager event filters first. Don't
  // reset the pointer until later because other widgets (e.g. app list) may
  // access it later in shutdown.
  shelf_layout_manager_->PrepareForShutdown();

  Shell::Get()->focus_cycler()->RemoveWidget(status_area_widget_.get());

  Shell::Get()->focus_cycler()->RemoveWidget(navigation_widget_.get());
  Shell::Get()->focus_cycler()->RemoveWidget(hotseat_widget_.get());

  // Don't need to update the shelf background during shutdown.
  background_animator_.RemoveObserver(delegate_view_);
  shelf_->RemoveObserver(this);

  // Don't need to observe focus/activation during shutdown.
  Shell::Get()->focus_cycler()->RemoveWidget(this);
  SetFocusCycler(nullptr);

  // The contents view of |hotseat_widget_| may rely on |status_area_widget_|.
  // So do explicit destruction here.
  hotseat_widget_.reset();

  status_area_widget_.reset();
}

void ShelfWidget::CreateNavigationWidget(aura::Window* container) {
  DCHECK(container);
  DCHECK(!navigation_widget_);
  navigation_widget_ = std::make_unique<ShelfNavigationWidget>(
      shelf_, hotseat_widget()->GetShelfView());
  navigation_widget_->Initialize(container);
  Shell::Get()->focus_cycler()->AddWidget(navigation_widget_.get());
}

void ShelfWidget::CreateHotseatWidget(aura::Window* container) {
  DCHECK(container);
  DCHECK(!hotseat_widget_);
  hotseat_widget_ = std::make_unique<HotseatWidget>();
  hotseat_widget_->Initialize(container, shelf_);

  // Show a context menu for right clicks anywhere on the shelf widget.
  delegate_view_->set_context_menu_controller(hotseat_widget_->GetShelfView());
  hotseat_transition_animator_.reset(new HotseatTransitionAnimator(this));
  hotseat_transition_animator_->AddObserver(delegate_view_);
  hotseat_transition_animator_->AddObserver(hotseat_widget_.get());
}

void ShelfWidget::CreateStatusAreaWidget(aura::Window* status_container) {
  DCHECK(status_container);
  DCHECK(!status_area_widget_);
  status_area_widget_ =
      std::make_unique<StatusAreaWidget>(status_container, shelf_);
  status_area_widget_->Initialize();
  Shell::Get()->focus_cycler()->AddWidget(status_area_widget_.get());
  status_container->SetLayoutManager(new StatusAreaLayoutManager(this));
}

ShelfBackgroundType ShelfWidget::GetBackgroundType() const {
  return background_animator_.target_background_type();
}

int ShelfWidget::GetBackgroundAlphaValue(
    ShelfBackgroundType background_type) const {
  return SkColorGetA(background_animator_.GetBackgroundColor(background_type));
}

void ShelfWidget::OnShelfAlignmentChanged() {
  // Check added for http://crbug.com/738011.
  CHECK(status_area_widget_);
  status_area_widget_->UpdateAfterShelfAlignmentChange();
  // This call will in turn trigger a call to delegate_view_->SchedulePaint().
  delegate_view_->UpdateOpaqueBackground();
}

void ShelfWidget::OnTabletModeChanged() {
  delegate_view_->UpdateOpaqueBackground();
  hotseat_widget()->OnTabletModeChanged();

  // Resets |is_hotseat_forced_to_show| when leaving the tablet mode.
  if (!IsInTabletMode())
    is_hotseat_forced_to_show_ = false;

  shelf_layout_manager()->UpdateVisibilityState();
}

void ShelfWidget::PostCreateShelf() {
  SetFocusCycler(Shell::Get()->focus_cycler());
  hotseat_widget()->SetFocusCycler(Shell::Get()->focus_cycler());

  shelf_layout_manager_->LayoutShelf();
  shelf_layout_manager_->UpdateAutoHideState();
  ShowIfHidden();
}

bool ShelfWidget::IsShowingAppList() const {
  return GetHomeButton() && GetHomeButton()->IsShowingAppList();
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

  gfx::Rect bounds(
      hotseat_widget()->GetShelfView()->GetIdealBoundsOfItemIcon(id));
  gfx::Point screen_origin;
  views::View::ConvertPointToScreen(hotseat_widget()->GetShelfView(),
                                    &screen_origin);
  return gfx::Rect(screen_origin.x() + bounds.x(),
                   screen_origin.y() + bounds.y(), bounds.width(),
                   bounds.height());
}

HomeButton* ShelfWidget::GetHomeButton() const {
  return navigation_widget_.get()->GetHomeButton();
}

BackButton* ShelfWidget::GetBackButton() const {
  return navigation_widget_.get()->GetBackButton();
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

void ShelfWidget::OnSessionStateChanged(session_manager::SessionState state) {
  // Do not show shelf widget:
  // * when views based shelf is disabled
  // * in UNKNOWN state - it might be called before shelf was initialized
  // * on secondary screens in states other than ACTIVE
  //
  // TODO(alemate): better handle show-hide for some UI screens:
  // https://crbug.com/935842
  // https://crbug.com/935844
  // https://crbug.com/935846
  // https://crbug.com/935847
  // https://crbug.com/935852
  // https://crbug.com/935853
  // https://crbug.com/935856
  // https://crbug.com/935857
  // https://crbug.com/935858
  // https://crbug.com/935860
  // https://crbug.com/935861
  // https://crbug.com/935863
  bool using_views_shelf = IsUsingViewsShelf();
  bool unknown_state = state == session_manager::SessionState::UNKNOWN;
  bool hide_on_secondary_screen = shelf_->ShouldHideOnSecondaryDisplay(state);
  if (!using_views_shelf || unknown_state || hide_on_secondary_screen) {
    HideIfShown();
  } else {
    bool show_hotseat = (state == session_manager::SessionState::ACTIVE);
    hotseat_widget()->GetShelfView()->SetVisible(show_hotseat);
    login_shelf_view()->SetVisible(!show_hotseat);
    delegate_view_->SetLayoutManager(
        show_hotseat ? nullptr : std::make_unique<views::FillLayout>());

    // When FillLayout is no longer the layout manager, ensure the correct size
    // for the drag handle is set.
    if (show_hotseat)
      delegate_view_->UpdateDragHandle();

    ShowIfHidden();
  }
  login_shelf_view_->UpdateAfterSessionChange();
}

void ShelfWidget::OnUserSessionAdded(const AccountId& account_id) {
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

void ShelfWidget::OnMouseEvent(ui::MouseEvent* event) {
  if (event->IsMouseWheelEvent()) {
    ui::MouseWheelEvent* mouse_wheel_event = event->AsMouseWheelEvent();
    shelf_->ProcessMouseWheelEvent(mouse_wheel_event);
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
  shelf_layout_manager()->ProcessGestureEventFromShelfWidget(&event_in_screen);
  if (!event->handled())
    views::Widget::OnGestureEvent(event);
}

void ShelfWidget::OnAccessibilityStatusChanged() {
  // Only handles when the spoken feedback is disabled.
  if (Shell::Get()->accessibility_controller()->spoken_feedback_enabled())
    return;

  if (!is_hotseat_forced_to_show_)
    return;

  is_hotseat_forced_to_show_ = false;
  shelf_layout_manager_->UpdateVisibilityState();
}

}  // namespace ash
