// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_EXPANDED_PUBLIC_ACCOUNT_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_EXPANDED_PUBLIC_ACCOUNT_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/public_account_menu_view.h"
#include "ash/style/system_shadow.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event_handler.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

class PrefRegistrySimple;

namespace ash {

class MonitoringWarningView;
class ArrowButtonView;
struct LocaleItem;
class LoginUserView;
class RightPaneView;
class PublicAccountMonitoringInfoDialog;
struct LoginUserInfo;

// Implements an expanded view for the public account user to select language
// and keyboard options.
class ASH_EXPORT LoginExpandedPublicAccountView : public NonAccessibleView {
  METADATA_HEADER(LoginExpandedPublicAccountView, NonAccessibleView)

 public:
  // TestApi is used for tests to get internal implementation details.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(LoginExpandedPublicAccountView* view);
    ~TestApi();

    LoginUserView* user_view();
    views::View* advanced_view_button();
    ArrowButtonView* submit_button();
    views::View* advanced_view();
    PublicAccountMonitoringInfoDialog* learn_more_dialog();
    views::StyledLabel* learn_more_label();
    PublicAccountMenuView* language_menu_view();
    PublicAccountMenuView* keyboard_menu_view();
    std::string selected_language_item_value();
    std::string selected_keyboard_item_value();
    views::ImageView* monitoring_warning_icon();
    views::Label* monitoring_warning_label();
    void ResetUserForTest();
    bool SelectLanguage(const std::string& language_code);
    bool SelectKeyboard(const std::string& ime_id);
    std::vector<LocaleItem> GetLocales();

   private:
    const raw_ptr<LoginExpandedPublicAccountView> view_;
  };

  using OnPublicSessionViewDismissed = base::RepeatingClosure;
  explicit LoginExpandedPublicAccountView(
      const OnPublicSessionViewDismissed& on_dismissed);

  LoginExpandedPublicAccountView(const LoginExpandedPublicAccountView&) =
      delete;
  LoginExpandedPublicAccountView& operator=(
      const LoginExpandedPublicAccountView&) = delete;

  ~LoginExpandedPublicAccountView() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  void ProcessPressedEvent(const ui::LocatedEvent* event);
  void UpdateForUser(const LoginUserInfo& user);
  const LoginUserInfo& current_user() const;
  void Hide();
  void ShowWarningDialog();
  void OnLearnMoreDialogClosed();
  void SetShowFullManagementDisclosure(bool show_full_management_disclosure);

  static gfx::Size GetPreferredSizeLandscape();
  static gfx::Size GetPreferredSizePortrait();

  // views::View:
  void Layout(PassKey) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

 private:
  void UseLandscapeLayout();
  void UsePortraitLayout();

  raw_ptr<views::BoxLayout> layout_ = nullptr;
  raw_ptr<LoginUserView> user_view_ = nullptr;
  raw_ptr<MonitoringWarningView> monitoring_warning_view_ = nullptr;
  raw_ptr<views::View> left_pane_ = nullptr;
  raw_ptr<views::View> separator_ = nullptr;
  raw_ptr<RightPaneView> right_pane_ = nullptr;
  raw_ptr<ArrowButtonView> submit_button_ = nullptr;

  OnPublicSessionViewDismissed on_dismissed_;
  raw_ptr<PublicAccountMonitoringInfoDialog> learn_more_dialog_ = nullptr;
  std::unique_ptr<ui::EventHandler> event_handler_;
  std::unique_ptr<SystemShadow> shadow_;

  base::WeakPtrFactory<LoginExpandedPublicAccountView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_EXPANDED_PUBLIC_ACCOUNT_VIEW_H_
