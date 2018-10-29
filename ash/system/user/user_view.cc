// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/user/user_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/multi_profile_uma.h"
#include "ash/public/cpp/ash_typography.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/user/button_from_view.h"
#include "ash/system/user/login_status.h"
#include "ash/system/user/rounded_image_view.h"
#include "ash/system/user/user_card_view.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/painter.h"
#include "ui/views/view_model.h"
#include "ui/wm/core/transient_window_manager.h"

namespace ash {
namespace tray {

namespace {

// Vertical mergin for the top/bottom edges of the view.
constexpr size_t kVerticalMargin = 2;

// Button that appears in the top-right of the system tray popup. Usually says
// "Sign out".
class LogoutButton : public views::LabelButton {
 public:
  // Creates a button with CONTEXT_TRAY_POPUP_BUTTON to get the right font.
  LogoutButton(views::ButtonListener* listener, const base::string16& text)
      : LabelButton(listener, text, CONTEXT_TRAY_POPUP_BUTTON) {
    const int kHorizontalPadding = 20;
    SetBorder(views::CreateEmptyBorder(gfx::Insets(0, kHorizontalPadding)));
    SetHorizontalAlignment(gfx::ALIGN_CENTER);
    SetFocusPainter(TrayPopupUtils::CreateFocusPainter());
    TrayPopupUtils::ConfigureTrayPopupButton(this);

    // By a series of consequences (see https://crbug.com/779732#c21 ), this
    // button was using gfx::kChromeIconGrey, but unrelated flags would change
    // that. Ideally, the LabelButton constructor would take an appropriate
    // views::style constant, but this is currently the only LabelButton wanting
    // this behavior, and https://crbug.com/842079 removes this entire button.
    SetEnabledTextColors(gfx::kChromeIconGrey);
  }

  ~LogoutButton() override = default;

  // views::LabelButton:
  int GetHeightForWidth(int width) const override { return kMenuButtonSize; }

 private:
  // TODO(estade,bruthig): there's a lot in common here with ActionableView.
  // Find a way to share. See related TODO on InkDropHostView::SetInkDropMode().
  std::unique_ptr<views::InkDrop> CreateInkDrop() override {
    return TrayPopupUtils::CreateInkDrop(this);
  }

  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override {
    return TrayPopupUtils::CreateInkDropRipple(
        TrayPopupInkDropStyle::INSET_BOUNDS, this,
        GetInkDropCenterBasedOnLastEvent());
  }

  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override {
    return TrayPopupUtils::CreateInkDropHighlight(
        TrayPopupInkDropStyle::INSET_BOUNDS, this);
  }

  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override {
    return TrayPopupUtils::CreateInkDropMask(
        TrayPopupInkDropStyle::INSET_BOUNDS, this);
  }

  DISALLOW_COPY_AND_ASSIGN(LogoutButton);
};

// Switch to a user with the given |user_index|.
void SwitchUser(UserIndex user_index) {
  // Do not switch users when the log screen is presented.
  SessionController* controller = Shell::Get()->session_controller();
  if (controller->IsUserSessionBlocked())
    return;

  // |user_index| must be in range (0, number_of_user). Note 0 is excluded
  // because it represents the active user and SwitchUser should not be called
  // for such case.
  DCHECK_GT(user_index, 0);
  DCHECK_LT(user_index, controller->NumberOfLoggedInUsers());

  MultiProfileUMA::RecordSwitchActiveUser(
      MultiProfileUMA::SWITCH_ACTIVE_USER_BY_TRAY);
  controller->SwitchActiveUser(
      controller->GetUserSession(user_index)->user_info->account_id);
}

// Returns true when clicking the user card should show the user dropdown menu.
bool IsUserDropdownEnabled() {
  // Don't allow user add or switch when screen cast warning dialog is open.
  // See http://crrev.com/291276 and http://crbug.com/353170.
  if (Shell::IsSystemModalWindowOpen())
    return false;

  // Don't allow at login, lock or when adding a multi-profile user.
  SessionController* session = Shell::Get()->session_controller();
  if (session->IsUserSessionBlocked())
    return false;

  // Show if we can add or switch users.
  return session->GetAddUserPolicy() == AddUserSessionPolicy::ALLOWED ||
         session->NumberOfLoggedInUsers() > 1;
}

// Creates the view shown in the user switcher popup ("AddUserMenuOption").
views::View* CreateAddUserView(AddUserSessionPolicy policy) {
  auto* view = new views::View();
  const int icon_padding = (kMenuButtonSize - kMenuIconSize) / 2;
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, gfx::Insets(),
      kTrayPopupLabelHorizontalPadding + icon_padding);
  layout->set_minimum_cross_axis_size(kTrayPopupItemMinHeight);
  view->SetLayoutManager(std::move(layout));
  view->SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));

