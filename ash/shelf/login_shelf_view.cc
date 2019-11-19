// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/login_shelf_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "ash/focus_cycler.h"
#include "ash/lock_screen_action/lock_screen_action_background_state.h"
#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/login_constants.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/wm/lock_state_controller.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/metrics/user_metrics.h"
#include "base/sequence_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "skia/ext/image_operations.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

using session_manager::SessionState;

namespace ash {
namespace {

const char* kLoginShelfButtonClassName = "LoginShelfButton";

LoginMetricsRecorder::ShelfButtonClickTarget GetUserClickTarget(int button_id) {
  switch (button_id) {
    case LoginShelfView::kShutdown:
      return LoginMetricsRecorder::ShelfButtonClickTarget::kShutDownButton;
    case LoginShelfView::kRestart:
      return LoginMetricsRecorder::ShelfButtonClickTarget::kRestartButton;
    case LoginShelfView::kSignOut:
      return LoginMetricsRecorder::ShelfButtonClickTarget::kSignOutButton;
    case LoginShelfView::kCloseNote:
      return LoginMetricsRecorder::ShelfButtonClickTarget::kCloseNoteButton;
    case LoginShelfView::kBrowseAsGuest:
      return LoginMetricsRecorder::ShelfButtonClickTarget::kBrowseAsGuestButton;
    case LoginShelfView::kAddUser:
      return LoginMetricsRecorder::ShelfButtonClickTarget::kAddUserButton;
    case LoginShelfView::kCancel:
      return LoginMetricsRecorder::ShelfButtonClickTarget::kCancelButton;
    case LoginShelfView::kParentAccess:
      return LoginMetricsRecorder::ShelfButtonClickTarget::kParentAccessButton;
  }
  return LoginMetricsRecorder::ShelfButtonClickTarget::kTargetCount;
}

// The margins of the button contents.
constexpr int kButtonMarginTopDp = 18;
constexpr int kButtonMarginLeftDp = 18;
constexpr int kButtonMarginBottomDp = 18;
constexpr int kButtonMarginRightDp = 16;

// Spacing between the button image and label.
constexpr int kImageLabelSpacingDp = 10;

// The color of the button background.
constexpr SkColor kButtonBackgroundColor =
    SkColorSetA(gfx::kGoogleGrey100, 0x19);

// The color of the button text.
constexpr SkColor kButtonTextColor = gfx::kGoogleGrey100;

// The color of the button icon.
constexpr SkColor kButtonIconColor = SkColorSetRGB(0xEB, 0xEA, 0xED);

SkPath GetButtonHighlightPath(const views::View* view) {
  gfx::Rect rect(view->GetLocalBounds());
  const int height_inset =
      (ShelfConfig::Get()->shelf_size() - ShelfConfig::Get()->control_size()) /
      2;
  rect.Inset(gfx::Insets(height_inset, ShelfConfig::Get()->button_spacing(),
                         height_inset, 0));

  const int border_radius = ShelfConfig::Get()->control_border_radius();
  return SkPath().addRoundRect(gfx::RectToSkRect(rect), border_radius,
                               border_radius);
}

class LoginShelfButtonHighlightPathGenerator
    : public views::HighlightPathGenerator {
 public:
  LoginShelfButtonHighlightPathGenerator() = default;

  // views::HighlightPathGenerator:
  SkPath GetHighlightPath(const views::View* view) override {
    return GetButtonHighlightPath(view);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginShelfButtonHighlightPathGenerator);
};

class LoginShelfButton : public views::LabelButton {
 public:
  LoginShelfButton(views::ButtonListener* listener,
                   int text_resource_id,
                   const gfx::VectorIcon& icon)
      : LabelButton(listener, l10n_util::GetStringUTF16(text_resource_id)),
        text_resource_id_(text_resource_id),
        icon_(icon) {
    SetAccessibleName(GetText());
    SetImage(views::Button::STATE_NORMAL,
             gfx::CreateVectorIcon(icon, kButtonIconColor));
    SetImage(views::Button::STATE_DISABLED,
             gfx::CreateVectorIcon(
                 icon, SkColorSetA(kButtonIconColor,
                                   login_constants::kButtonDisabledAlpha)));
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetInstallFocusRingOnFocus(true);
    views::HighlightPathGenerator::Install(
        this, std::make_unique<LoginShelfButtonHighlightPathGenerator>());
    focus_ring()->SetColor(ShelfConfig::Get()->shelf_focus_border_color());
    SetFocusPainter(nullptr);
    SetInkDropMode(InkDropMode::ON);
    set_has_ink_drop_action_on_click(true);
    set_ink_drop_base_color(ShelfConfig::Get()->shelf_ink_drop_base_color());
    set_ink_drop_visible_opacity(
        ShelfConfig::Get()->shelf_ink_drop_visible_opacity());

    // Layer rendering is required when the shelf background is visible, which
    // happens when the wallpaper is not blurred.
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);

