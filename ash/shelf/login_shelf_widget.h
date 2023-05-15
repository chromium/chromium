// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_LOGIN_SHELF_WIDGET_H_
#define ASH_SHELF_LOGIN_SHELF_WIDGET_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/shelf/shelf_component.h"
#include "ui/views/widget/widget.h"

namespace ash {
class Shelf;
class LoginShelfView;

// The widget showing the login shelf. Exists separately from `ShelfWidget` so
// that the login shelf can be focused without stacking the shelf widget above
// other shelf components.
class ASH_EXPORT LoginShelfWidget : public ShelfComponent,
                                    public views::Widget,
                                    public SessionObserver {
 public:
  LoginShelfWidget(Shelf* shelf, aura::Window* container);
  LoginShelfWidget(const LoginShelfWidget&) = delete;
  LoginShelfWidget& operator=(const LoginShelfWidget&) = delete;
  ~LoginShelfWidget() override;

  // Specifies whether the default focused view is the login shelf's last
  // focusable child.
  void SetDefaultLastFocusableChild(bool reverse);

  void SetLoginShelfButtonOpacity(float target_opacity);

  // Called when shelf layout manager detects a locale change.
  void HandleLocaleChange();

  // ShelfComponent:
  void CalculateTargetBounds() override;
  gfx::Rect GetTargetBounds() const override;
  void UpdateLayout(bool animate) override;
  void UpdateTargetBoundsForGesture(int shelf_position) override {}

  LoginShelfView* login_shelf_view() { return login_shelf_view_; }

 private:
  // views::Widget:
  bool OnNativeWidgetActivationChanged(bool active) override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnUserSessionAdded(const AccountId& account_id) override;

  const raw_ptr<Shelf> shelf_;

  class LoginShelfWidgetDelegate;
  raw_ptr<LoginShelfWidgetDelegate> delegate_;

  ScopedSessionObserver scoped_session_observer_;

  raw_ptr<LoginShelfView> login_shelf_view_ = nullptr;

  // The target widget bounds in screen coordinates.
  gfx::Rect target_bounds_;
};

}  // namespace ash

#endif  // ASH_SHELF_LOGIN_SHELF_WIDGET_H_