  base::string16 message;
  switch (policy) {
    case AddUserSessionPolicy::ALLOWED: {
      message = l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_SIGN_IN_ANOTHER_ACCOUNT);

      auto* icon = new views::ImageView();
      icon->SetImage(
          gfx::CreateVectorIcon(kSystemMenuNewUserIcon, kMenuIconColor));
      view->AddChildView(icon);
      break;
    }
    case AddUserSessionPolicy::ERROR_NOT_ALLOWED_PRIMARY_USER:
      message = l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_MESSAGE_NOT_ALLOWED_PRIMARY_USER);
      break;
    case AddUserSessionPolicy::ERROR_MAXIMUM_USERS_REACHED:
      message = l10n_util::GetStringFUTF16Int(
          IDS_ASH_STATUS_TRAY_MESSAGE_CANNOT_ADD_USER,
          session_manager::kMaximumNumberOfUserSessions);
      break;
    case AddUserSessionPolicy::ERROR_NO_ELIGIBLE_USERS:
      message =
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_MESSAGE_OUT_OF_USERS);
      break;
  }

  auto* command_label = new views::Label(message);
  command_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  command_label->SetMultiLine(true);

  TrayPopupItemStyle label_style(
      TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL);
  int vertical_padding = kMenuSeparatorVerticalPadding;
  if (policy != AddUserSessionPolicy::ALLOWED) {
    label_style.set_font_style(TrayPopupItemStyle::FontStyle::CAPTION);
    label_style.set_color_style(TrayPopupItemStyle::ColorStyle::INACTIVE);
    vertical_padding += kMenuSeparatorVerticalPadding;
  }
  label_style.SetupLabel(command_label);
  view->AddChildView(command_label);
  view->SetBorder(views::CreateEmptyBorder(vertical_padding, icon_padding,
                                           vertical_padding,
                                           kTrayPopupLabelHorizontalPadding));

  return view;
}

// A view that acts as the contents of the widget that appears when clicking
// the active user. If the mouse exits this view or an otherwise unhandled
// click is detected, it will invoke a closure passed at construction time.
class UserDropdownWidgetContents : public views::View,
                                   public views::FocusTraversable {
 public:
  explicit UserDropdownWidgetContents(base::OnceClosure close_widget_callback)
      : close_widget_callback_(std::move(close_widget_callback)),
        view_model_(std::make_unique<views::ViewModel>()),
        focus_search_(
            std::make_unique<DropDownFocusSearch>(this, view_model_.get())) {
    // Don't want to receive a mouse exit event when the cursor enters a child.
    set_notify_enter_exit_on_child(true);
  }

  ~UserDropdownWidgetContents() override = default;

  void CloseWidget() { std::move(close_widget_callback_).Run(); }

  views::ViewModel* view_model() { return view_model_.get(); }

  // views::View:
  FocusTraversable* GetPaneFocusTraversable() override { return this; }
  bool OnMousePressed(const ui::MouseEvent& event) override { return true; }
  void OnMouseReleased(const ui::MouseEvent& event) override { CloseWidget(); }
  void OnMouseExited(const ui::MouseEvent& event) override { CloseWidget(); }
  void OnGestureEvent(ui::GestureEvent* event) override { CloseWidget(); }

  // views::FocusTraversable:
  views::FocusSearch* GetFocusSearch() override { return focus_search_.get(); }
  FocusTraversable* GetFocusTraversableParent() override { return nullptr; }
  View* GetFocusTraversableParentView() override { return nullptr; }

 private:
  // Custom FocusSearch used to navigate the drop down widget. The navigation is
  // according to the view model, which should be populated the same order the
  // views are added, but this search also closes the widget when tabbing past
  // the boudaries.
  class DropDownFocusSearch : public views::FocusSearch {
   public:
    DropDownFocusSearch(UserDropdownWidgetContents* owner,
                        views::ViewModel* view_model)
        : views::FocusSearch(nullptr, true, true),
          owner_(owner),
          view_model_(view_model) {}
    ~DropDownFocusSearch() override = default;

    // views::FocusSearch:
    views::View* FindNextFocusableView(
        views::View* starting_view,
        SearchDirection search_direction,
        TraversalDirection traversal_direction,
        StartingViewPolicy check_starting_view,
        AnchoredDialogPolicy can_go_into_anchored_dialog,
        views::FocusTraversable** focus_traversable,
        views::View** focus_traversable_view) override {
      int index = view_model_->GetIndexOfView(starting_view);
      index += search_direction == SearchDirection::kForwards ? 1 : -1;
      if (index == -1 || index == view_model_->view_size()) {
        // |close_widget_| will delete the widget which has the view which owns
        // this custom search, so do not run callback immediately.
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(&DropDownFocusSearch::CloseWidget,
                                      base::Unretained(this)));
        return nullptr;
      }
      return view_model_->view_at(index);
    }

   private:
    void CloseWidget() { owner_->CloseWidget(); }

    UserDropdownWidgetContents* owner_;
    views::ViewModel* view_model_;

    DISALLOW_COPY_AND_ASSIGN(DropDownFocusSearch);
  };

  base::OnceClosure close_widget_callback_;
  // Used to manage the focusable items in the drop down widget. There is a
  // view per item in |view_model_|.
  std::unique_ptr<views::ViewModel> view_model_;
  std::unique_ptr<DropDownFocusSearch> focus_search_;

  DISALLOW_COPY_AND_ASSIGN(UserDropdownWidgetContents);
};