    SetTextSubpixelRenderingEnabled(false);

    SetImageLabelSpacing(kImageLabelSpacingDp);
    SetEnabledTextColors(kButtonTextColor);
    SetTextColor(
        views::Button::STATE_DISABLED,
        SkColorSetA(kButtonTextColor, login_constants::kButtonDisabledAlpha));
    label()->SetFontList(views::Label::GetDefaultFontList().Derive(
        1, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL));
  }

  ~LoginShelfButton() override = default;

  int text_resource_id() const { return text_resource_id_; }

  // views::LabelButton:
  gfx::Insets GetInsets() const override {
    return gfx::Insets(0, kButtonMarginLeftDp, 0, kButtonMarginRightDp);
  }

  const char* GetClassName() const override {
    return kLoginShelfButtonClassName;
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(kButtonBackgroundColor);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawPath(GetButtonHighlightPath(this), flags);
  }

  std::unique_ptr<views::InkDrop> CreateInkDrop() override {
    auto ink_drop = std::make_unique<views::InkDropImpl>(this, size());
    ink_drop->SetShowHighlightOnHover(false);
    ink_drop->SetShowHighlightOnFocus(false);
    return ink_drop;
  }

  base::string16 GetTooltipText(const gfx::Point& p) const override {
    if (label()->IsDisplayTextTruncated())
      return label()->GetText();
    return base::string16();
  }

  void PaintDarkColors() {
    SetEnabledTextColors(gfx::kGoogleGrey600);
    SetImage(views::Button::STATE_NORMAL,
             gfx::CreateVectorIcon(icon_, gfx::kGoogleGrey600));
  }

  void PaintLightColors() {
    SetEnabledTextColors(kButtonTextColor);
    SetImage(views::Button::STATE_NORMAL,
             gfx::CreateVectorIcon(icon_, kButtonTextColor));
  }

 private:
  const int text_resource_id_;
  const gfx::VectorIcon& icon_;

  DISALLOW_COPY_AND_ASSIGN(LoginShelfButton);
};

void StartAddUser() {
  Shell::Get()->login_screen_controller()->ShowGaiaSignin(
      true /*can_close*/, EmptyAccountId() /*prefilled_account*/);
}

bool DialogStateGuestAllowed(OobeDialogState state) {
  // Temp solution until https://crbug.com/981544 is fixed.
  if (state == OobeDialogState::HIDDEN)
    return true;

  return state == OobeDialogState::NONE ||
         state == OobeDialogState::GAIA_SIGNIN ||
         state == OobeDialogState::ERROR;
}

}  // namespace

