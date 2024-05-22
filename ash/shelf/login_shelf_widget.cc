// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/login_shelf_widget.h"

#include "ash/focus_cycler.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/login_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

// LoginShelfWidget::LoginShelfWidgetDelegate ----------------------------------
// The delegate of the login shelf widget.

class LoginShelfWidget::LoginShelfWidgetDelegate
    : public views::AccessiblePaneView,
      public views::WidgetDelegate {
 public:
  explicit LoginShelfWidgetDelegate(Shelf* shelf) : shelf_(shelf) {
    SetOwnedByWidget(true);
    set_allow_deactivate_on_esc(true);
    SetLayoutManager(std::make_unique<views::FillLayout>());
  }

  LoginShelfWidgetDelegate(const LoginShelfWidgetDelegate&) = delete;
  LoginShelfWidgetDelegate& operator=(const LoginShelfWidgetDelegate&) = delete;

  ~LoginShelfWidgetDelegate() override = default;

  // views::View:
  views::View* GetDefaultFocusableChild() override {
    // `login_shelf_view` is added to the widget delegate as a child when the
    // login shelf widget is constructed and is removed when the widget is
    // destructed. Therefore, `login_shelf_view` is not null here.
    views::View* login_shelf_view = children()[0];

    views::FocusSearch search(login_shelf_view, default_last_focusable_child_,
                              /*accessibility_mode=*/false);
    views::FocusTraversable* dummy_focus_traversable;
    views::View* dummy_focus_traversable_view;

    return search.FindNextFocusableView(
        login_shelf_view,
        default_last_focusable_child_
            ? views::FocusSearch::SearchDirection::kBackwards
            : views::FocusSearch::SearchDirection::kForwards,
        views::FocusSearch::TraversalDirection::kDown,
        views::FocusSearch::StartingViewPolicy::kSkipStartingView,
        views::FocusSearch::AnchoredDialogPolicy::kCanGoIntoAnchoredDialog,
        &dummy_focus_traversable, &dummy_focus_traversable_view);
  }

  // views::WidgetDelegate:
  bool CanActivate() const override {
    // We don't want mouse clicks to activate us, but we need to allow
    // activation when the user is using the keyboard (FocusCycler).
    bool can_active = Shell::Get()->focus_cycler()->widget_activating() ==
                      views::View::GetWidget();
    return can_active;
  }

  void ChildPreferredSizeChanged(views::View* child) override {
    shelf_->shelf_layout_manager()->LayoutShelf(/*animate=*/false);
  }

  void set_default_last_focusable_child(bool reverse) {
    default_last_focusable_child_ = reverse;
  }

 private:
  const raw_ptr<Shelf> shelf_ = nullptr;

  // When true, the default focus of the shelf is the last focusable child.
  bool default_last_focusable_child_ = false;
};

// LoginShelfWidget ------------------------------------------------------------

LoginShelfWidget::LoginShelfWidget(Shelf* shelf, aura::Window* container)
    : shelf_(shelf),
      delegate_(new LoginShelfWidgetDelegate(shelf)),
      scoped_session_observer_(this) {
  DCHECK(container);
  login_shelf_view_ = delegate_->AddChildView(std::make_unique<LoginShelfView>(
      RootWindowController::ForWindow(container)
          ->lock_screen_action_background_controller()));

  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "LoginShelfWidget";
  params.delegate = delegate_;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.parent = container;
  Init(std::move(params));
  SetContentsView(delegate_);

  // Hide the login shelf by default.
  Hide();

  // TODO(https://crbug.com/1343114): currently, some logics in shelf check the
  // login shelf view's visibility. Update them to check whether the login shelf
  // view is drawn or the widget's visibility. Then remove this line.
  login_shelf_view_->SetVisible(false);
}

LoginShelfWidget::~LoginShelfWidget() = default;

void LoginShelfWidget::SetDefaultLastFocusableChild(bool reverse) {
  delegate_->set_default_last_focusable_child(reverse);
}

void LoginShelfWidget::SetLoginShelfButtonOpacity(float target_opacity) {
  login_shelf_view_->SetButtonOpacity(target_opacity);
}

void LoginShelfWidget::HandleLocaleChange() {
  login_shelf_view_->HandleLocaleChange();
}

void LoginShelfWidget::CalculateTargetBounds() {
  const gfx::Point shelf_origin =
      shelf_->shelf_widget()->GetTargetBounds().origin();

  gfx::Point origin = gfx::Point(shelf_origin.x(), shelf_origin.y());
  const int target_bounds_width = delegate_->GetPreferredSize().width();
  if (shelf_->IsHorizontalAlignment() && base::i18n::IsRTL()) {
    origin.set_x(shelf_->shelf_widget()->GetTargetBounds().size().width() -
                 target_bounds_width);
  }

  target_bounds_ =
      gfx::Rect(origin, {target_bounds_width,
                         shelf_->shelf_widget()->GetTargetBounds().height()});
}

gfx::Rect LoginShelfWidget::GetTargetBounds() const {
  return target_bounds_;
}

void LoginShelfWidget::UpdateLayout(bool animate) {
  if (GetNativeView()->bounds() == target_bounds_)
    return;

  SetBounds(target_bounds_);
}

bool LoginShelfWidget::OnNativeWidgetActivationChanged(bool active) {
  if (!Widget::OnNativeWidgetActivationChanged(active))
    return false;

  if (active)
    delegate_->SetPaneFocusAndFocusDefault();

  return true;
}

void LoginShelfWidget::OnSessionStateChanged(
    session_manager::SessionState state) {
  // The login shelf should be hidden if:
  // 1. the user session is active; or
  // 2. the RMA app is active. The login shelf should be hidden to avoid
  // blocking the RMA app controls or intercepting UI events.
  bool hide_for_session_state =
      (state == session_manager::SessionState::ACTIVE ||
       state == session_manager::SessionState::RMA);

  // The visibility of `login_shelf_view_` is accessed in different places.
  // Therefore, ensure the consistency between the widget's visibility and the
  // view's visibility.
  if (!hide_for_session_state && !shelf_->ShouldHideOnSecondaryDisplay(state)) {
    if (!IsVisible()) {
      Show();
      login_shelf_view_->SetVisible(true);
    }
  } else if (IsVisible()) {
    Hide();
    login_shelf_view_->SetVisible(false);
  }

  login_shelf_view_->UpdateAfterSessionChange();
}

void LoginShelfWidget::OnUserSessionAdded(const AccountId& account_id) {
  login_shelf_view_->UpdateAfterSessionChange();
}

}  // namespace ash
