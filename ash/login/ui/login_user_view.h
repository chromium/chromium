// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_USER_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_USER_VIEW_H_

#include "ash/ash_export.h"
#include "ash/login/ui/login_base_bubble_view.h"
#include "ash/login/ui/login_display_style.h"
#include "ash/login/ui/login_remove_account_dialog.h"
#include "ash/public/cpp/login_types.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/views/view.h"

namespace ash {

class HoverNotifier;
class LoginButton;

// Display the user's profile icon, name, and a remove_account_dialog icon in
// various layout styles.
class ASH_EXPORT LoginUserView : public views::View,
                                 public display::DisplayConfigurator::Observer {
  METADATA_HEADER(LoginUserView, views::View)

 public:
  // TestApi is used for tests to get internal implementation details.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(LoginUserView* view);
    ~TestApi();

    LoginDisplayStyle display_style() const;

    const std::u16string& displayed_name() const;

    views::View* user_label() const;
    views::View* tap_button() const;
    views::View* dropdown() const;
    views::View* enterprise_icon_container() const;

    void OnTap() const;

    bool is_opaque() const;

   private:
    const raw_ptr<LoginUserView, DanglingUntriaged> view_;
  };

  using OnTap = base::RepeatingClosure;
  using OnRemoveWarningShown = base::RepeatingClosure;
  using OnRemove = base::RepeatingClosure;
  using OnDropdownPressed = base::RepeatingClosure;

  // Returns the width of this view for the given display style.
  static int WidthForLayoutStyle(LoginDisplayStyle style);

  // Use null callbacks for |on_remove_warning_shown| and |on_remove| when
  // |show_dropdown| arg is false.
  LoginUserView(LoginDisplayStyle style,
                bool show_dropdown,
                const OnTap& on_tap,
                const OnDropdownPressed& on_dropdown_pressed);

  LoginUserView(const LoginUserView&) = delete;
  LoginUserView& operator=(const LoginUserView&) = delete;

  ~LoginUserView() override;

  // Update the user view to display the given user information.
  void UpdateForUser(const LoginUserInfo& user, bool animate);

  // Set if the view must be opaque.
  void SetForceOpaque(bool force_opaque);

  // Enables or disables tapping the view.
  void SetTapEnabled(bool enabled);

  // DisplayConfigurator::Observer
  void OnPowerStateChanged(chromeos::DisplayPowerState power_state) override;

  const LoginUserInfo& current_user() const { return current_user_; }

  // Get dropdown view that can be used as an anchor view for attaching a bubble
  // view.
  base::WeakPtr<views::View> GetDropdownAnchorView();

  // Get dropdown view as a LoginButton.
  LoginButton* GetDropdownButton();

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void Layout(PassKey) override;
  void RequestFocus() override;
  views::View::Views GetChildrenInZOrder() override;

 private:
  class UserImage;
  class UserLabel;
  class TapButton;

  // Called when hover state changes.
  void OnHover(bool has_hover);

  void DropdownButtonPressed();

  // Updates UI element values so they reflect the data in |current_user_|.
  void UpdateCurrentUserState();
  // Updates view opacity based on input state and |force_opaque_|.
  void UpdateOpacity();

  void SetLargeLayout();
  void SetSmallishLayout();

  // Executed when the user view is pressed.
  OnTap on_tap_;
  // Executed when a user-remove has been requested.
  OnDropdownPressed on_dropdown_pressed_;

  // The user that is currently being displayed (or will be displayed when an
  // animation completes).
  LoginUserInfo current_user_;

  // Used to dispatch opacity update events.
  std::unique_ptr<HoverNotifier> hover_notifier_;

  LoginDisplayStyle display_style_;
  raw_ptr<UserImage> user_image_ = nullptr;
  raw_ptr<UserLabel> user_label_ = nullptr;
  raw_ptr<LoginButton> dropdown_ = nullptr;
  raw_ptr<TapButton> tap_button_ = nullptr;

  // True iff the view is currently opaque (ie, opacity = 1).
  bool is_opaque_ = false;
  // True if the view must be opaque (ie, opacity = 1) regardless of input
  // state.
  bool force_opaque_ = false;

  base::ScopedObservation<display::DisplayConfigurator,
                          display::DisplayConfigurator::Observer>
      display_observation_{this};
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_USER_VIEW_H_
