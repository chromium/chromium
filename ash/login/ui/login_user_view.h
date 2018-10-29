// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_USER_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_USER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/login/ui/login_display_style.h"
#include "ash/public/interfaces/login_user_info.mojom.h"
#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace ash {

class HoverNotifier;
class LoginBubble;
class LoginButton;

// Display the user's profile icon, name, and a menu icon in various layout
// styles.
class ASH_EXPORT LoginUserView : public views::View,
                                 public views::ButtonListener {
 public:
  // TestApi is used for tests to get internal implementation details.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(LoginUserView* view);
    ~TestApi();

    LoginDisplayStyle display_style() const;

    const base::string16& displayed_name() const;

    views::View* user_label() const;
    views::View* tap_button() const;
    views::View* dropdown() const;
    LoginBubble* menu() const;
    views::View* user_domain() const;

    bool is_opaque() const;

   private:
    LoginUserView* const view_;
  };

  using OnTap = base::RepeatingClosure;
  using OnRemoveWarningShown = base::RepeatingClosure;
  using OnRemove = base::RepeatingClosure;

  // Returns the width of this view for the given display style.
  static int WidthForLayoutStyle(LoginDisplayStyle style);

  // Use null callbacks for |on_remove_warning_shown| and |on_remove| when
  // |show_dropdown| arg is false.
  LoginUserView(LoginDisplayStyle style,
                bool show_dropdown,
                bool show_domain,
                const OnTap& on_tap,
                const OnRemoveWarningShown& on_remove_warning_shown,
                const OnRemove& on_remove);
  ~LoginUserView() override;

  // Update the user view to display the given user information.
  void UpdateForUser(const mojom::LoginUserInfoPtr& user, bool animate);

  // Set if the view must be opaque.
  void SetForceOpaque(bool force_opaque);

  // Enables or disables tapping the view.
  void SetTapEnabled(bool enabled);

  const mojom::LoginUserInfoPtr& current_user() const { return current_user_; }

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  void RequestFocus() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  class UserDomainInfoView;
  class UserImage;
  class UserLabel;
  class TapButton;

  // Called when hover state changes.
  void OnHover(bool has_hover);

  // Updates UI element values so they reflect the data in |current_user_|.
  void UpdateCurrentUserState();
  // Updates view opacity based on input state and |force_opaque_|.
  void UpdateOpacity();

  void SetLargeLayout();
  void SetSmallishLayout();

  // Executed when the user view is pressed.
  OnTap on_tap_;
  // Executed when the user has seen the remove user warning.
  OnRemoveWarningShown on_remove_warning_shown_;
  // Executed when a user-remove has been requested.
  OnRemove on_remove_;

  // The user that is currently being displayed (or will be displayed when an
  // animation completes).
  mojom::LoginUserInfoPtr current_user_;

  // Used to dispatch opacity update events.
  std::unique_ptr<HoverNotifier> hover_notifier_;

  LoginDisplayStyle display_style_;
  UserImage* user_image_ = nullptr;
  UserLabel* user_label_ = nullptr;
  LoginButton* dropdown_ = nullptr;
  TapButton* tap_button_ = nullptr;

  std::unique_ptr<LoginBubble> menu_;

  // Show the domain information for public account user.
  UserDomainInfoView* user_domain_ = nullptr;

  // True iff the view is currently opaque (ie, opacity = 1).
  bool is_opaque_ = false;
  // True if the view must be opaque (ie, opacity = 1) regardless of input
  // state.
  bool force_opaque_ = false;

  DISALLOW_COPY_AND_ASSIGN(LoginUserView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_USER_VIEW_H_
