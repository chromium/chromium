// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/login_shelf_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/focus_cycler.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/lock_screen_action/lock_screen_action_background_state.h"
#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/metrics/login_metrics_recorder.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/login_accelerators.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/kiosk_app_instruction_bubble.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_shutdown_confirmation_bubble.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/pill_button.h"
#include "ash/style/style_util.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/wm/lock_state_controller.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/user_metrics.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "skia/ext/image_operations.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
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

// Skip only that many to avoid blocking users in case of any subtle bugs.
const int kMaxDroppedCallsWhenDisplaysOff = 3;

constexpr LoginShelfView::ButtonId kButtonIds[] = {
    LoginShelfView::kShutdown,
    LoginShelfView::kRestart,
    LoginShelfView::kSignOut,
    LoginShelfView::kCloseNote,
    LoginShelfView::kCancel,
    LoginShelfView::kParentAccess,
    LoginShelfView::kBrowseAsGuest,
    LoginShelfView::kAddUser,
    LoginShelfView::kEnterpriseEnrollment,
    LoginShelfView::kSignIn,
    LoginShelfView::kOsInstall,
};

// TODO(1190978): Remove this check once light mode is the default mode.
bool IsOobe() {
  return Shell::Get()->session_controller()->GetSessionState() ==
         SessionState::OOBE;
}

ui::ColorId GetButtonTextColorId() {
  return IsOobe() ? kColorAshButtonLabelColorLight : kColorAshButtonLabelColor;
}

ui::ColorId GetButtonIconColorId() {
  return IsOobe() ? kColorAshButtonIconColorLight : kColorAshButtonIconColor;
}

ui::ColorId GetButtonBackgroundColorId() {
  return IsOobe() ? cros_tokens::kCrosSysSystemOnBase
                  : cros_tokens::kCrosSysSystemOnBase1;
}

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
    case LoginShelfView::kEnterpriseEnrollment:
      return LoginMetricsRecorder::ShelfButtonClickTarget::
          kEnterpriseEnrollmentButton;
    case LoginShelfView::kSignIn:
      return LoginMetricsRecorder::ShelfButtonClickTarget::kSignIn;
    case LoginShelfView::kOsInstall:
      return LoginMetricsRecorder::ShelfButtonClickTarget::kOsInstallButton;
  }
  return LoginMetricsRecorder::ShelfButtonClickTarget::kTargetCount;
}

void ButtonPressed(int id, base::RepeatingClosure callback) {
  UserMetricsRecorder::RecordUserClickOnShelfButton(GetUserClickTarget(id));
  std::move(callback).Run();
}

// The margins of the button contents.
constexpr int kButtonMarginRightDp = PillButton::kPillButtonHorizontalSpacing;
constexpr int kButtonMarginLeftDp = kButtonMarginRightDp - 4;

// Spacing between the button image and label.
constexpr int kImageLabelSpacingDp = 8;

void AnimateButtonOpacity(ui::Layer* layer,
                          float target_opacity,
                          base::TimeDelta animation_duration,
                          gfx::Tween::Type type) {
  ui::ScopedLayerAnimationSettings animation_setter(layer->GetAnimator());
  animation_setter.SetTransitionDuration(animation_duration);
  animation_setter.SetTweenType(type);
  animation_setter.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  layer->SetOpacity(target_opacity);
}

}  // namespace

class LoginShelfButton : public PillButton {
 public:
  LoginShelfButton(PressedCallback callback,
                   int text_resource_id,
                   const gfx::VectorIcon& icon)
      : PillButton(std::move(callback),
                   l10n_util::GetStringUTF16(text_resource_id),
                   PillButton::Type::kDefaultLargeWithIconLeading,
                   &icon,
                   PillButton::kPillButtonHorizontalSpacing),
        text_resource_id_(text_resource_id),
        icon_(icon) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    set_suppress_default_focus_handling();
    SetFocusPainter(nullptr);
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  }

  LoginShelfButton(const LoginShelfButton&) = delete;
  LoginShelfButton& operator=(const LoginShelfButton&) = delete;

  ~LoginShelfButton() override = default;

  int text_resource_id() const { return text_resource_id_; }

  const char* GetClassName() const override {
    return kLoginShelfButtonClassName;
  }

  std::u16string GetTooltipText(const gfx::Point& p) const override {
    if (label()->IsDisplayTextTruncated())
      return label()->GetText();
    return std::u16string();
  }

  void UpdateButtonColors() {
    SetEnabledTextColorIds(GetButtonTextColorId());
    if (chromeos::features::IsJellyrollEnabled()) {
      SetBackgroundColorId(GetButtonBackgroundColorId());
    }
    SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(*icon_, GetButtonIconColorId()));
  }

  void OnFocus() override {
    auto* const keyboard_controller = keyboard::KeyboardUIController::Get();
    keyboard_controller->set_keyboard_locked(false /*lock*/);
    keyboard_controller->HideKeyboardImplicitlyByUser();
  }

 private:
  const int text_resource_id_;
  const raw_ref<const gfx::VectorIcon, ExperimentalAsh> icon_;
};