// This border reserves 4dp above and 8dp below and paints a horizontal
// separator 3dp below the host view.
class ActiveUserBorder : public views::Border {
 public:
  ActiveUserBorder() = default;
  ~ActiveUserBorder() override = default;

  // views::Border:
  void Paint(const views::View& view, gfx::Canvas* canvas) override {
    const int separator_width = TrayConstants::separator_width();
    canvas->FillRect(
        gfx::Rect(
            0, view.height() - kMenuSeparatorVerticalPadding - separator_width,
            view.width(), separator_width),
        kMenuSeparatorColor);
  }

  gfx::Insets GetInsets() const override {
    return gfx::Insets(kVerticalMargin + kMenuSeparatorVerticalPadding,
                       kMenuExtraMarginFromLeftEdge,
                       kVerticalMargin + kMenuSeparatorVerticalPadding, 0);
  }

  gfx::Size GetMinimumSize() const override { return gfx::Size(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(ActiveUserBorder);
};

}  // namespace

UserView::UserView(SystemTrayItem* owner, LoginStatus login) : owner_(owner) {
  CHECK_NE(LoginStatus::NOT_LOGGED_IN, login);
  // The logout button must be added before the user card so that the user card
  // can correctly calculate the remaining available width.
  AddLogoutButton(login);
  AddUserCard(login);

  auto* layout = SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kHorizontal));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  layout->SetFlexForView(user_card_container_, 1);

  SetBorder(std::make_unique<ActiveUserBorder>());
}

UserView::~UserView() {
  HideUserDropdownWidget();
}

TrayUser::TestState UserView::GetStateForTest() const {
  if (user_dropdown_widget_)
    return add_user_enabled_ ? TrayUser::ACTIVE : TrayUser::ACTIVE_BUT_DISABLED;

  // If the container is the user card view itself, there's no ButtonFromView
  // wrapping it.
  if (user_card_container_ == user_card_view_)
    return TrayUser::SHOWN;

  return static_cast<ButtonFromView*>(user_card_container_)
                 ->is_hovered_for_test()
             ? TrayUser::HOVERED
             : TrayUser::SHOWN;
}

gfx::Rect UserView::GetBoundsInScreenOfUserButtonForTest() {
  return user_card_container_->GetBoundsInScreen();
}

int UserView::GetHeightForWidth(int width) const {
  return GetPreferredSize().height();
}