class KioskAppsButton : public views::MenuButton,
                        public views::ButtonListener,
                        public ui::SimpleMenuModel,
                        public ui::SimpleMenuModel::Delegate {
 public:
  KioskAppsButton()
      : MenuButton(l10n_util::GetStringUTF16(IDS_ASH_SHELF_APPS_BUTTON), this),
        ui::SimpleMenuModel(this) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetInstallFocusRingOnFocus(true);
    views::HighlightPathGenerator::Install(
        this, std::make_unique<LoginShelfButtonHighlightPathGenerator>());
    focus_ring()->SetColor(ShelfConfig::Get()->shelf_focus_border_color());
    SetFocusPainter(nullptr);
    SetInkDropMode(InkDropMode::ON);
    set_has_ink_drop_action_on_click(true);
    set_ink_drop_base_color(ShelfConfig::Get()->shelf_ink_drop_base_color());
    set_ink_drop_visible_opacity(
        ShelfConfig::Get()->shelf_ink_drop_visible_opacity());

    // Layer rendering is required when the shelf background is visible, which
    // happens when the wallpaper is not blurred.
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);

    SetTextSubpixelRenderingEnabled(false);

    SetImage(views::Button::STATE_NORMAL,
             CreateVectorIcon(kShelfAppsButtonIcon, kButtonIconColor));
    SetImageLabelSpacing(kImageLabelSpacingDp);
    SetEnabledTextColors(kButtonTextColor);
    label()->SetFontList(views::Label::GetDefaultFontList().Derive(
        1, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL));
  }

  bool LaunchAppForTesting(const std::string& app_id) {
    for (size_t i = 0; i < kiosk_apps_.size(); ++i) {
      if (kiosk_apps_[i].app_id == app_id) {
        ExecuteCommand(i, 0);
        return true;
      }
    }
    return false;
  }

  // Replace the existing items list with a new list of kiosk app menu items.
  void SetApps(const std::vector<KioskAppMenuEntry>& kiosk_apps,
               const base::RepeatingCallback<void(const KioskAppMenuEntry&)>&
                   launch_app) {
    launch_app_callback_ = launch_app;
    kiosk_apps_ = kiosk_apps;
    Clear();
    const gfx::Size kAppIconSize(16, 16);
    for (size_t i = 0; i < kiosk_apps_.size(); ++i) {
      gfx::ImageSkia icon = gfx::ImageSkiaOperations::CreateResizedImage(
          kiosk_apps_[i].icon, skia::ImageOperations::RESIZE_GOOD,
          kAppIconSize);
      AddItemWithIcon(i, kiosk_apps_[i].name, icon);
    }
  }

  bool HasApps() const { return !kiosk_apps_.empty(); }

  // views::MenuButton:
  gfx::Insets GetInsets() const override {
    return gfx::Insets(kButtonMarginTopDp, kButtonMarginLeftDp,
                       kButtonMarginBottomDp, kButtonMarginRightDp);
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(kButtonBackgroundColor);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawPath(GetButtonHighlightPath(this), flags);
  }

  void SetVisible(bool visible) override {
    MenuButton::SetVisible(visible);
    if (visible)
      is_launch_enabled_ = true;
  }

  std::unique_ptr<views::InkDrop> CreateInkDrop() override {
    auto ink_drop = std::make_unique<views::InkDropImpl>(this, size());
    ink_drop->SetShowHighlightOnHover(false);
    ink_drop->SetShowHighlightOnFocus(false);
    return ink_drop;
  }

  void PaintDarkColors() {
    SetEnabledTextColors(gfx::kGoogleGrey600);
    SetImage(views::Button::STATE_NORMAL,
             CreateVectorIcon(kShelfAppsButtonIcon, gfx::kGoogleGrey600));
  }

  void PaintLightColors() {
    SetEnabledTextColors(kButtonTextColor);
    SetImage(views::Button::STATE_NORMAL,
             CreateVectorIcon(kShelfAppsButtonIcon, kButtonIconColor));
  }

  // views::ButtonListener:
  void ButtonPressed(Button* source, const ui::Event& event) override {
    if (!is_launch_enabled_)
      return;
    menu_runner_.reset(
        new views::MenuRunner(this, views::MenuRunner::HAS_MNEMONICS));

    const gfx::Point point = source->GetMenuPosition();
    const gfx::Point origin(point.x() - source->width(),
                            point.y() - source->height());
    menu_runner_->RunMenuAt(source->GetWidget()->GetTopLevelWidget(),
                            button_controller(), gfx::Rect(origin, gfx::Size()),
                            views::MenuAnchorPosition::kTopLeft,
                            ui::MENU_SOURCE_NONE);
  }

  // ui::SimpleMenuModel:
  void ExecuteCommand(int command_id, int event_flags) override {
    DCHECK(command_id >= 0 &&
           base::checked_cast<size_t>(command_id) < kiosk_apps_.size());
    // Once an app is clicked on, don't allow any additional clicks until
    // the state is reset (when login screen reappears).
    is_launch_enabled_ = false;

    launch_app_callback_.Run(kiosk_apps_[command_id]);
  }

  bool IsCommandIdChecked(int command_id) const override { return false; }

  bool IsCommandIdEnabled(int command_id) const override { return true; }

 private:
  base::RepeatingCallback<void(const KioskAppMenuEntry&)> launch_app_callback_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  std::vector<KioskAppMenuEntry> kiosk_apps_;
  bool is_launch_enabled_ = true;

  DISALLOW_COPY_AND_ASSIGN(KioskAppsButton);
};