class KioskAppsButton : public views::MenuButton,
                        public ui::SimpleMenuModel,
                        public ui::SimpleMenuModel::Delegate {
 public:
  KioskAppsButton()
      : MenuButton(base::BindRepeating(
                       [](KioskAppsButton* button) {
                         if (button->is_launch_enabled_)
                           button->DisplayMenu();
                       },
                       this),
                   l10n_util::GetStringUTF16(IDS_ASH_SHELF_APPS_BUTTON)),
        ui::SimpleMenuModel(this) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    set_suppress_default_focus_handling();
    SetInstallFocusRingOnFocus(true);
    views::FocusRing::Get(this)->SetColorId(ui::kColorAshFocusRing);
    views::InstallRoundRectHighlightPathGenerator(
        this, gfx::Insets(), ShelfConfig::Get()->control_border_radius());
    SetFocusPainter(nullptr);
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
    SetHasInkDropActionOnClick(true);
    views::InkDrop::UseInkDropWithoutAutoHighlight(
        views::InkDrop::Get(this), /*highlight_on_hover=*/false);

    StyleUtil::ConfigureInkDropAttributes(
        this, StyleUtil::kBaseColor | StyleUtil::kInkDropOpacity);

    // Layer rendering is required when the shelf background is visible, which
    // happens when the wallpaper is not blurred.
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);

    SetTextSubpixelRenderingEnabled(false);

    SetImageLabelSpacing(kImageLabelSpacingDp);
    label()->SetFontList(views::Label::GetDefaultFontList().Derive(
        1, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  }

  KioskAppsButton(const KioskAppsButton&) = delete;
  KioskAppsButton& operator=(const KioskAppsButton&) = delete;

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
  void SetApps(const std::vector<KioskAppMenuEntry>& kiosk_apps) {
    kiosk_apps_ = kiosk_apps;
    Clear();
    const gfx::Size kAppIconSize(16, 16);
    for (size_t i = 0; i < kiosk_apps_.size(); ++i) {
      gfx::ImageSkia icon = gfx::ImageSkiaOperations::CreateResizedImage(
          kiosk_apps_[i].icon, skia::ImageOperations::RESIZE_GOOD,
          kAppIconSize);
      AddItemWithIcon(i, kiosk_apps_[i].name,
                      ui::ImageModel::FromImageSkia(icon));
    }

    // If the menu is being shown, update it.
    if (menu_runner_ && menu_runner_->IsRunning()) {
      DisplayMenu();
    }
  }

  void ConfigureKioskCallbacks(
      const base::RepeatingCallback<void(const KioskAppMenuEntry&)>& launch_app,
      const base::RepeatingClosure& on_show_menu,
      const base::RepeatingClosure& on_close_menu) {
    launch_app_callback_ = launch_app;
    on_show_menu_ = on_show_menu;
    on_close_menu_ = on_close_menu;
  }

  bool HasApps() const { return !kiosk_apps_.empty(); }

  int GetHeightForWidth(int width) const override {
    return ShelfConfig::Get()->control_size();
  }

  // views::MenuButton:
  gfx::Insets GetInsets() const override {
    return gfx::Insets::TLBR(0, kButtonMarginLeftDp, 0, kButtonMarginRightDp);
  }

  SkPath GetButtonHighlightPath(const views::View* view) {
    gfx::Rect rect(view->GetLocalBounds());

    const int border_radius = ShelfConfig::Get()->control_border_radius();
    return SkPath().addRoundRect(gfx::RectToSkRect(rect), border_radius,
                                 border_radius);
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(ShelfConfig::Get()->GetShelfControlButtonColor(GetWidget()));
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawPath(GetButtonHighlightPath(this), flags);
  }

  void SetVisible(bool visible) override {
    MenuButton::SetVisible(visible);
    if (visible)
      is_launch_enabled_ = true;
  }

  void UpdateButtonColors() {
    SetEnabledTextColorIds(GetButtonTextColorId());
    SetImageModel(views::Button::STATE_NORMAL,
                  ui::ImageModel::FromVectorIcon(kShelfAppsButtonIcon,
                                                 GetButtonIconColorId()));
  }

  void DisplayMenu() {
    const gfx::Point point = GetMenuPosition();
    const gfx::Point origin(point.x() - width(), point.y() - height());
    menu_runner_ = std::make_unique<views::MenuRunner>(
        this, views::MenuRunner::HAS_MNEMONICS);
    menu_runner_->RunMenuAt(GetWidget()->GetTopLevelWidget(),
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

  void OnMenuWillShow(SimpleMenuModel* source) override {
    is_menu_opened_ = true;
    on_show_menu_.Run();
  }

  void MenuClosed(SimpleMenuModel* source) override {
    on_close_menu_.Run();
    is_menu_opened_ = false;
  }

  bool IsCommandIdChecked(int command_id) const override { return false; }

  bool IsCommandIdEnabled(int command_id) const override { return true; }

  bool IsMenuOpened() { return is_menu_opened_; }

 private:
  base::RepeatingCallback<void(const KioskAppMenuEntry&)> launch_app_callback_;
  base::RepeatingClosure on_show_menu_;
  base::RepeatingClosure on_close_menu_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  std::vector<KioskAppMenuEntry> kiosk_apps_;

  bool is_launch_enabled_ = true;
  bool is_menu_opened_ = false;
};

// Class that temporarily disables Guest login buttin on shelf.
class LoginShelfView::ScopedGuestButtonBlockerImpl
    : public ScopedGuestButtonBlocker {
 public:
  explicit ScopedGuestButtonBlockerImpl(
      base::WeakPtr<LoginShelfView> shelf_view)
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

void LoginShelfView::CallIfDisplayIsOn(const base::RepeatingClosure& closure) {
  if (!Shell::Get()->display_configurator()->IsDisplayOn() &&
      dropped_calls_when_displays_off_ < kMaxDroppedCallsWhenDisplaysOff) {
    ++dropped_calls_when_displays_off_;
    return;
  }
  dropped_calls_when_displays_off_ = 0;
  closure.Run();
}

void LoginShelfView::OnRequestShutdownConfirmed() {
  test_shutdown_confirmation_bubble_ = nullptr;
  Shell::Get()->lock_state_controller()->RequestShutdown(
      ShutdownReason::LOGIN_SHUT_DOWN_BUTTON);
}

void LoginShelfView::OnRequestShutdownCancelled() {
  test_shutdown_confirmation_bubble_ = nullptr;
}

void LoginShelfView::RequestShutdown() {
  base::RecordAction(base::UserMetricsAction("Shelf_ShutDown"));
  if (base::FeatureList::IsEnabled(features::kShutdownConfirmationBubble)) {
    Shelf* shelf = Shelf::ForWindow(GetWidget()->GetNativeWindow());
    // When the created ShelfShutdownConfirmationBubble is destroyed, it would
    // call LoginShelfView::OnRequestShutdownCancelled() in the destructor to
    // ensure that the pointer test_shutdown_confirmation_bubble_ here is
    // cleaned up.
    // And ShelfShutdownConfirmationBubble would be destroyed when it's
    // dismissed or its buttons were presses.
    test_shutdown_confirmation_bubble_ = new ShelfShutdownConfirmationBubble(
        GetViewByID(kShutdown), shelf->alignment(),
        base::BindOnce(&LoginShelfView::OnRequestShutdownConfirmed,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&LoginShelfView::OnRequestShutdownCancelled,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    OnRequestShutdownConfirmed();
  }
}

LoginShelfView::LoginShelfView(
    LockScreenActionBackgroundController* lock_screen_action_background)
    : lock_screen_action_background_(lock_screen_action_background) {
  ShelfConfig::Get()->AddObserver(this);
  // We reuse the focusable state on this view as a signal that focus should
  // switch to the lock screen or status area. This view should otherwise not
  // be focusable.
  SetFocusBehavior(FocusBehavior::ALWAYS);
  std::unique_ptr<views::BoxLayout> box_layout =
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  box_layout->set_between_child_spacing(ShelfConfig::Get()->button_spacing());
  box_layout->set_inside_border_insets(
      gfx::Insets::TLBR(0, ShelfConfig::Get()->button_spacing(), 0, 0));
  SetLayoutManager(std::move(box_layout));

  auto add_button = [this](ButtonId id, base::RepeatingClosure callback,
                           int text_resource_id, const gfx::VectorIcon& icon) {
    LoginShelfButton* button = new LoginShelfButton(
        base::BindRepeating(&ButtonPressed, id, std::move(callback)),
        text_resource_id, icon);
    button->SetID(id);
    AddChildView(button);
    login_shelf_buttons_.push_back(button);
  };
  const auto shutdown_callback = base::BindRepeating(
      &LoginShelfView::RequestShutdown, weak_ptr_factory_.GetWeakPtr());
  add_button(
      kShutdown,
      base::BindRepeating(&LoginShelfView::CallIfDisplayIsOn,
                          weak_ptr_factory_.GetWeakPtr(), shutdown_callback),
      IDS_ASH_SHELF_SHUTDOWN_BUTTON, kShelfShutdownButtonIcon);
  const auto restart_callback = base::BindRepeating(
      &LockStateController::RequestShutdown,
      base::Unretained(Shell::Get()->lock_state_controller()),
      ShutdownReason::LOGIN_SHUT_DOWN_BUTTON);
  add_button(kRestart, std::move(restart_callback),
             IDS_ASH_SHELF_RESTART_BUTTON, kShelfShutdownButtonIcon);
  add_button(
      kSignOut,
      base::BindRepeating(
          &LoginShelfView::CallIfDisplayIsOn, weak_ptr_factory_.GetWeakPtr(),
          base::BindRepeating([]() {
            base::RecordAction(base::UserMetricsAction("ScreenLocker_Signout"));
            Shell::Get()->session_controller()->RequestSignOut();
          })),
      IDS_ASH_SHELF_SIGN_OUT_BUTTON, kShelfSignOutButtonIcon);
  kiosk_apps_button_ = new KioskAppsButton();
  kiosk_apps_button_->SetID(kApps);
  AddChildView(kiosk_apps_button_.get());
  add_button(kCloseNote,
             base::BindRepeating(
                 &TrayAction::CloseLockScreenNote,
                 base::Unretained(Shell::Get()->tray_action()),
                 mojom::CloseLockScreenNoteReason::kUnlockButtonPressed),
             IDS_ASH_SHELF_UNLOCK_BUTTON, kShelfUnlockButtonIcon);
  add_button(kCancel,
             base::BindRepeating(
                 [](LoginShelfView* shelf) {
                   // If the Cancel button has focus, clear it. Otherwise the
                   // shelf within active session may still be focused.
                   shelf->GetFocusManager()->ClearFocus();
                   Shell::Get()->login_screen_controller()->CancelAddUser();
                 },
                 this),
             IDS_ASH_SHELF_CANCEL_BUTTON, kShelfCancelButtonIcon);
  add_button(kBrowseAsGuest,
             base::BindRepeating(
                 &LoginScreenController::ShowGuestTosScreen,
                 base::Unretained(Shell::Get()->login_screen_controller())),
             IDS_ASH_BROWSE_AS_GUEST_BUTTON, kShelfBrowseAsGuestButtonIcon);
  add_button(kAddUser,
             base::BindRepeating(
                 &LoginScreenController::ShowGaiaSignin,
                 base::Unretained(Shell::Get()->login_screen_controller()),
                 EmptyAccountId()),
             IDS_ASH_ADD_USER_BUTTON, kShelfAddPersonButtonIcon);
  add_button(kParentAccess, base::BindRepeating([]() {
               // TODO(https://crbug.com/999387): Remove this when handling
               // touch cancellation is fixed for system modal windows.
               base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                   FROM_HERE, base::BindOnce([]() {
                     LockScreen::Get()->ShowParentAccessDialog();
                   }));
             }),
             IDS_ASH_PARENT_ACCESS_BUTTON, kPinRequestLockIcon);
  add_button(kEnterpriseEnrollment,
             base::BindRepeating(
                 &LoginScreenController::HandleAccelerator,
                 base::Unretained(Shell::Get()->login_screen_controller()),
                 ash::LoginAcceleratorAction::kStartEnrollment),
             IDS_ASH_ENTERPRISE_ENROLLMENT_BUTTON, kShelfEnterpriseIcon);
  add_button(kSignIn,
             base::BindRepeating(
                 &LoginScreenController::HandleAccelerator,
                 base::Unretained(Shell::Get()->login_screen_controller()),
                 ash::LoginAcceleratorAction::kCancelScreenAction),
             IDS_ASH_SHELF_SIGNIN_BUTTON, kShelfAddPersonButtonIcon);
  add_button(kOsInstall,
             base::BindRepeating(
                 &LoginScreenController::ShowOsInstallScreen,
                 base::Unretained(Shell::Get()->login_screen_controller())),
             IDS_ASH_SHELF_OS_INSTALL_BUTTON, kShelfOsInstallButtonIcon);

  // Adds observers for states that affect the visibility of different buttons.
  tray_action_observation_.Observe(Shell::Get()->tray_action());
  shutdown_controller_observation_.Observe(Shell::Get()->shutdown_controller());
  lock_screen_action_background_observation_.Observe(
      lock_screen_action_background);
  login_data_dispatcher_observation_.Observe(
      Shell::Get()->login_screen_controller()->data_dispatcher());
  enterprise_domain_model_observation_.Observe(
      Shell::Get()->system_tray_model()->enterprise_domain());
}

LoginShelfView::~LoginShelfView() {
  ShelfConfig::Get()->RemoveObserver(this);
}

void LoginShelfView::UpdateAfterSessionChange() {
  UpdateUi();
}

void LoginShelfView::AddedToWidget() {
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
    StatusAreaWidget* status_area_widget =
        Shelf::ForWindow(GetWidget()->GetNativeWindow())->GetStatusAreaWidget();
    // Focus goes to status area if it is visible.
    if (status_area_widget->IsVisible()) {
      status_area_widget->status_area_widget_delegate()
          ->set_default_last_focusable_child(reverse);
      Shell::Get()->focus_cycler()->FocusWidget(status_area_widget);
    } else {
      Shell::Get()->system_tray_notifier()->NotifyFocusOut(reverse);
    }
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

void LoginShelfView::Layout() {
  views::View::Layout();
  UpdateButtonUnionBounds();
}

void LoginShelfView::OnShelfConfigUpdated() {
  views::InstallRoundRectHighlightPathGenerator(
      kiosk_apps_button_, gfx::Insets(),
      ShelfConfig::Get()->control_border_radius());
}

bool LoginShelfView::LaunchAppForTesting(const std::string& app_id) {
  return kiosk_apps_button_->GetEnabled() &&
         kiosk_apps_button_->LaunchAppForTesting(app_id);
}

void LoginShelfView::InstallTestUiUpdateDelegate(
    std::unique_ptr<TestUiUpdateDelegate> delegate) {
  DCHECK(!test_ui_update_delegate_.get());
  test_ui_update_delegate_ = std::move(delegate);
}

void LoginShelfView::OnKioskMenuShown(
    const base::RepeatingClosure& on_kiosk_menu_shown) {
  if (kiosk_instruction_bubble_)
    kiosk_instruction_bubble_->GetWidget()->Hide();

  on_kiosk_menu_shown.Run();
}

void LoginShelfView::OnKioskMenuclosed() {
  if (kiosk_instruction_bubble_)
    kiosk_instruction_bubble_->GetWidget()->Show();
}

void LoginShelfView::SetKioskApps(
    const std::vector<KioskAppMenuEntry>& kiosk_apps) {
  kiosk_apps_button_->SetApps(kiosk_apps);
  UpdateUi();
  if (LockScreen::HasInstance()) {
    LockScreen::Get()->SetHasKioskApp(kiosk_apps_button_->HasApps());
  }
}

void LoginShelfView::ConfigureKioskCallbacks(
    const base::RepeatingCallback<void(const KioskAppMenuEntry&)>& launch_app,
    const base::RepeatingClosure& on_show_menu) {
  const auto show_kiosk_menu_callback =
      base::BindRepeating(&LoginShelfView::OnKioskMenuShown,
                          weak_ptr_factory_.GetWeakPtr(), on_show_menu);
  const auto close_kiosk_menu_callback = base::BindRepeating(
      &LoginShelfView::OnKioskMenuclosed, weak_ptr_factory_.GetWeakPtr());
  kiosk_apps_button_->ConfigureKioskCallbacks(
      launch_app, show_kiosk_menu_callback, close_kiosk_menu_callback);
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

void LoginShelfView::SetIsFirstSigninStep(bool is_first) {
  is_first_signin_step_ = is_first;
  UpdateUi();
}

void LoginShelfView::SetAddUserButtonEnabled(bool enable_add_user) {
  GetViewByID(kAddUser)->SetEnabled(enable_add_user);
}

void LoginShelfView::SetShutdownButtonEnabled(bool enable_shutdown_button) {
  GetViewByID(kShutdown)->SetEnabled(enable_shutdown_button);
}

void LoginShelfView::SetButtonEnabled(bool enabled) {
  // Only allow enabling shelf buttons when shelf is temporarily disabled and
  // only allow temporarily disabling shelf buttons when shelf is not already
  // disabled.
  if (enabled != is_shelf_temp_disabled_)
    return;
  is_shelf_temp_disabled_ = !enabled;

  for (const auto& button_id : kButtonIds) {
    GetViewByID(button_id)->SetEnabled(enabled);
  }

  StatusAreaWidget* status_area_widget =
      Shelf::ForWindow(GetWidget()->GetNativeWindow())->GetStatusAreaWidget();
  if (enabled) {
    for (TrayBackgroundView* tray_button : status_area_widget->tray_buttons()) {
      // Do not enable the button if it is already in disabled state before we
      // temporarily disable it.
      if (disabled_tray_buttons_.count(tray_button))
        continue;
      tray_button->SetEnabled(true);
    }
    disabled_tray_buttons_.clear();
  } else {
    for (TrayBackgroundView* tray_button : status_area_widget->tray_buttons()) {
      // Record the tray button if it is already in disabled state before we
      // temporarily disable it.
      if (!tray_button->GetEnabled())
        disabled_tray_buttons_.insert(tray_button);
      tray_button->SetEnabled(false);
    }
  }
}

void LoginShelfView::SetButtonOpacity(float target_opacity) {
  for (const auto& button_id : kButtonIds) {
    AnimateButtonOpacity(GetViewByID(button_id)->layer(), target_opacity,
                         ShelfConfig::Get()->DimAnimationDuration(),
                         ShelfConfig::Get()->DimAnimationTween());
  }
  AnimateButtonOpacity(kiosk_apps_button_->layer(), target_opacity,
                       ShelfConfig::Get()->DimAnimationDuration(),
                       ShelfConfig::Get()->DimAnimationTween());
}

void LoginShelfView::SetKioskLicenseModeForTesting(bool is_kiosk_license_mode) {
  kiosk_license_mode_ = is_kiosk_license_mode;
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

void LoginShelfView::OnDeviceEnterpriseInfoChanged() {
  // If feature is enabled, update the boolean kiosk_license_mode_. Otherwise,
  // it's false by default.
  if (features::IsKioskLoginScreenEnabled()) {
    kiosk_license_mode_ =
        Shell::Get()
            ->system_tray_model()
            ->enterprise_domain()
            ->management_device_mode() == ManagementDeviceMode::kKioskSku;
    UpdateUi();
  }
}

void LoginShelfView::OnEnterpriseAccountDomainChanged() {}

void LoginShelfView::HandleLocaleChange() {
  for (views::View* child : children()) {
    if (child->GetClassName() == kLoginShelfButtonClassName) {
      auto* button = static_cast<LoginShelfButton*>(child);
      button->SetText(l10n_util::GetStringUTF16(button->text_resource_id()));
      button->SetAccessibleName(button->GetText());
    }
  }
}

KioskAppInstructionBubble*
LoginShelfView::GetKioskInstructionBubbleForTesting() {
  return kiosk_instruction_bubble_;
}

ShelfShutdownConfirmationBubble*
LoginShelfView::GetShutdownConfirmationBubbleForTesting() {
  return test_shutdown_confirmation_bubble_;
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
  if (session_state == SessionState::ACTIVE ||
      session_state == SessionState::RMA) {
    // The entire view was set invisible. The buttons are also set invisible
    // to avoid affecting calculation of the shelf size.
    for (auto* child : children())
      child->SetVisible(false);

    return;
  }

  const gfx::Size old_preferred_size = GetPreferredSize();
  bool show_reboot = Shell::Get()->shutdown_controller()->reboot_on_shutdown();
  mojom::TrayActionState tray_action_state =
      Shell::Get()->tray_action()->GetLockScreenNoteState();
  bool is_locked = (session_state == SessionState::LOCKED);
  bool is_lock_screen_note_in_foreground =
      (tray_action_state == mojom::TrayActionState::kActive ||
       tray_action_state == mojom::TrayActionState::kLaunching) &&
      !LockScreenActionBackgroundAnimating();

  GetViewByID(kShutdown)->SetVisible(!show_reboot &&
                                     !is_lock_screen_note_in_foreground &&
                                     ShouldShowShutdownButton());
  GetViewByID(kRestart)->SetVisible(show_reboot &&
                                    !is_lock_screen_note_in_foreground &&
                                    ShouldShowShutdownButton());
  GetViewByID(kSignOut)->SetVisible(is_locked &&
                                    !is_lock_screen_note_in_foreground);
  GetViewByID(kCloseNote)
      ->SetVisible(is_locked && is_lock_screen_note_in_foreground);
  GetViewByID(kCancel)->SetVisible(session_state ==
                                   SessionState::LOGIN_SECONDARY);
  GetViewByID(kParentAccess)->SetVisible(is_locked && show_parent_access_);

  GetViewByID(kBrowseAsGuest)->SetVisible(ShouldShowGuestButton());
  GetViewByID(kEnterpriseEnrollment)
      ->SetVisible(ShouldShowEnterpriseEnrollmentButton());
  GetViewByID(kSignIn)->SetVisible(ShouldShowSignInButton());

  GetViewByID(kAddUser)->SetVisible(ShouldShowAddUserButton());
  kiosk_apps_button_->SetVisible(kiosk_apps_button_->HasApps() &&
                                 ShouldShowAppsButton());
  if (kiosk_license_mode_) {
    // Create the bubble once the login shelf view is available for anchoring.
    if (!kiosk_instruction_bubble_) {
      Shelf* shelf = Shelf::ForWindow(GetWidget()->GetNativeWindow());
      kiosk_instruction_bubble_ =
          new KioskAppInstructionBubble(GetViewByID(kApps), shelf->alignment());
    }
    if (kiosk_instruction_bubble_) {
      // Show kiosk instructions if the kiosk app button is visible and the menu
      // is not opened.
      if (kiosk_apps_button_->GetVisible() &&
          !kiosk_apps_button_->IsMenuOpened()) {
        kiosk_instruction_bubble_->GetWidget()->Show();
      } else {
        kiosk_instruction_bubble_->GetWidget()->Hide();
      }
    }
  }

  GetViewByID(kOsInstall)->SetVisible(ShouldShowOsInstallButton());

  // If there is no visible (and thus focusable) buttons, we shouldn't focus
  // LoginShelfView. We update it here, so we don't need to check visibility
  // every time we move focus to system tray.
  bool is_anything_focusable = false;
  for (auto* child : login_shelf_buttons_) {
    if (child->IsFocusable()) {
      is_anything_focusable = true;
      break;
    }
  }
  SetFocusBehavior(is_anything_focusable ? views::View::FocusBehavior::ALWAYS
                                         : views::View::FocusBehavior::NEVER);
  UpdateButtonsColors();

  // When the login shelf view is moved to its own widget, the login shelf
  // widget needs to change the size according to the login shelf view's
  // preferred size.
  if (old_preferred_size != GetPreferredSize() &&
      features::IsUseLoginShelfWidgetEnabled()) {
    PreferredSizeChanged();
  } else {
    Layout();
  }
}

void LoginShelfView::UpdateButtonsColors() {
  static_cast<LoginShelfButton*>(GetViewByID(kShutdown))->UpdateButtonColors();
  static_cast<LoginShelfButton*>(GetViewByID(kRestart))->UpdateButtonColors();
  static_cast<LoginShelfButton*>(GetViewByID(kSignOut))->UpdateButtonColors();
  static_cast<LoginShelfButton*>(GetViewByID(kCloseNote))->UpdateButtonColors();
  static_cast<LoginShelfButton*>(GetViewByID(kCancel))->UpdateButtonColors();
  static_cast<LoginShelfButton*>(GetViewByID(kParentAccess))
      ->UpdateButtonColors();
  static_cast<LoginShelfButton*>(GetViewByID(kBrowseAsGuest))
      ->UpdateButtonColors();
  static_cast<LoginShelfButton*>(GetViewByID(kAddUser))->UpdateButtonColors();
  static_cast<LoginShelfButton*>(GetViewByID(kEnterpriseEnrollment))
      ->UpdateButtonColors();
  static_cast<LoginShelfButton*>(GetViewByID(kSignIn))->UpdateButtonColors();
  static_cast<LoginShelfButton*>(GetViewByID(kOsInstall))->UpdateButtonColors();
  kiosk_apps_button_->UpdateButtonColors();
}

void LoginShelfView::UpdateButtonUnionBounds() {
  button_union_bounds_ = gfx::Rect();
  View::Views children = GetChildrenInZOrder();
  for (auto* child : children) {
    if (child->GetVisible())
      button_union_bounds_.Union(child->bounds());
  }
}

bool LoginShelfView::ShouldShowGuestAndAppsButtons() const {
  bool dialog_state_allowed = false;
  if (dialog_state_ == OobeDialogState::USER_CREATION ||
      dialog_state_ == OobeDialogState::GAIA_SIGNIN ||
      dialog_state_ == OobeDialogState::GAIA_INFO) {
    dialog_state_allowed = !login_screen_has_users_ && is_first_signin_step_;
  } else if (dialog_state_ == OobeDialogState::ERROR ||
             dialog_state_ == OobeDialogState::HIDDEN ||
             dialog_state_ == OobeDialogState::EXTENSION_LOGIN_CLOSED) {
    dialog_state_allowed = true;
  }

  const bool user_session_started =
      Shell::Get()->session_controller()->NumberOfLoggedInUsers() != 0;

  return dialog_state_allowed && !user_session_started;
}

// Show Shutdown button only in one of the cases:
//  1. On general login screen, when OOBE is completed and device is owned;
//  2. On enrollment success step (admins/resellers may use the on screen button
//     to shut down the device after enrollment);
//  3. On first screen of gaia login flow (same reason as 2).
bool LoginShelfView::ShouldShowShutdownButton() const {
  return dialog_state_ == OobeDialogState::HIDDEN ||
         dialog_state_ == OobeDialogState::EXTENSION_LOGIN_CLOSED ||
         dialog_state_ == OobeDialogState::ENROLLMENT_SUCCESS ||
         dialog_state_ == OobeDialogState::EXTENSION_LOGIN ||
         dialog_state_ == OobeDialogState::BLOCKING ||
         (dialog_state_ == OobeDialogState::GAIA_SIGNIN &&
          is_first_signin_step_);
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
// 7. The device is not in kiosk license mode.
bool LoginShelfView::ShouldShowGuestButton() const {
  if (!allow_guest_)
    return false;

  if (scoped_guest_button_blockers_ > 0)
    return false;

  if (!ShouldShowGuestAndAppsButtons())
    return false;

  const SessionState session_state =
      Shell::Get()->session_controller()->GetSessionState();

  if (session_state == SessionState::OOBE)
    return is_first_signin_step_;

  if (session_state != SessionState::LOGIN_PRIMARY)
    return false;

  return true;
}

bool LoginShelfView::ShouldShowEnterpriseEnrollmentButton() const {
  const SessionState session_state =
      Shell::Get()->session_controller()->GetSessionState();
  return session_state == SessionState::OOBE &&
         dialog_state_ == OobeDialogState::USER_CREATION;
}

bool LoginShelfView::ShouldShowSignInButton() const {
  const SessionState session_state =
      Shell::Get()->session_controller()->GetSessionState();
  return session_state == SessionState::OOBE &&
         dialog_state_ == OobeDialogState::ENROLLMENT_CANCEL_ENABLED;
}

// Show add user button if:
// 1. We are on the login screen.
// 2. The device is not in kiosk license mode.
// 3. The OOBE dialog or login extension UI is closed.
bool LoginShelfView::ShouldShowAddUserButton() const {
  const SessionState session_state =
      Shell::Get()->session_controller()->GetSessionState();

  if (session_state != SessionState::LOGIN_PRIMARY)
    return false;

  if (kiosk_license_mode_)
    return false;

  if (dialog_state_ != OobeDialogState::HIDDEN &&
      dialog_state_ != OobeDialogState::EXTENSION_LOGIN_CLOSED)
    return false;

  return true;
}

bool LoginShelfView::ShouldShowAppsButton() const {
  if (!ShouldShowGuestAndAppsButtons())
    return false;

  const SessionState session_state =
      Shell::Get()->session_controller()->GetSessionState();
  if (session_state != SessionState::LOGIN_PRIMARY)
    return false;

  return true;
}

bool LoginShelfView::ShouldShowOsInstallButton() const {
  if (!switches::IsOsInstallAllowed())
    return false;

  if (!ShouldShowGuestAndAppsButtons())
    return false;

  const SessionState session_state =
      Shell::Get()->session_controller()->GetSessionState();

  if (session_state == SessionState::OOBE)
    return is_first_signin_step_;

  if (session_state != SessionState::LOGIN_PRIMARY)
    return false;

  return true;
}

}  // namespace ash