void UserView::ButtonPressed(views::Button* sender, const ui::Event& event) {
  if (sender == logout_button_) {
    Shell::Get()->metrics()->RecordUserMetricsAction(UMA_STATUS_AREA_SIGN_OUT);
    HideUserDropdownWidget();
    Shell::Get()->session_controller()->RequestSignOut();
  } else if (sender == user_card_container_ && IsUserDropdownEnabled()) {
    ToggleUserDropdownWidget(event.IsKeyEvent());
  } else if (user_dropdown_widget_ &&
             sender->GetWidget() == user_dropdown_widget_.get()) {
    DCHECK_EQ(Shell::Get()->session_controller()->NumberOfLoggedInUsers(),
              sender->parent()->child_count() - 1);
    const int index_in_add_menu = sender->parent()->GetIndexOf(sender);
    // The last item is the "sign in another user" row.
    if (index_in_add_menu == sender->parent()->child_count() - 1) {
      MultiProfileUMA::RecordSigninUser(MultiProfileUMA::SIGNIN_USER_BY_TRAY);
      Shell::Get()->session_controller()->ShowMultiProfileLogin();
    } else {
      const int user_index = index_in_add_menu;
      SwitchUser(user_index);
    }
    HideUserDropdownWidget();
    owner_->system_tray()->CloseBubble();
  } else {
    NOTREACHED();
  }
}

void UserView::OnWillChangeFocus(View* focused_before, View* focused_now) {
  if (focused_now)
    HideUserDropdownWidget();
}

void UserView::OnDidChangeFocus(View* focused_before, View* focused_now) {
  // Nothing to do here.
}

void UserView::AddLogoutButton(LoginStatus login) {
  AddChildView(TrayPopupUtils::CreateVerticalSeparator());
  logout_button_ = new LogoutButton(
      this, user::GetLocalizedSignOutStringForStatus(login, true));
  AddChildView(logout_button_);
}

void UserView::AddUserCard(LoginStatus login) {
  DCHECK(!user_card_container_);
  DCHECK(!user_card_view_);
  user_card_view_ = new UserCardView(0);
  // The entry is clickable when the user menu can be opened.
  if (IsUserDropdownEnabled()) {
    user_card_container_ = new ButtonFromView(
        user_card_view_, this, TrayPopupInkDropStyle::INSET_BOUNDS);
  } else {
    user_card_container_ = user_card_view_;
  }
  AddChildViewAt(user_card_container_, 0);
}