// Class that temporarily disables Guest login buttin on shelf.
class LoginShelfView::ScopedGuestButtonBlockerImpl
    : public ScopedGuestButtonBlocker {
 public:
  ScopedGuestButtonBlockerImpl(base::WeakPtr<LoginShelfView> shelf_view)
      : shelf_view_(shelf_view) {
    ++(shelf_view_->scoped_guest_button_blockers_);
    if (shelf_view_->scoped_guest_button_blockers_ == 1)
      shelf_view_->UpdateUi();
  }

  ~ScopedGuestButtonBlockerImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!shelf_view_)
      return;

    DCHECK_GT(shelf_view_->scoped_guest_button_blockers_, 0);
    --(shelf_view_->scoped_guest_button_blockers_);
    if (!shelf_view_->scoped_guest_button_blockers_)
      shelf_view_->UpdateUi();
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // ScopedGuestButtonBlockerImpl is not owned by the LoginShelfView,
  // so they could be independently destroyed.
  base::WeakPtr<LoginShelfView> shelf_view_;
};

LoginShelfView::TestUiUpdateDelegate::~TestUiUpdateDelegate() = default;

LoginShelfView::LoginShelfView(
    LockScreenActionBackgroundController* lock_screen_action_background)
    : lock_screen_action_background_(lock_screen_action_background) {
  // We reuse the focusable state on this view as a signal that focus should
  // switch to the lock screen or status area. This view should otherwise not
  // be focusable.
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));

  auto add_button = [this](ButtonId id, int text_resource_id,
                           const gfx::VectorIcon& icon) {
    LoginShelfButton* button =
        new LoginShelfButton(this, text_resource_id, icon);
    button->SetID(id);
    AddChildView(button);
  };
  add_button(kShutdown, IDS_ASH_SHELF_SHUTDOWN_BUTTON,
             kShelfShutdownButtonIcon);
  kiosk_apps_button_ = new KioskAppsButton();
  kiosk_apps_button_->SetID(kApps);
  AddChildView(kiosk_apps_button_);
  add_button(kRestart, IDS_ASH_SHELF_RESTART_BUTTON, kShelfShutdownButtonIcon);
  add_button(kSignOut, IDS_ASH_SHELF_SIGN_OUT_BUTTON, kShelfSignOutButtonIcon);
  add_button(kCloseNote, IDS_ASH_SHELF_UNLOCK_BUTTON, kShelfUnlockButtonIcon);
  add_button(kCancel, IDS_ASH_SHELF_CANCEL_BUTTON, kShelfCancelButtonIcon);
  add_button(kBrowseAsGuest, IDS_ASH_BROWSE_AS_GUEST_BUTTON,
             kShelfBrowseAsGuestButtonIcon);
  add_button(kAddUser, IDS_ASH_ADD_USER_BUTTON, kShelfAddPersonButtonIcon);
  add_button(kParentAccess, IDS_ASH_PARENT_ACCESS_BUTTON,
             kParentAccessLockIcon);

  // Adds observers for states that affect the visiblity of different buttons.
  tray_action_observer_.Add(Shell::Get()->tray_action());
  shutdown_controller_observer_.Add(Shell::Get()->shutdown_controller());
  lock_screen_action_background_observer_.Add(lock_screen_action_background);
  locale_change_observer_.Add(Shell::Get()->locale_update_controller());
  login_data_dispatcher_observer_.Add(
      Shell::Get()->login_screen_controller()->data_dispatcher());
  UpdateUi();
}

LoginShelfView::~LoginShelfView() = default;

void LoginShelfView::UpdateAfterSessionChange() {
  UpdateUi();
}

const char* LoginShelfView::GetClassName() const {
  return "LoginShelfView";
}

void LoginShelfView::OnFocus() {
  LOG(WARNING) << "LoginShelfView was focused, but this should never happen. "
                  "Forwarded focus to shelf widget with an unknown direction.";
  Shell::Get()->focus_cycler()->FocusWidget(
      Shelf::ForWindow(GetWidget()->GetNativeWindow())->shelf_widget());
}

