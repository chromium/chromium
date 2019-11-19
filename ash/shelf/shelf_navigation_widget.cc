// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_navigation_widget.h"

#include "ash/focus_cycler.h"
#include "ash/shelf/back_button.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/background.h"
#include "ui/views/view.h"

namespace ash {
namespace {

// Duration of the back button's opacity animation.
constexpr auto kBackButtonOpacityAnimationDuration =
    base::TimeDelta::FromMilliseconds(200);

bool IsTabletMode() {
  return Shell::Get()->tablet_mode_controller() &&
         Shell::Get()->tablet_mode_controller()->InTabletMode();
}

// Returns the bounds for the first button shown in this view (the back
// button in tablet mode, the home button otherwise).
gfx::Rect GetFirstButtonBounds() {
  return gfx::Rect(0, 0, ShelfConfig::Get()->control_size(),
                   ShelfConfig::Get()->control_size());
}

// Returns the bounds for the second button shown in this view (which is
// always the home button and only in tablet mode, which implies a horizontal
// shelf).
gfx::Rect GetSecondButtonBounds() {
  return gfx::Rect(
      ShelfConfig::Get()->control_size() + ShelfConfig::Get()->button_spacing(),
      0, ShelfConfig::Get()->control_size(),
      ShelfConfig::Get()->control_size());
}

bool IsBackButtonShown() {
  return chromeos::switches::ShouldShowShelfHotseat()
             ? IsTabletMode() && ShelfConfig::Get()->is_in_app()
             : IsTabletMode();
}

}  // namespace

class ShelfNavigationWidget::Delegate : public views::AccessiblePaneView,
                                        public views::WidgetDelegate {
 public:
  Delegate(Shelf* shelf, ShelfView* shelf_view);
  ~Delegate() override;

  // Initializes the view.
  void Init(ui::Layer* parent_layer);

  void UpdateOpaqueBackground();

  // views::View:
  FocusTraversable* GetPaneFocusTraversable() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void ReorderChildLayers(ui::Layer* parent_layer) override;
  void OnBoundsChanged(const gfx::Rect& old_bounds) override;

  // views::AccessiblePaneView:
  View* GetDefaultFocusableChild() override;

  // views::WidgetDelegate:
  bool CanActivate() const override;
  views::Widget* GetWidget() override { return View::GetWidget(); }
  const views::Widget* GetWidget() const override { return View::GetWidget(); }

  BackButton* back_button() const { return back_button_; }
  HomeButton* home_button() const { return home_button_; }

  void set_default_last_focusable_child(bool default_last_focusable_child) {
    default_last_focusable_child_ = default_last_focusable_child;
  }

 private:
  void SetParentLayer(ui::Layer* layer);

  BackButton* back_button_ = nullptr;
  HomeButton* home_button_ = nullptr;
  // When true, the default focus of the navigation widget is the last
  // focusable child.
  bool default_last_focusable_child_ = false;

  // A background layer that may be visible depending on shelf state.
  ui::Layer opaque_background_;

  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

ShelfNavigationWidget::Delegate::Delegate(Shelf* shelf, ShelfView* shelf_view)
    : opaque_background_(ui::LAYER_SOLID_COLOR) {
  set_allow_deactivate_on_esc(true);

  const int control_size = ShelfConfig::Get()->control_size();
  std::unique_ptr<BackButton> back_button_ptr =
      std::make_unique<BackButton>(shelf);
  back_button_ = AddChildView(std::move(back_button_ptr));
  back_button_->SetSize(gfx::Size(control_size, control_size));

  std::unique_ptr<HomeButton> home_button_ptr =
      std::make_unique<HomeButton>(shelf);
  home_button_ = AddChildView(std::move(home_button_ptr));
  home_button_->set_context_menu_controller(shelf_view);
  home_button_->SetSize(gfx::Size(control_size, control_size));

  GetViewAccessibility().OverrideNextFocus(
      shelf->shelf_widget()->hotseat_widget());
  GetViewAccessibility().OverridePreviousFocus(shelf->GetStatusAreaWidget());
}

ShelfNavigationWidget::Delegate::~Delegate() = default;

void ShelfNavigationWidget::Delegate::Init(ui::Layer* parent_layer) {
  SetParentLayer(parent_layer);
  UpdateOpaqueBackground();
}

void ShelfNavigationWidget::Delegate::UpdateOpaqueBackground() {
  opaque_background_.SetColor(ShelfConfig::Get()->GetShelfControlButtonColor());

  if (chromeos::switches::ShouldShowShelfHotseat() && IsTabletMode() &&
      ShelfConfig::Get()->is_in_app()) {
    opaque_background_.SetVisible(false);
    return;
  }
  opaque_background_.SetVisible(true);

  int radius = ShelfConfig::Get()->control_border_radius();
  gfx::RoundedCornersF rounded_corners = {radius, radius, radius, radius};
  if (opaque_background_.rounded_corner_radii() != rounded_corners)
    opaque_background_.SetRoundedCornerRadius(rounded_corners);

  opaque_background_.SetBounds(GetLocalBounds());
  opaque_background_.SetBackgroundBlur(
      ShelfConfig::Get()->GetShelfControlButtonBlurRadius());
}

bool ShelfNavigationWidget::Delegate::CanActivate() const {
  // We don't want mouse clicks to activate us, but we need to allow
  // activation when the user is using the keyboard (FocusCycler).
  return Shell::Get()->focus_cycler()->widget_activating() == GetWidget();
}

views::FocusTraversable*
ShelfNavigationWidget::Delegate::GetPaneFocusTraversable() {
  return this;
}

void ShelfNavigationWidget::Delegate::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kToolbar;
  node_data->SetName(l10n_util::GetStringUTF8(IDS_ASH_SHELF_ACCESSIBLE_NAME));

  ShelfWidget* shelf_widget =
      Shelf::ForWindow(GetWidget()->GetNativeWindow())->shelf_widget();
  GetViewAccessibility().OverrideNextFocus(shelf_widget->hotseat_widget());
  GetViewAccessibility().OverridePreviousFocus(
      shelf_widget->status_area_widget());
}

void ShelfNavigationWidget::Delegate::ReorderChildLayers(
    ui::Layer* parent_layer) {
  views::View::ReorderChildLayers(parent_layer);
  parent_layer->StackAtBottom(&opaque_background_);
}

void ShelfNavigationWidget::Delegate::OnBoundsChanged(
    const gfx::Rect& old_bounds) {
  UpdateOpaqueBackground();
}

views::View* ShelfNavigationWidget::Delegate::GetDefaultFocusableChild() {
  return default_last_focusable_child_ ? GetLastFocusableChild()
                                       : GetFirstFocusableChild();
}

void ShelfNavigationWidget::Delegate::SetParentLayer(ui::Layer* layer) {
  layer->Add(&opaque_background_);
  ReorderLayers();
}

ShelfNavigationWidget::ShelfNavigationWidget(Shelf* shelf,
                                             ShelfView* shelf_view)
    : shelf_(shelf),
      delegate_(new ShelfNavigationWidget::Delegate(shelf, shelf_view)),
      bounds_animator_(std::make_unique<views::BoundsAnimator>(delegate_)) {
  DCHECK(shelf_);
  bounds_animator_->SetAnimationDuration(kBackButtonOpacityAnimationDuration);
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
  Shell::Get()->AddShellObserver(this);
  ShelfConfig::Get()->AddObserver(this);
}

ShelfNavigationWidget::~ShelfNavigationWidget() {
  // Shell destroys the TabletModeController before destroying all root windows.
  if (Shell::Get()->tablet_mode_controller())
    Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  Shell::Get()->RemoveShellObserver(this);
  ShelfConfig::Get()->RemoveObserver(this);
}

void ShelfNavigationWidget::Initialize(aura::Window* container) {
  DCHECK(container);
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "ShelfNavigationWidget";
  params.delegate = delegate_;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = container;
  Init(std::move(params));
  delegate_->Init(GetLayer());
  set_focus_on_creation(false);
  GetFocusManager()->set_arrow_key_traversal_enabled_for_widget(true);
  SetContentsView(delegate_);
  GetBackButton()->SetBoundsRect(GetFirstButtonBounds());
  SetSize(GetIdealSize());
  UpdateLayout();
}

gfx::Size ShelfNavigationWidget::GetIdealSize() const {
  const int control_size = ShelfConfig::Get()->control_size();
  if (!shelf_->IsHorizontalAlignment())
    return gfx::Size(control_size, control_size);

  return gfx::Size(IsBackButtonShown() ? (2 * control_size +
                                          ShelfConfig::Get()->button_spacing())
                                       : control_size,
                   control_size);
}

void ShelfNavigationWidget::OnMouseEvent(ui::MouseEvent* event) {
  if (event->IsMouseWheelEvent()) {
    ui::MouseWheelEvent* mouse_wheel_event = event->AsMouseWheelEvent();
    shelf_->ProcessMouseWheelEvent(mouse_wheel_event);
    return;
  }

  views::Widget::OnMouseEvent(event);
}

bool ShelfNavigationWidget::OnNativeWidgetActivationChanged(bool active) {
  if (!Widget::OnNativeWidgetActivationChanged(active))
    return false;
  if (active)
    delegate_->SetPaneFocusAndFocusDefault();
  return true;
}

void ShelfNavigationWidget::OnGestureEvent(ui::GestureEvent* event) {
  if (shelf_->ProcessGestureEvent(*event)) {
    event->StopPropagation();
    return;
  }
  views::Widget::OnGestureEvent(event);
}

BackButton* ShelfNavigationWidget::GetBackButton() const {
  return delegate_->back_button();
}

HomeButton* ShelfNavigationWidget::GetHomeButton() const {
  return delegate_->home_button();
}

void ShelfNavigationWidget::FocusFirstOrLastFocusableChild(bool last) {
  views::View* to_focus = GetHomeButton();
  if (IsTabletMode() && !last)
    to_focus = GetBackButton();
  GetFocusManager()->SetFocusedView(to_focus);
}

void ShelfNavigationWidget::SetDefaultLastFocusableChild(
    bool default_last_focusable_child) {
  delegate_->set_default_last_focusable_child(default_last_focusable_child);
}

void ShelfNavigationWidget::OnTabletModeStarted() {
  UpdateLayout();
}

void ShelfNavigationWidget::OnTabletModeEnded() {
  UpdateLayout();
}

void ShelfNavigationWidget::OnShelfAlignmentChanged(aura::Window* root_window) {
  UpdateLayout();
}

void ShelfNavigationWidget::OnImplicitAnimationsCompleted() {
  // Hide the back button once it has become fully transparent.
  if (!IsTabletMode() || !IsBackButtonShown())
    GetBackButton()->SetVisible(false);
}

void ShelfNavigationWidget::OnShelfConfigUpdated() {
  UpdateLayout();
}

void ShelfNavigationWidget::UpdateLayout() {
  bool is_back_button_shown = IsBackButtonShown();

  // Show the back button right away so that the animation is visible.
  if (is_back_button_shown)
    GetBackButton()->SetVisible(true);
  GetBackButton()->SetFocusBehavior(is_back_button_shown
                                        ? views::View::FocusBehavior::ALWAYS
                                        : views::View::FocusBehavior::NEVER);
  ui::ScopedLayerAnimationSettings settings(
      GetBackButton()->layer()->GetAnimator());
  settings.SetTransitionDuration(kBackButtonOpacityAnimationDuration);
  settings.AddObserver(this);
  GetBackButton()->layer()->SetOpacity(is_back_button_shown ? 1 : 0);

  bounds_animator_->AnimateViewTo(
      GetHomeButton(),
      is_back_button_shown ? GetSecondButtonBounds() : GetFirstButtonBounds());
  GetBackButton()->SetBoundsRect(GetFirstButtonBounds());

  delegate_->UpdateOpaqueBackground();
}

}  // namespace ash