void UserView::ToggleUserDropdownWidget(bool toggled_by_key_event) {
  user_dropdown_widget_toggled_by_key_event_ = toggled_by_key_event;
  if (user_dropdown_widget_) {
    HideUserDropdownWidget();
    return;
  }

  // Note: We do not need to install a global event handler to delete this
  // item since it will destroyed automatically before the menu / user menu item
  // gets destroyed.
  user_dropdown_widget_.reset(new views::Widget);
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_MENU;
  params.keep_on_top = true;
  params.accept_events = true;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_YES;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.name = "AddUserMenuOption";
  // Use |kShellWindowId_SettingBubbleContainer| as the widget has to be
  // activatable.
  params.parent = GetWidget()->GetNativeWindow()->GetRootWindow()->GetChildById(
      kShellWindowId_SettingBubbleContainer);
  user_dropdown_widget_->Init(params);

  const SessionController* const session_controller =
      Shell::Get()->session_controller();
  const AddUserSessionPolicy add_user_policy =
      session_controller->GetAddUserPolicy();
  const int separator_width = TrayConstants::separator_width();
  add_user_enabled_ = add_user_policy == AddUserSessionPolicy::ALLOWED;

  // Position the widget on top of the user card view (which is still in the
  // system menu). The top half of the widget will be transparent to allow
  // the active user to show through.
  gfx::Rect bounds = user_card_container_->GetBoundsInScreen();
  bounds.set_width(bounds.width() + separator_width);
  int row_height = bounds.height();

  UserDropdownWidgetContents* container =
      new UserDropdownWidgetContents(base::BindOnce(
          &UserView::HideUserDropdownWidget, base::Unretained(this)));
  views::View* add_user_view = CreateAddUserView(add_user_policy);
  const SkColor bg_color = add_user_view->background()->get_color();
  container->SetBorder(views::CreatePaddedBorder(
      views::CreateSolidSidedBorder(0, 0, 0, separator_width, bg_color),
      gfx::Insets(row_height, 0, 0, 0)));

  // Create the contents aside from the empty window through which the active
  // user is seen.
  views::View* user_dropdown_padding = new views::View();
  user_dropdown_padding->SetBorder(views::CreateSolidSidedBorder(
      kMenuSeparatorVerticalPadding - separator_width, 0, 0, 0, bg_color));
  user_dropdown_padding->SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
  views::Separator* separator = new views::Separator();
  separator->SetPreferredHeight(separator_width);
  separator->SetColor(
      color_utils::GetResultingPaintColor(kMenuSeparatorColor, bg_color));
  const int separator_horizontal_padding =
      (kTrayPopupItemMinStartWidth - kTrayItemSize) / 2;
  separator->SetBorder(views::CreateEmptyBorder(
      0, separator_horizontal_padding, 0, separator_horizontal_padding));
  user_dropdown_padding->AddChildView(separator);
  user_dropdown_padding->SetBackground(views::CreateThemedSolidBackground(
      user_dropdown_padding, ui::NativeTheme::kColorId_BubbleBackground));

  // Add other logged in users.
  // Helper function to add a descendant view of |widget_content_view| which
  // will need to grab focus with the custom search.
  auto add_focusable_child = [](views::View* parent, views::View* child,
                                UserDropdownWidgetContents* widget_content_view,
                                int* out_model_index) {
    DCHECK(out_model_index);
    parent->AddChildView(child);
    widget_content_view->view_model()->Add(child, *out_model_index);
    ++(*out_model_index);
  };

  int model_index = 0;
  for (int i = 1; i < session_controller->NumberOfLoggedInUsers(); ++i) {
    auto* button = new ButtonFromView(new UserCardView(i), this,
                                      TrayPopupInkDropStyle::INSET_BOUNDS);
    add_focusable_child(user_dropdown_padding, button, container, &model_index);
  }

  // Add the "add user" option or the "can't add another user" message.
  if (add_user_enabled_) {
    auto* button = new ButtonFromView(add_user_view, this,
                                      TrayPopupInkDropStyle::INSET_BOUNDS);
    button->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SIGN_IN_ANOTHER_ACCOUNT));
    add_focusable_child(user_dropdown_padding, button, container, &model_index);
  } else {
    user_dropdown_padding->AddChildView(add_user_view);
  }

  container->AddChildView(user_dropdown_padding);
  container->SetLayoutManager(std::make_unique<views::FillLayout>());
  user_dropdown_widget_->SetContentsView(container);

  bounds.set_height(container->GetPreferredSize().height());
  user_dropdown_widget_->SetBounds(bounds);

  // Suppress the appearance of the collective capture icon while the dropdown
  // is open (the icon will appear in the specific user rows).
  user_card_view_->SetSuppressCaptureIcon(true);

  // Make |user_dropdown_widget_| a transient child of the tray bubble, so that
  // the bubble does not close when |user_dropdown_widget_| loses activation.
  ::wm::TransientWindowManager::GetOrCreate(
      GetBubbleWidget()->GetNativeWindow())
      ->AddTransientChild(user_dropdown_widget_->GetNativeWindow());

  // Show the content.
  user_dropdown_widget_->SetAlwaysOnTop(true);
  user_dropdown_widget_->Show();
  if (toggled_by_key_event && container->view_model()->view_size() > 0) {
    user_dropdown_widget_->GetFocusManager()->SetFocusedView(
        container->view_model()->view_at(0));
  }

  // Install a listener to focus changes so that we can remove the card when
  // the focus gets changed. When called through the destruction of the bubble,
  // the FocusManager cannot be determined anymore and we remember it here.
  focus_manager_ = user_card_container_->GetFocusManager();
  focus_manager_->AddFocusChangeListener(this);
}

void UserView::HideUserDropdownWidget() {
  if (!user_dropdown_widget_)
    return;
  focus_manager_->RemoveFocusChangeListener(this);
  focus_manager_ = nullptr;
  if (user_card_container_->GetFocusManager()) {
    // Return activation to bubble before destroying |user_dropdown_widget_|,
    // otherwise tray_bubble_wrapper will not know |user_dropdown_widget_| is
    // a transient child of the bubble.
    if (GetBubbleWidget())
      GetBubbleWidget()->Activate();
    user_card_container_->GetFocusManager()->ClearFocus();
    if (user_dropdown_widget_toggled_by_key_event_) {
      user_card_container_->GetFocusManager()->SetFocusedView(
          user_card_container_);
    }
  }
  user_card_view_->SetSuppressCaptureIcon(false);
  user_dropdown_widget_.reset();
}

views::Widget* UserView::GetBubbleWidget() {
  if (!owner_->system_tray()->GetSystemBubble())
    return nullptr;
  return owner_->system_tray()->GetSystemBubble()->bubble_view()->GetWidget();
}

}  // namespace tray
}  // namespace ash