void LoginShelfView::AboutToRequestFocusFromTabTraversal(bool reverse) {
  if (reverse) {
    // Focus should leave the system tray.
    Shell::Get()->system_tray_notifier()->NotifyFocusOut(reverse);
  } else {
    // Focus goes to status area.
    StatusAreaWidget* status_area_widget =
        Shelf::ForWindow(GetWidget()->GetNativeWindow())->GetStatusAreaWidget();
    status_area_widget->status_area_widget_delegate()
        ->set_default_last_focusable_child(reverse);
    Shell::Get()->focus_cycler()->FocusWidget(status_area_widget);
  }
}

void LoginShelfView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (LockScreen::HasInstance()) {
    GetViewAccessibility().OverridePreviousFocus(LockScreen::Get()->widget());
  }

  Shelf* shelf = Shelf::ForWindow(GetWidget()->GetNativeWindow());

  GetViewAccessibility().OverrideNextFocus(shelf->GetStatusAreaWidget());
  node_data->role = ax::mojom::Role::kToolbar;
  node_data->SetName(l10n_util::GetStringUTF8(IDS_ASH_SHELF_ACCESSIBLE_NAME));
}

void LoginShelfView::ButtonPressed(views::Button* sender,
                                   const ui::Event& event) {
  UserMetricsRecorder::RecordUserClickOnShelfButton(
      GetUserClickTarget(sender->GetID()));
  switch (sender->GetID()) {
    case kShutdown:
    case kRestart:
      // |ShutdownController| will further distinguish the two cases based on
      // shutdown policy.
      Shell::Get()->lock_state_controller()->RequestShutdown(
          ShutdownReason::LOGIN_SHUT_DOWN_BUTTON);
      break;
    case kSignOut:
      base::RecordAction(base::UserMetricsAction("ScreenLocker_Signout"));
      Shell::Get()->session_controller()->RequestSignOut();
      break;
    case kCloseNote:
      Shell::Get()->tray_action()->CloseLockScreenNote(
          mojom::CloseLockScreenNoteReason::kUnlockButtonPressed);
      break;
    case kCancel:
      // If the Cancel button has focus, clear it. Otherwise the shelf within
      // active session may still be focused.
      GetFocusManager()->ClearFocus();
      Shell::Get()->login_screen_controller()->CancelAddUser();
      break;
    case kBrowseAsGuest:
      Shell::Get()->login_screen_controller()->LoginAsGuest();
      break;
    case kAddUser:
      StartAddUser();
      break;
    case kParentAccess:
      // TODO(https://crbug.com/999387): Remove this when handling touch
      // cancellation is fixed for system modal windows.
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce([]() {
            LockScreen::Get()->ShowParentAccessDialog();
          }));
      break;
    default:
      NOTREACHED();
  }
}

bool LoginShelfView::LaunchAppForTesting(const std::string& app_id) {
  return kiosk_apps_button_->GetEnabled() &&
         kiosk_apps_button_->LaunchAppForTesting(app_id);
}

bool LoginShelfView::SimulateAddUserButtonForTesting() {
  views::View* add_user_button = GetViewByID(kAddUser);
  if (!add_user_button->GetEnabled())
    return false;

  StartAddUser();
  return true;
}

bool LoginShelfView::SimulateGuestButtonForTesting() {
  views::View* guest_login_button = GetViewByID(kBrowseAsGuest);
  if (!guest_login_button->GetEnabled())
    return false;

  Shell::Get()->login_screen_controller()->LoginAsGuest();
  return true;
}

void LoginShelfView::InstallTestUiUpdateDelegate(
    std::unique_ptr<TestUiUpdateDelegate> delegate) {
  DCHECK(!test_ui_update_delegate_.get());
  test_ui_update_delegate_ = std::move(delegate);
}

void LoginShelfView::SetKioskApps(
    const std::vector<KioskAppMenuEntry>& kiosk_apps,
    const base::RepeatingCallback<void(const KioskAppMenuEntry&)>& launch_app) {
  kiosk_apps_button_->SetApps(kiosk_apps, launch_app);
  UpdateUi();
}

void LoginShelfView::SetLoginDialogState(OobeDialogState state) {
  dialog_state_ = state;
  UpdateUi();
}

void LoginShelfView::SetAllowLoginAsGuest(bool allow_guest) {
  allow_guest_ = allow_guest;
  UpdateUi();
}

void LoginShelfView::ShowParentAccessButton(bool show) {
  show_parent_access_ = show;
  UpdateUi();
}

void LoginShelfView::ShowGuestButtonInOobe(bool show) {
  allow_guest_in_oobe_ = show;
  UpdateUi();
}

void LoginShelfView::SetAddUserButtonEnabled(bool enable_add_user) {
  GetViewByID(kAddUser)->SetEnabled(enable_add_user);
}

void LoginShelfView::SetShutdownButtonEnabled(bool enable_shutdown_button) {
  GetViewByID(kShutdown)->SetEnabled(enable_shutdown_button);
}

std::unique_ptr<ScopedGuestButtonBlocker>
LoginShelfView::GetScopedGuestButtonBlocker() {
  return std::make_unique<LoginShelfView::ScopedGuestButtonBlockerImpl>(
      weak_ptr_factory_.GetWeakPtr());
}

void LoginShelfView::OnLockScreenNoteStateChanged(
    mojom::TrayActionState state) {
  UpdateUi();
}

void LoginShelfView::OnLockScreenActionBackgroundStateChanged(
    LockScreenActionBackgroundState state) {
  UpdateUi();
}

void LoginShelfView::OnShutdownPolicyChanged(bool reboot_on_shutdown) {
  UpdateUi();
}

void LoginShelfView::OnUsersChanged(const std::vector<LoginUserInfo>& users) {
  login_screen_has_users_ = !users.empty();
  UpdateUi();
}

void LoginShelfView::OnOobeDialogStateChanged(OobeDialogState state) {
  SetLoginDialogState(state);
}

void LoginShelfView::OnLocaleChanged() {
  for (views::View* child : children()) {
    if (child->GetClassName() == kLoginShelfButtonClassName) {
      auto* button = static_cast<LoginShelfButton*>(child);
      button->SetText(l10n_util::GetStringUTF16(button->text_resource_id()));
      button->SetAccessibleName(button->GetText());
    }
  }
}

bool LoginShelfView::LockScreenActionBackgroundAnimating() const {
  return lock_screen_action_background_->state() ==
             LockScreenActionBackgroundState::kShowing ||
         lock_screen_action_background_->state() ==
             LockScreenActionBackgroundState::kHiding;
}

void LoginShelfView::UpdateUi() {
  // Make sure observers are notified.
  base::ScopedClosureRunner fire_observer(base::BindOnce(
      [](LoginShelfView* self) {
        if (self->test_ui_update_delegate())
          self->test_ui_update_delegate()->OnUiUpdate();
      },
      base::Unretained(this)));

  SessionState session_state =
      Shell::Get()->session_controller()->GetSessionState();
  if (session_state == SessionState::ACTIVE) {
    // The entire view was set invisible. The buttons are also set invisible
    // to avoid affecting calculation of the shelf size.
    for (auto* child : children())
      child->SetVisible(false);

    return;
  }
  bool show_reboot = Shell::Get()->shutdown_controller()->reboot_on_shutdown();
  mojom::TrayActionState tray_action_state =
      Shell::Get()->tray_action()->GetLockScreenNoteState();
  bool is_locked = (session_state == SessionState::LOCKED);
  bool is_lock_screen_note_in_foreground =
      (tray_action_state == mojom::TrayActionState::kActive ||
       tray_action_state == mojom::TrayActionState::kLaunching) &&
      !LockScreenActionBackgroundAnimating();

  // TODO: https://crbug.com/935849
  GetViewByID(kShutdown)->SetVisible(!show_reboot &&
                                     !is_lock_screen_note_in_foreground);
  GetViewByID(kRestart)->SetVisible(show_reboot &&
                                    !is_lock_screen_note_in_foreground);
  GetViewByID(kSignOut)->SetVisible(is_locked &&
                                    !is_lock_screen_note_in_foreground);
  GetViewByID(kCloseNote)
      ->SetVisible(is_locked && is_lock_screen_note_in_foreground);
  GetViewByID(kCancel)->SetVisible(session_state ==
                                   SessionState::LOGIN_SECONDARY);
  GetViewByID(kParentAccess)->SetVisible(is_locked && show_parent_access_);

  bool is_login_primary = (session_state == SessionState::LOGIN_PRIMARY);
  bool dialog_visible = dialog_state_ != OobeDialogState::HIDDEN;
  bool is_oobe = (session_state == SessionState::OOBE);

  GetViewByID(kBrowseAsGuest)->SetVisible(ShouldShowGuestButton());

  // Show add user button when it's in login screen and Oobe UI dialog is not
  // visible. The button should not appear if the device is not connected to a
  // network.
  GetViewByID(kAddUser)->SetVisible(!dialog_visible && is_login_primary);
  // Show kiosk apps button if:
  // 1. It's in login screen.
  // 2. There are Kiosk apps available.
  // 3. Oobe UI dialog is not visible or is currently showing gaia signin
  // screen.
  kiosk_apps_button_->SetVisible(
      (!dialog_visible || dialog_state_ == OobeDialogState::GAIA_SIGNIN) &&
      kiosk_apps_button_->HasApps() && (is_login_primary || is_oobe));

  UpdateButtonColors(is_oobe);
  Layout();
  UpdateButtonUnionBounds();
}

void LoginShelfView::UpdateButtonColors(bool use_dark_colors) {
  if (use_dark_colors) {
    static_cast<LoginShelfButton*>(GetViewByID(kShutdown))->PaintDarkColors();
    static_cast<LoginShelfButton*>(GetViewByID(kRestart))->PaintDarkColors();
    static_cast<LoginShelfButton*>(GetViewByID(kSignOut))->PaintDarkColors();
    static_cast<LoginShelfButton*>(GetViewByID(kCloseNote))->PaintDarkColors();
    static_cast<LoginShelfButton*>(GetViewByID(kCancel))->PaintDarkColors();
    static_cast<LoginShelfButton*>(GetViewByID(kParentAccess))
        ->PaintDarkColors();
    static_cast<LoginShelfButton*>(GetViewByID(kBrowseAsGuest))
        ->PaintDarkColors();
    static_cast<LoginShelfButton*>(GetViewByID(kAddUser))->PaintDarkColors();
    kiosk_apps_button_->PaintDarkColors();
  } else {
    static_cast<LoginShelfButton*>(GetViewByID(kShutdown))->PaintLightColors();
    static_cast<LoginShelfButton*>(GetViewByID(kRestart))->PaintLightColors();
    static_cast<LoginShelfButton*>(GetViewByID(kSignOut))->PaintLightColors();
    static_cast<LoginShelfButton*>(GetViewByID(kCloseNote))->PaintLightColors();
    static_cast<LoginShelfButton*>(GetViewByID(kCancel))->PaintLightColors();
    static_cast<LoginShelfButton*>(GetViewByID(kParentAccess))
        ->PaintLightColors();
    static_cast<LoginShelfButton*>(GetViewByID(kBrowseAsGuest))
        ->PaintLightColors();
    static_cast<LoginShelfButton*>(GetViewByID(kAddUser))->PaintLightColors();
    kiosk_apps_button_->PaintLightColors();
  }
}

void LoginShelfView::UpdateButtonUnionBounds() {
  button_union_bounds_ = gfx::Rect();
  View::Views children = GetChildrenInZOrder();
  for (auto* child : children) {
    if (child->GetVisible())
      button_union_bounds_.Union(child->bounds());
  }
}

// Show guest button if:
// 1. Guest login is allowed.
// 2. OOBE UI dialog is currently showing the login UI or error.
// 3. No users sessions have started. Button is hidden from all post login
// screens like sync consent, etc.
// 4. It's in login screen or OOBE. Note: In OOBE, the guest button visibility
// is manually controlled by the WebUI.
// 5. OOBE UI dialog is not currently showing gaia signin screen, or if there
// are no user views available. If there are no user pods (i.e. Gaia is the
// only signin option), the guest button should be shown if allowed by policy
// and OOBE.
// 6. There are no scoped guest buttons blockers active.
bool LoginShelfView::ShouldShowGuestButton() const {
  if (!allow_guest_)
    return false;

  if (scoped_guest_button_blockers_ > 0)
    return false;

  if (!DialogStateGuestAllowed(dialog_state_))
    return false;

  const bool user_session_started =
      Shell::Get()->session_controller()->NumberOfLoggedInUsers() != 0;
  if (user_session_started)
    return false;

  const SessionState session_state =
      Shell::Get()->session_controller()->GetSessionState();

  if (session_state == SessionState::OOBE)
    return allow_guest_in_oobe_;

  if (session_state != SessionState::LOGIN_PRIMARY)
    return false;

  if (dialog_state_ == OobeDialogState::GAIA_SIGNIN)
    return !login_screen_has_users_ && allow_guest_in_oobe_;

  return true;
}

}  // namespace ash
