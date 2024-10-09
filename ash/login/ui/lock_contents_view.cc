// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/login/ui/lock_contents_view.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/detachable_base/detachable_base_pairing_status.h"
#include "ash/focus_cycler.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/auth_error_bubble.h"
#include "ash/login/ui/bottom_status_indicator.h"
#include "ash/login/ui/kiosk_app_default_message.h"
#include "ash/login/ui/lock_contents_view_constants.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/lock_screen_media_view.h"
#include "ash/login/ui/login_auth_user_view.h"
#include "ash/login/ui/login_big_user_view.h"
#include "ash/login/ui/login_camera_timeout_view.h"
#include "ash/login/ui/login_detachable_base_model.h"
#include "ash/login/ui/login_expanded_public_account_view.h"
#include "ash/login/ui/login_public_account_user_view.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/note_action_launch_button.h"
#include "ash/login/ui/scrollable_users_list_view.h"
#include "ash/login/ui/views_utils.h"
#include "ash/media/media_controller_impl.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/child_accounts/parent_access_controller.h"
#include "ash/public/cpp/login_accelerators.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/management_disclosure_client.h"
#include "ash/public/cpp/reauth_reason.h"
#include "ash/public/cpp/smartlock_state.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/login_shelf_view.h"
#include "ash/shelf/login_shelf_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/power/power_button_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/login/auth/auth_events_recorder.h"
#include "chromeos/ash/components/proximity_auth/public/mojom/auth_type.mojom.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/multi_user/multi_user_sign_in_policy.h"
#include "components/user_manager/user_type.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/base/user_activity/user_activity_observer.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Sets the preferred width for |view| with an arbitrary height.
void SetPreferredWidthForView(views::View* view, int width) {
  view->SetPreferredSize(gfx::Size(width, kNonEmptyHeightDp));
}

// Focuses the first or last focusable child of |root|. If |reverse| is false,
// this focuses the first focusable child. If |reverse| is true, this focuses
// the last focusable child.
void FocusFirstOrLastFocusableChild(views::View* root, bool reverse) {
  views::FocusSearch search(root, reverse /*cycle*/,
                            false /*accessibility_mode*/);
  views::FocusTraversable* dummy_focus_traversable;
  views::View* dummy_focus_traversable_view;
  views::View* focusable_view = search.FindNextFocusableView(
      root,
      reverse ? views::FocusSearch::SearchDirection::kBackwards
              : views::FocusSearch::SearchDirection::kForwards,
      views::FocusSearch::TraversalDirection::kDown,
      views::FocusSearch::StartingViewPolicy::kSkipStartingView,
      views::FocusSearch::AnchoredDialogPolicy::kCanGoIntoAnchoredDialog,
      &dummy_focus_traversable, &dummy_focus_traversable_view);
  if (focusable_view) {
    focusable_view->AboutToRequestFocusFromTabTraversal(reverse);
    focusable_view->RequestFocus();
  }
}

keyboard::KeyboardUIController* GetKeyboardControllerForWidget(
    const views::Widget* widget) {
  auto* keyboard_ui_controller = keyboard::KeyboardUIController::Get();
  if (!keyboard_ui_controller->IsEnabled()) {
    return nullptr;
  }

  aura::Window* keyboard_window = keyboard_ui_controller->GetRootWindow();
  aura::Window* this_window = widget->GetNativeWindow()->GetRootWindow();
  return keyboard_window == this_window ? keyboard_ui_controller : nullptr;
}

bool IsPublicAccountUser(const LoginUserInfo& user) {
  return user.basic_user_info.type == user_manager::UserType::kPublicAccount;
}

bool IsTimeInFuture(cryptohome::PinLockAvailability available_time) {
  return available_time.has_value() &&
         available_time.value() > base::Time::Now() &&
         available_time.value() < base::Time::Max();
}

//
// Computes a layout described as follows:
//
//    l L R r
//
// L R go from [0, L/R_max_fixed_width]
// l and r go from [0, inf]
//
// First, width is distributed to L and R up to their maximum widths. If there
// is not enough width for them, space will be distributed evenly in the same
// ratio as their original sizes.
//
// If L and R are at max width, l and r will receive all remaining space in the
// specified relative weighting.
//
// l -> left_flex_weight
// L -> left_max_fixed_width
// R -> right_max_fixed_width
// r -> right_flex_weight
//
// Output data is in the member variables.
//
struct MediumViewLayout {
  MediumViewLayout(int width,
                   int left_flex_weight,
                   int left_max_fixed_width,
                   int right_max_fixed_width,
                   int right_flex_weight) {
    // No space to distribute.
    if (width <= 0) {
      return;
    }

    auto set_values_from_weight = [](int width, float weight_a, float weight_b,
                                     int* value_a, int* value_b) {
      float total_weight = weight_a + weight_b;
      *value_a = width * (weight_a / total_weight);
      // Subtract to avoid floating point rounding errors, ie, guarantee that
      // that |value_a + value_b = width|.
      *value_b = width - *value_a;
    };

    int flex_width = width - (left_max_fixed_width + right_max_fixed_width);
    if (flex_width < 0) {
      // No flex available, distribute to fixed width only
      set_values_from_weight(width, left_max_fixed_width, right_max_fixed_width,
                             &left_fixed_width, &right_fixed_width);
      DCHECK_EQ(width, left_fixed_width + right_fixed_width);
    } else {
      // Flex is available; fixed goes to maximum size, extra goes to flex.
      left_fixed_width = left_max_fixed_width;
      right_fixed_width = right_max_fixed_width;
      set_values_from_weight(flex_width, left_flex_weight, right_flex_weight,
                             &left_flex_width, &right_flex_width);
      DCHECK_EQ(flex_width, left_flex_width + right_flex_width);
    }
  }

  int left_fixed_width = 0;
  int right_fixed_width = 0;
  int left_flex_width = 0;
  int right_flex_width = 0;
};

class UserAddingScreenIndicator : public views::View {
  METADATA_HEADER(UserAddingScreenIndicator, views::View)

 public:
  UserAddingScreenIndicator() {
    views::BoxLayout* layout_manager =
        SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal,
            gfx::Insets(kBubblePaddingDp), kBubbleBetweenChildSpacingDp));
    layout_manager->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStart);

    info_icon_ = AddChildView(std::make_unique<views::ImageView>());
    info_icon_->SetPreferredSize(gfx::Size(kInfoIconSizeDp, kInfoIconSizeDp));
    info_icon_->SetImage(ui::ImageModel::FromVectorIcon(
        views::kInfoIcon, kColorAshIconColorPrimary));

    std::u16string message =
        l10n_util::GetStringUTF16(IDS_ASH_LOGIN_USER_ADDING_BANNER);
    label_ =
        AddChildView(login_views_utils::CreateThemedBubbleLabel(message, this));
    label_->SetText(message);

    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
    layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);

    SetBackground(views::CreateThemedRoundedRectBackground(
        kColorAshShieldAndBase80, kBubbleBorderRadius));
  }

  UserAddingScreenIndicator(const UserAddingScreenIndicator&) = delete;
  UserAddingScreenIndicator& operator=(const UserAddingScreenIndicator&) =
      delete;
  ~UserAddingScreenIndicator() override = default;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(kUserAddingScreenIndicatorWidth,
                     GetLayoutManager()->GetPreferredHeightForWidth(
                         this, kUserAddingScreenIndicatorWidth));
  }

 private:
  raw_ptr<views::ImageView> info_icon_ = nullptr;
  raw_ptr<views::Label> label_ = nullptr;
};

BEGIN_METADATA(UserAddingScreenIndicator)
END_METADATA

}  // namespace

// static
const int LockContentsView::kLoginAttemptsBeforeGaiaDialog = 4;

LockContentsView::LockContentsView(
    mojom::TrayActionState initial_note_action_state,
    LockScreen::ScreenType screen_type,
    LoginDataDispatcher* data_dispatcher,
    std::unique_ptr<LoginDetachableBaseModel> detachable_base_model)
    : screen_type_(screen_type),
      data_dispatcher_(data_dispatcher),
      detachable_base_model_(std::move(detachable_base_model)) {
  data_dispatcher_->AddObserver(this);
  Shell::Get()->system_tray_notifier()->AddSystemTrayObserver(this);
  keyboard::KeyboardUIController::Get()->AddObserver(this);
  enterprise_domain_model_observation_.Observe(
      Shell::Get()->system_tray_model()->enterprise_domain());

  // We reuse the focusable state on this view as a signal that focus should
  // switch to the system tray. LockContentsView should otherwise not be
  // focusable.
  SetFocusBehavior(FocusBehavior::ALWAYS);
  set_suppress_default_focus_handling();

  SetLayoutManager(std::make_unique<views::FillLayout>());

  main_view_ = AddChildView(std::make_unique<NonAccessibleView>());

  // The top header view.
  top_header_ = AddChildView(std::make_unique<views::View>());
  auto top_header_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal);
  top_header_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  top_header_->SetLayoutManager(std::move(top_header_layout));

  system_info_ = top_header_->AddChildView(std::make_unique<views::View>());
  auto* system_info_layout =
      system_info_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets::VH(6, 8)));
  system_info_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kEnd);
  system_info_->SetVisible(false);

  // The bottom status indicator view.
  bottom_status_indicator_ =
      AddChildView(std::make_unique<BottomStatusIndicator>(
          base::BindRepeating(&LockContentsView::OnBottomStatusIndicatorTapped,
                              weak_ptr_factory_.GetWeakPtr())));
  auto bottom_status_indicator_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kBottomStatusIndicatorChildSpacingDp);
  bottom_status_indicator_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);
  bottom_status_indicator_->SetLayoutManager(
      std::move(bottom_status_indicator_layout));

  std::string enterprise_domain_manager = Shell::Get()
                                              ->system_tray_model()
                                              ->enterprise_domain()
                                              ->enterprise_domain_manager();
  if (!enterprise_domain_manager.empty()) {
    ShowEnterpriseDomainManager(enterprise_domain_manager);
  }

  note_action_ = top_header_->AddChildView(
      std::make_unique<NoteActionLaunchButton>(initial_note_action_state));

  // Public Session expanded view.
  expanded_view_ =
      AddChildView(std::make_unique<LoginExpandedPublicAccountView>(
          base::BindRepeating(&LockContentsView::SetDisplayStyle,
                              base::Unretained(this), DisplayStyle::kAll)));
  expanded_view_->SetVisible(false);

  detachable_base_error_bubble_ =
      AddChildView(std::make_unique<LoginErrorBubble>());
  detachable_base_error_bubble_->set_persistent(true);

  management_bubble_ = AddChildView(std::make_unique<ManagementBubble>(
      l10n_util::GetStringFUTF16(IDS_ASH_LOGIN_ENTERPRISE_MANAGED_POP_UP,
                                 ui::GetChromeOSDeviceName(),
                                 base::UTF8ToUTF16(enterprise_domain_manager)),
      bottom_status_indicator_ != nullptr
          ? bottom_status_indicator_->AsWeakPtr()
          : nullptr));

  warning_banner_bubble_ = AddChildView(std::make_unique<LoginErrorBubble>());
  warning_banner_bubble_->set_persistent(true);

  auth_error_bubble_ = AddChildView(std::make_unique<AuthErrorBubble>(
      base::BindRepeating(&LockContentsView::LearnMoreButtonPressed,
                          base::Unretained(this)),
      base::BindRepeating(&LockContentsView::RecoverUserButtonPressed,
                          base::Unretained(this))));

  if (Shell::Get()->session_controller()->GetSessionState() ==
      session_manager::SessionState::LOGIN_SECONDARY) {
    user_adding_screen_indicator_ =
        AddChildView(std::make_unique<UserAddingScreenIndicator>());
  }
  OnLockScreenNoteStateChanged(initial_note_action_state);
  chromeos::PowerManagerClient::Get()->AddObserver(this);
  RegisterAccelerators();

  // If feature is enabled, update the boolean kiosk_license_mode_. Otherwise,
  // it's false by default.
  if (features::IsKioskLoginScreenEnabled()) {
    kiosk_license_mode_ =
        Shell::Get()
            ->system_tray_model()
            ->enterprise_domain()
            ->management_device_mode() == ManagementDeviceMode::kKioskSku;
  }

  GetViewAccessibility().SetRole(ax::mojom::Role::kWindow);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(screen_type_ == LockScreen::ScreenType::kLogin
                                    ? IDS_ASH_LOGIN_SCREEN_ACCESSIBLE_NAME
                                    : IDS_ASH_LOCK_SCREEN_ACCESSIBLE_NAME));
}

LockContentsView::~LockContentsView() {
  Shell::Get()->accelerator_controller()->UnregisterAll(this);
  data_dispatcher_->RemoveObserver(this);
  keyboard::KeyboardUIController::Get()->RemoveObserver(this);
  Shell::Get()->system_tray_notifier()->RemoveSystemTrayObserver(this);

  // Times a password was incorrectly entered until view is destroyed.
  for (auto& unlock_attempt : unlock_attempt_by_user_) {
    // If the user recorded a successful attempt already, the number of attempts
    // was reset to 0. Therefore, we will not record it the second time
    // here.
    if (unlock_attempt.second > 0) {
      RecordAndResetPasswordAttempts(
          AuthEventsRecorder::AuthenticationOutcome::kFailure,
          unlock_attempt.first);
    }
  }

  chromeos::PowerManagerClient::Get()->RemoveObserver(this);

  if (widget_) {
    views::FocusManager* focus_manager = widget_->GetFocusManager();
    if (focus_manager) {
      focus_manager->RemoveFocusChangeListener(this);
    }
  }
}

void LockContentsView::FocusNextUser() {
  if (users_.empty()) {
    return;
  }

  if (login_views_utils::HasFocusInAnyChildView(primary_big_view_)) {
    if (opt_secondary_big_view_) {
      SwapActiveAuthBetweenPrimaryAndSecondary(false /*is_primary*/);
      opt_secondary_big_view_->RequestFocus();
    } else if (users_list_) {
      users_list_->user_view_at(0)->RequestFocus();
    }
    return;
  }

  if (opt_secondary_big_view_ &&
      login_views_utils::HasFocusInAnyChildView(opt_secondary_big_view_)) {
    SwapActiveAuthBetweenPrimaryAndSecondary(true /*is_primary*/);
    primary_big_view_->RequestFocus();
    return;
  }

  if (users_list_) {
    for (int i = 0; i < users_list_->user_count(); ++i) {
      LoginUserView* user_view = users_list_->user_view_at(i);
      if (!login_views_utils::HasFocusInAnyChildView(user_view)) {
        continue;
      }

      if (i == users_list_->user_count() - 1) {
        SwapActiveAuthBetweenPrimaryAndSecondary(true /*is_primary*/);
        primary_big_view_->RequestFocus();
        return;
      }

      user_view->GetNextFocusableView()->RequestFocus();
      return;
    }
  }
}

void LockContentsView::FocusPreviousUser() {
  if (users_.empty()) {
    return;
  }

  if (login_views_utils::HasFocusInAnyChildView(primary_big_view_)) {
    if (users_list_) {
      users_list_->user_view_at(users_list_->user_count() - 1)->RequestFocus();
    } else if (opt_secondary_big_view_) {
      SwapActiveAuthBetweenPrimaryAndSecondary(false /*is_primary*/);
      opt_secondary_big_view_->RequestFocus();
    }
    return;
  }

  if (opt_secondary_big_view_ &&
      login_views_utils::HasFocusInAnyChildView(opt_secondary_big_view_)) {
    SwapActiveAuthBetweenPrimaryAndSecondary(true /*is_primary*/);
    primary_big_view_->RequestFocus();
    return;
  }

  if (users_list_) {
    for (int i = 0; i < users_list_->user_count(); ++i) {
      LoginUserView* user_view = users_list_->user_view_at(i);
      if (!login_views_utils::HasFocusInAnyChildView(user_view)) {
        continue;
      }

      if (i == 0) {
        SwapActiveAuthBetweenPrimaryAndSecondary(true /*is_primary*/);
        primary_big_view_->RequestFocus();
        return;
      }

      user_view->GetPreviousFocusableView()->RequestFocus();
      return;
    }
  }
}

void LockContentsView::ShowEnterpriseDomainManager(
    const std::string& entreprise_domain_manager) {
  bottom_status_indicator_->SetText(l10n_util::GetStringFUTF16(
      IDS_ASH_LOGIN_MANAGED_DEVICE_INDICATOR, ui::GetChromeOSDeviceName(),
      base::UTF8ToUTF16(entreprise_domain_manager)));
  bottom_status_indicator_->GetViewAccessibility().SetRole(
      ax::mojom::Role::kButton);
  bottom_status_indicator_state_ = BottomIndicatorState::kManagedDevice;
  UpdateBottomStatusIndicatorColors();
  UpdateBottomStatusIndicatorVisibility();
}

void LockContentsView::ShowAdbEnabled() {
  bottom_status_indicator_->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_SCREEN_UNVERIFIED_CODE_WARNING));
  bottom_status_indicator_->GetViewAccessibility().SetRole(
      ax::mojom::Role::kStaticText);
  bottom_status_indicator_state_ = BottomIndicatorState::kAdbSideLoadingEnabled;
  UpdateBottomStatusIndicatorColors();
  UpdateBottomStatusIndicatorVisibility();
}

void LockContentsView::ToggleSystemInfo() {
  enable_system_info_if_possible_ = !enable_system_info_if_possible_;
  // Whether the system information should be displayed or not might be
  // enforced according to policy settings.
  bool system_info_visibility = GetSystemInfoVisibility();
  if (system_info_visibility != system_info_->GetVisible()) {
    system_info_->SetVisible(system_info_visibility);
    LayoutTopHeader();
    LayoutBottomStatusIndicator();
  }
}

void LockContentsView::ShowParentAccessDialog() {
  // ParentAccessDialog should only be shown on lock screen from here.
  DCHECK(primary_big_view_);
  const AccountId account_id =
      CurrentBigUserView()->GetCurrentUser().basic_user_info.account_id;

  Shell::Get()->parent_access_controller()->ShowWidget(
      account_id,
      base::BindOnce(&LockContentsView::OnParentAccessValidationFinished,
                     weak_ptr_factory_.GetWeakPtr(), account_id),
      SupervisedAction::kUnlockTimeLimits, false, base::Time::Now());
  Shell::Get()->login_screen_controller()->ShowParentAccessButton(false);
}

void LockContentsView::ShowManagementDisclosureDialog() {
  if (management_disclosure_dialog_) {
    // Do not create another dialog if one already exists.
    return;
  }

  auto* dialog = new ManagementDisclosureDialog(
      Shell::Get()->login_screen_controller()
                  ->GetManagementDisclosureClient() != nullptr
          ? Shell::Get()
                ->login_screen_controller()
                ->GetManagementDisclosureClient()
                ->GetDisclosures()
          : std::vector<std::u16string>(),
      base::BindOnce(
          [](base::WeakPtr<ManagementDisclosureDialog> dialog) {
            dialog.reset();
          },
          management_disclosure_dialog_));
  // Save the dialog so it doesn't go out of scope before it is
  // used and closed.
  management_disclosure_dialog_ = dialog->GetWeakPtr();
}

void LockContentsView::SetHasKioskApp(bool has_kiosk_apps) {
  has_kiosk_apps_ = has_kiosk_apps;

  UpdateKioskDefaultMessageVisibility();
}

void LockContentsView::Layout(PassKey) {
  LayoutSuperclass<View>(this);
  LayoutTopHeader();
  LayoutBottomStatusIndicator();
  LayoutUserAddingScreenIndicator();
  LayoutPublicSessionView();

  if (users_list_) {
    users_list_->DeprecatedLayoutImmediately();
  }
}

void LockContentsView::AddedToWidget() {
  DoLayout();

  views::Widget* widget = GetWidget();
  CHECK(widget);
  widget_ = widget->GetWeakPtr();

  views::FocusManager* focus_manager = widget->GetFocusManager();
  if (focus_manager) {
    focus_manager->AddFocusChangeListener(this);
  } else {
    LOG(ERROR) << "LockContentsView attached to Widget without FocusManager";
  }

  // Focus the primary user when showing the UI. This will focus the password.
  if (primary_big_view_) {
    primary_big_view_->RequestFocus();
  }
}

void LockContentsView::RemovedFromWidget() {
  if (!widget_) {
    return;
  }
  views::FocusManager* focus_manager = widget_->GetFocusManager();
  if (focus_manager) {
    focus_manager->RemoveFocusChangeListener(this);
  }
  widget_ = nullptr;
}

void LockContentsView::OnFocus() {
  // If LockContentsView somehow gains focus (ie, a test, but it should not
  // under typical circumstances), immediately forward the focus to the
  // primary_big_view_ since LockContentsView has no real focusable content by
  // itself.
  if (primary_big_view_) {
    primary_big_view_->RequestFocus();
  }
}

void LockContentsView::AboutToRequestFocusFromTabTraversal(bool reverse) {
  // The LockContentsView itself doesn't have anything to focus. If it gets
  // focused we should change the currently focused widget (ie, to the shelf or
  // status area, or lock screen apps, if they are active).
  if (reverse && lock_screen_apps_active_) {
    Shell::Get()->login_screen_controller()->FocusLockScreenApps(reverse);
    return;
  }

  FocusNextWidget(reverse);
}

void LockContentsView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  Shelf* shelf = Shelf::ForWindow(GetWidget()->GetNativeWindow());
  ShelfWidget* shelf_widget = shelf->shelf_widget();
  GetViewAccessibility().SetNextFocus(shelf_widget);
  GetViewAccessibility().SetPreviousFocus(shelf->GetStatusAreaWidget());
}

bool LockContentsView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  auto entry = accel_map_.find(accelerator);
  if (entry == accel_map_.end()) {
    return false;
  }

  PerformAction(entry->second);
  return true;
}

void LockContentsView::OnUsersChanged(const std::vector<LoginUserInfo>& users) {
  if (Shell::Get()->login_screen_controller()->IsAuthenticating()) {
    // TODO(b/276246832): We should avoid re-layouting during Authentication.
    LOG(WARNING) << "LockContentsView::OnUsersChanged called during "
                    "Authentication. We are postponing the re-layout.";
    pending_users_change_ = users;
    return;
  }
  ApplyUserChanges(users);
}

void LockContentsView::ApplyUserChanges(
    const std::vector<LoginUserInfo>& users) {
  CHECK(!Shell::Get()->login_screen_controller()->IsAuthenticating() ||
        Shell::Get()
            ->login_screen_controller()
            ->IsAuthenticationCallbackExecuting());
  AuthEventsRecorder::Get()->OnLockContentsViewUpdate();
  // The debug view will potentially call this method many times. Make sure to
  // invalidate any child references.
  primary_big_view_ = nullptr;
  opt_secondary_big_view_ = nullptr;
  users_list_ = nullptr;
  middle_spacing_view_ = nullptr;
  media_view_ = nullptr;
  layout_actions_.clear();
  pending_users_change_.reset();
  // Removing child views can change focus, which may result in LockContentsView
  // getting focused. Make sure to clear internal references before that happens
  // so there is not stale-pointer usage. See crbug.com/884402.
  // TODO(crbug.com/1222096): We should figure out a better way of handling
  // user info changes such as avatar changes. They should not cause view
  // re-layouting.
  main_view_->RemoveAllChildViews();

  // Build user state list. Preserve previous state if the user already exists.
  std::vector<UserState> new_users;
  for (const LoginUserInfo& user : users) {
    UserState* old_state = FindStateForUser(user.basic_user_info.account_id);
    if (old_state) {
      new_users.push_back(std::move(*old_state));
    } else {
      new_users.emplace_back(user);
    }
  }

  users_ = std::move(new_users);

  // Allocate layout which is common between all densities.
  auto* main_layout =
      main_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  main_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  main_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  if (kiosk_license_mode_) {
    return;
  }

  // If there are no users, show GAIA signin if login.
  if (users.empty() && screen_type_ == LockScreen::ScreenType::kLogin) {
    // Create a UI that will be shown on camera usage timeout after the
    // third-party SAML dialog has been dismissed. For more info check
    // discussion under privacy review in FLB crbug.com/1221337.
    login_camera_timeout_view_ =
        main_view_->AddChildView(std::make_unique<LoginCameraTimeoutView>(
            base::BindRepeating(&LockContentsView::OnBackToSigninButtonTapped,
                                weak_ptr_factory_.GetWeakPtr())));
    // TODO(b/333882432): Remove this log after the bug fixed.
    LOG(WARNING) << " b/333882432: LockContentsView::ApplyUserChanges";
    Shell::Get()->login_screen_controller()->ShowGaiaSignin(
        /*prefilled_account=*/EmptyAccountId());
    return;
  }

  if (!users.empty()) {
    auto primary_big_view =
        AllocateLoginBigUserView(users[0], true /*is_primary*/);

    // Build layout for additional users.
    if (users.size() <= 2) {
      CreateLowDensityLayout(users, std::move(primary_big_view));
    } else if (users.size() >= 3 && users.size() <= 6) {
      CreateMediumDensityLayout(users, std::move(primary_big_view));
    } else if (users.size() >= 7) {
      CreateHighDensityLayout(users, main_layout, std::move(primary_big_view));
    }

    // |primary_big_view_| must have been set by one of the above functions.
    DCHECK(primary_big_view_);

    LayoutAuth(primary_big_view_, opt_secondary_big_view_, false /*animate*/);

    // Big user may be the same if we already built lock screen.
    OnBigUserChanged();
  }

  // Force layout.
  PreferredSizeChanged();
  DeprecatedLayoutImmediately();

  // If one of the child views had focus before we deleted them, then this view
  // will get focused. Move focus back to the primary big view.
  if (primary_big_view_ && HasFocus()) {
    primary_big_view_->RequestFocus();
  }
}

void LockContentsView::OnUserAvatarChanged(const AccountId& account_id,
                                           const UserAvatar& avatar) {
  auto replace = [&avatar](const LoginUserInfo& user) {
    auto changed = user;
    changed.basic_user_info.avatar = avatar;
    return changed;
  };

  LoginBigUserView* big =
      TryToFindBigUser(account_id, false /*require_auth_active*/);
  if (big) {
    big->UpdateForUser(replace(big->GetCurrentUser()));
    return;
  }

  LoginUserView* user =
      users_list_ ? users_list_->GetUserView(account_id) : nullptr;
  if (user) {
    user->UpdateForUser(replace(user->current_user()), false /*animate*/);
    return;
  }
}

void LockContentsView::OnUserAuthFactorsChanged(
    const AccountId& user,
    cryptohome::AuthFactorsSet auth_factors,
    cryptohome::PinLockAvailability pin_available_at) {
  UserState* state = FindStateForUser(user);
  if (!state) {
    LOG(ERROR) << "Unable to find user when updating auth factors";
    return;
  }

  const bool enable_password =
      auth_factors.Has(cryptohome::AuthFactorType::kPassword);
  const bool enable_pin = auth_factors.Has(cryptohome::AuthFactorType::kPin);
  const bool enable_smart_card =
      auth_factors.Has(cryptohome::AuthFactorType::kSmartCard);

  // If PIN is enabled, or PIN is disabled and permanently locked, reset
  // the `pin_available_at` as it's meaningless.
  if (enable_pin || !IsTimeInFuture(pin_available_at)) {
    pin_available_at = std::nullopt;
  }

  if (!enable_password && !enable_pin && !enable_smart_card &&
      !pin_available_at.has_value()) {
    LOG(ERROR) << "Unable to update auth factors, neither password, PIN or "
                  "smart card auth is configured";
    return;
  }

  if (state->show_password == enable_password &&
      state->show_pin == enable_pin &&
      state->show_challenge_response_auth == enable_smart_card &&
      state->pin_available_at == pin_available_at) {
    LOG(WARNING)
        << "Unexpected call to OnUserAuthFactorsChanged; state unchanged.";
    return;
  }

  state->show_password = enable_password;
  state->show_pin = enable_pin;
  state->autosubmit_pin_length =
      user_manager::KnownUser(Shell::Get()->local_state())
          .GetUserPinLength(user);
  state->pin_available_at = pin_available_at;
  state->show_challenge_response_auth = enable_smart_card;

  LoginBigUserView* big_user =
      TryToFindBigUser(user, true /*require_auth_active*/);
  if (big_user && big_user->auth_user()) {
    LayoutAuth(big_user, nullptr /*opt_to_hide*/, true /*animate*/);
  }
}

void LockContentsView::OnPinEnabledForUserChanged(
    const AccountId& user,
    bool enabled,
    cryptohome::PinLockAvailability available_at) {
  UserState* state = FindStateForUser(user);
  if (!state) {
    LOG(ERROR) << "Unable to find user when changing PIN state to " << enabled;
    return;
  }

  // If PIN is enabled, or PIN is disabled and permanently locked, reset
  // the `pin_available_at` as it's meaningless.
  if (enabled || !IsTimeInFuture(available_at)) {
    available_at = std::nullopt;
  }

  if (state->show_pin == enabled && state->pin_available_at == available_at) {
    LOG(WARNING)
        << "Unexpected call to OnPinEnabledForUserChanged; state unchanged.";
    return;
  }

  state->show_pin = enabled;
  state->autosubmit_pin_length =
      user_manager::KnownUser(Shell::Get()->local_state())
          .GetUserPinLength(user);
  state->pin_available_at = available_at;

  LoginBigUserView* big_user =
      TryToFindBigUser(user, true /*require_auth_active*/);
  if (big_user && big_user->auth_user()) {
    LayoutAuth(big_user, nullptr /*opt_to_hide*/, true /*animate*/);
  }
}

void LockContentsView::OnChallengeResponseAuthEnabledForUserChanged(
    const AccountId& user,
    bool enabled) {
  UserState* state = FindStateForUser(user);
  if (!state) {
    LOG(ERROR)
        << "Unable to find user when changing challenge-response auth state to "
        << enabled;
    return;
  }

  state->show_challenge_response_auth = enabled;

  LoginBigUserView* big_user =
      TryToFindBigUser(user, /*require_auth_active=*/true);
  if (big_user && big_user->auth_user()) {
    LayoutAuth(big_user, /*opt_to_hide=*/nullptr, /*animate=*/true);
  }
}

void LockContentsView::OnFingerprintStateChanged(const AccountId& account_id,
                                                 FingerprintState state) {
  UserState* user_state = FindStateForUser(account_id);
  if (!user_state) {
    LOG(ERROR) << "Unable to find user when changing fingerprint state.";
    return;
  }
  if (user_state->fingerprint_state == state) {
    LOG(WARNING)
        << "Unexpected call to OnFingerprintStateChanged; state unchanged.";
    return;
  }

  user_state->fingerprint_state = state;
  LoginBigUserView* big_view =
      TryToFindBigUser(account_id, true /*require_auth_active*/);
  if (!big_view || !big_view->auth_user()) {
    return;
  }

  big_view->auth_user()->SetFingerprintState(user_state->fingerprint_state);
  LayoutAuth(big_view, nullptr /*opt_to_hide*/, true /*animate*/);
}

void LockContentsView::OnResetFingerprintUIState(const AccountId& account_id) {
  UserState* user_state = FindStateForUser(account_id);
  if (!user_state) {
    return;
  }

  LoginBigUserView* big_view =
      TryToFindBigUser(account_id, true /*require_auth_active*/);
  if (!big_view || !big_view->auth_user()) {
    return;
  }

  big_view->auth_user()->ResetFingerprintUIState();
  LayoutAuth(big_view, nullptr /*opt_to_hide*/, true /*animate*/);
}

void LockContentsView::OnFingerprintAuthResult(const AccountId& account_id,
                                               bool success) {
  // Make sure the display backlight is not forced off if there is a fingerprint
  // authentication attempt. If the display backlight is off, then the device
  // will authenticate and dismiss the lock screen but it will not be visible to
  // the user.
  Shell::Get()->power_button_controller()->StopForcingBacklightsOff();

  // |account_id| comes from IPC, make sure it refers to a valid user. The
  // fingerprint scan could have also happened while switching users, so the
  // associated account is no longer a big user.
  LoginBigUserView* big_view =
      TryToFindBigUser(account_id, true /*require_auth_active*/);
  if (!big_view || !big_view->auth_user()) {
    return;
  }

  big_view->auth_user()->NotifyFingerprintAuthResult(success);
}

void LockContentsView::OnSmartLockStateChanged(const AccountId& account_id,
                                               SmartLockState state) {
  UserState* user_state = FindStateForUser(account_id);
  if (!user_state) {
    return;
  }

  user_state->smart_lock_state = state;
  LoginBigUserView* big_view =
      TryToFindBigUser(account_id, true /*require_auth_active*/);
  if (!big_view || !big_view->auth_user()) {
    return;
  }

  big_view->auth_user()->SetSmartLockState(state);
  LayoutAuth(big_view, /*opt_to_hide=*/nullptr, /*animate=*/true);
}

void LockContentsView::OnSmartLockAuthResult(const AccountId& account_id,
                                             bool success) {
  LoginBigUserView* big_view =
      TryToFindBigUser(account_id, true /*require_auth_active*/);
  if (!big_view || !big_view->auth_user()) {
    return;
  }

  big_view->auth_user()->NotifySmartLockAuthResult(success);
  LayoutAuth(big_view, /*opt_to_hide=*/nullptr, /*animate=*/true);
}

void LockContentsView::OnAuthFactorIsHidingPasswordChanged(
    const AccountId& account_id,
    bool auth_factor_is_hiding_password) {
  UserState* user_state = FindStateForUser(account_id);
  if (!user_state) {
    return;
  }

  user_state->auth_factor_is_hiding_password = auth_factor_is_hiding_password;

  if (auth_factor_is_hiding_password) {
    HideAuthErrorMessage();
  }

  // Do not call LayoutAuth() here. This event is triggered by
  // OnSmartLockStateChanged, which calls LayoutAuth(). Calling LayoutAuth() a
  // second time will prevent animations from running properly.
}

void LockContentsView::OnAuthEnabledForUser(const AccountId& user) {
  UserState* state = FindStateForUser(user);
  if (!state) {
    LOG(ERROR) << "Unable to find user when enabling auth.";
    return;
  }

  state->disable_auth = false;
  disable_lock_screen_note_ = state->disable_auth;
  OnLockScreenNoteStateChanged(
      Shell::Get()->tray_action()->GetLockScreenNoteState());

  LoginBigUserView* big_user =
      TryToFindBigUser(user, true /*require_auth_active*/);
  if (big_user && big_user->auth_user()) {
    LayoutAuth(big_user, nullptr /*opt_to_hide*/, true /*animate*/);
  }
}

void LockContentsView::OnAuthDisabledForUser(
    const AccountId& user,
    const AuthDisabledData& auth_disabled_data) {
  UserState* state = FindStateForUser(user);
  if (!state) {
    LOG(ERROR) << "Unable to find user when disabling auth";
    return;
  }

  state->disable_auth = true;
  disable_lock_screen_note_ = state->disable_auth;
  OnLockScreenNoteStateChanged(mojom::TrayActionState::kNotAvailable);

  if (auth_disabled_data.disable_lock_screen_media) {
    Shell::Get()->media_controller()->SuspendMediaSessions();
    HideMediaView();
  }

  LoginBigUserView* big_user =
      TryToFindBigUser(user, true /*require_auth_active*/);
  if (big_user && big_user->auth_user()) {
    LayoutAuth(big_user, nullptr /*opt_to_hide*/, true /*animate*/);
    big_user->auth_user()->SetAuthDisabledMessage(auth_disabled_data);
  }
}

void LockContentsView::OnAuthenticationStageChanged(
    const AuthenticationStage auth_stage) {
  if (auth_stage != AuthenticationStage::kIdle && auth_error_bubble_) {
    HideAuthErrorMessage();
  }
}

void LockContentsView::OnSetTpmLockedState(const AccountId& user,
                                           bool is_locked,
                                           base::TimeDelta time_left) {
  UserState* state = FindStateForUser(user);
  if (!state) {
    LOG(ERROR) << "Unable to find user when setting TPM lock state";
    return;
  }

  state->time_until_tpm_unlock =
      is_locked ? std::make_optional(time_left) : std::nullopt;

  LoginBigUserView* big_user =
      TryToFindBigUser(user, true /*require_auth_active*/);
  if (big_user && big_user->auth_user()) {
    LayoutAuth(big_user, nullptr /*opt_to_hide*/, true /*animate*/);
  }
}

void LockContentsView::OnForceOnlineSignInForUser(const AccountId& user) {
  UserState* state = FindStateForUser(user);
  if (!state) {
    LOG(ERROR) << "Unable to find user forcing online sign in";
    return;
  }
  state->force_online_sign_in = true;

  LoginBigUserView* big_user =
      TryToFindBigUser(user, true /*require_auth_active*/);
  if (big_user && big_user->auth_user()) {
    LayoutAuth(big_user, nullptr /*opt_to_hide*/, true /*animate*/);
  }
}

void LockContentsView::OnWarningMessageUpdated(const std::u16string& message) {
  if (message.empty()) {
    if (warning_banner_bubble_->GetVisible()) {
      warning_banner_bubble_->Hide();
    }
    return;
  }

  if (!CurrentBigUserView() || !CurrentBigUserView()->auth_user()) {
    LOG(ERROR) << "Unable to find the current active big user to show a "
                  "warning banner.";
    return;
  }

  if (warning_banner_bubble_->GetVisible()) {
    warning_banner_bubble_->Hide();
  }
  // Shows warning banner as a persistent error bubble.
  warning_banner_bubble_->SetAnchorView(
      CurrentBigUserView()->auth_user()->GetActiveInputView());
  warning_banner_bubble_->SetTextContent(message);
  warning_banner_bubble_->Show();
}

void LockContentsView::OnLockScreenNoteStateChanged(
    mojom::TrayActionState state) {
  if (disable_lock_screen_note_) {
    state = mojom::TrayActionState::kNotAvailable;
  }

  bool old_lock_screen_apps_active = lock_screen_apps_active_;
  lock_screen_apps_active_ = state == mojom::TrayActionState::kActive;
  note_action_->UpdateVisibility(state);
  LayoutTopHeader();

  // If lock screen apps just got deactivated - request focus for primary auth,
  // which should focus the password field.
  if (old_lock_screen_apps_active && !lock_screen_apps_active_ &&
      primary_big_view_) {
    primary_big_view_->RequestFocus();
  }
}

void LockContentsView::OnSystemInfoChanged(
    bool show,
    bool enforced,
    const std::string& os_version_label_text,
    const std::string& enterprise_info_text,
    const std::string& bluetooth_name,
    bool adb_sideloading_enabled) {
  // Helper function to create a label for the system info view.
  auto create_info_label = []() {
    auto label = std::make_unique<views::Label>();
    label->SetAutoColorReadabilityEnabled(false);
    label->SetFontList(views::Label::GetDefaultFontList().Derive(
        -1, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL));
    label->SetSubpixelRenderingEnabled(false);
    return label;
  };

  // Initialize the system info view.
  if (system_info_->children().empty()) {
    for (int i = 0; i < 3; ++i) {
      system_info_->AddChildView(create_info_label());
    }
    UpdateSystemInfoColors();
  }

  if (enforced) {
    enable_system_info_enforced_ = show;
  } else {
    enable_system_info_enforced_ = std::nullopt;
    enable_system_info_if_possible_ |= show;
  }

  bool system_info_visible = GetSystemInfoVisibility();
  system_info_->SetVisible(system_info_visible);

  auto update_label = [&](size_t index, const std::string& text) {
    views::Label* label =
        static_cast<views::Label*>(system_info_->children()[index]);
    label->SetText(base::UTF8ToUTF16(text));
    label->SetVisible(!text.empty());
  };
  update_label(0, os_version_label_text);
  update_label(1, enterprise_info_text);
  update_label(2, bluetooth_name);

  LayoutTopHeader();

  // TODO(crbug.com/40727114): Separate ADB sideloading from system info
  // changed. Note that if ADB is enabled and the device is enrolled, only the
  // ADB warning message will be displayed.
  if (adb_sideloading_enabled) {
    ShowAdbEnabled();
  }

  LayoutBottomStatusIndicator();
}

void LockContentsView::OnPublicSessionDisplayNameChanged(
    const AccountId& account_id,
    const std::string& display_name) {
  LoginUserView* user_view = TryToFindUserView(account_id);
  if (!user_view || !IsPublicAccountUser(user_view->current_user())) {
    return;
  }

  LoginUserInfo user_info = user_view->current_user();
  user_info.basic_user_info.display_name = display_name;
  user_view->UpdateForUser(user_info, false /*animate*/);
  MaybeUpdateExpandedView(account_id, user_info);
}

void LockContentsView::OnPublicSessionLocalesChanged(
    const AccountId& account_id,
    const std::vector<LocaleItem>& locales,
    const std::string& default_locale,
    bool show_advanced_view) {
  LoginUserView* user_view = TryToFindUserView(account_id);
  if (!user_view || !IsPublicAccountUser(user_view->current_user())) {
    return;
  }

  LoginUserInfo user_info = user_view->current_user();
  user_info.public_account_info->available_locales = locales;
  user_info.public_account_info->default_locale = default_locale;
  user_info.public_account_info->show_advanced_view = show_advanced_view;
  user_view->UpdateForUser(user_info, false /*animate*/);
  MaybeUpdateExpandedView(account_id, user_info);
}

void LockContentsView::OnPublicSessionKeyboardLayoutsChanged(
    const AccountId& account_id,
    const std::string& locale,
    const std::vector<InputMethodItem>& keyboard_layouts) {
  LoginUserView* user_view = TryToFindUserView(account_id);
  if (!user_view || !IsPublicAccountUser(user_view->current_user())) {
    LOG(ERROR) << "Unable to find public account user.";
    return;
  }

  LoginUserInfo user_info = user_view->current_user();
  user_info.public_account_info->keyboard_layouts = keyboard_layouts;
  // Skip updating keyboard layouts if |locale| is not the default locale
  // of the user. I.e. user changed the default locale in the expanded view,
  // and it should be handled by expanded view.
  if (user_info.public_account_info->default_locale == locale) {
    user_view->UpdateForUser(user_info, false /*animate*/);
  }
  user_info.public_account_info->default_locale = locale;
  MaybeUpdateExpandedView(account_id, user_info);
}

void LockContentsView::OnPublicSessionShowFullManagementDisclosureChanged(
    bool show_full_management_disclosure) {
  expanded_view_->SetShowFullManagementDisclosure(
      show_full_management_disclosure);
}

void LockContentsView::OnDetachableBasePairingStatusChanged(
    DetachableBasePairingStatus pairing_status) {
  // If the current big user is public account user, or the base is not paired,
  // or the paired base matches the last used by the current user, the
  // detachable base error bubble should be hidden. Otherwise, the bubble should
  // be shown.
  if (!CurrentBigUserView() || !CurrentBigUserView()->auth_user() ||
      pairing_status == DetachableBasePairingStatus::kNone ||
      (pairing_status == DetachableBasePairingStatus::kAuthenticated &&
       detachable_base_model_->PairedBaseMatchesLastUsedByUser(
           CurrentBigUserView()->GetCurrentUser().basic_user_info))) {
    if (detachable_base_error_bubble_->GetVisible()) {
      detachable_base_error_bubble_->Hide();
    }
    return;
  }

  HideAuthErrorMessage();

  std::u16string error_text =
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_ERROR_DETACHABLE_BASE_CHANGED);

  detachable_base_error_bubble_->SetTextContent(error_text);
  detachable_base_error_bubble_->SetAnchorView(
      CurrentBigUserView()->auth_user()->GetActiveInputView());
  detachable_base_error_bubble_->Show();

  // Remove the focus from the password field, to make user less likely to enter
  // the password without seeing the warning about detachable base change.
  if (GetWidget()->IsActive()) {
    GetWidget()->GetFocusManager()->ClearFocus();
  }
}

void LockContentsView::OnFocusLeavingLockScreenApps(bool reverse) {
  if (!reverse || lock_screen_apps_active_) {
    FocusNextWidget(reverse);
  } else {
    FocusFirstOrLastFocusableChild(this, reverse);
  }
}

void LockContentsView::OnOobeDialogStateChanged(OobeDialogState state) {
  const bool oobe_dialog_was_visible = oobe_dialog_visible_;
  oobe_dialog_visible_ = state != OobeDialogState::HIDDEN &&
                         state != OobeDialogState::EXTENSION_LOGIN &&
                         state != OobeDialogState::EXTENSION_LOGIN_CLOSED;
  extension_ui_visible_ = state == OobeDialogState::EXTENSION_LOGIN;
  const bool oobe_dialog_closed = state == OobeDialogState::HIDDEN;
  // If the main dialog is not visible any more. The main dialog can either be
  // the OOBE dialog or the login screen extension UI.
  const bool main_dialog_closed =
      oobe_dialog_closed || state == OobeDialogState::EXTENSION_LOGIN_CLOSED;

  // Show either oobe dialog or user pods.
  if (main_view_) {
    main_view_->SetVisible(main_dialog_closed);
  }
  GetWidget()->widget_delegate()->SetCanActivate(main_dialog_closed);

  UpdateBottomStatusIndicatorVisibility();

  if (main_dialog_closed && CurrentBigUserView()) {
    OnBigUserChanged();
  } else if (oobe_dialog_closed && login_camera_timeout_view_) {
    login_camera_timeout_view_->RequestFocus();
  }
  // If OOBE dialog visibility changes we need to force an update of the a11y
  // tree to fix linear navigation from `StatusAreaWidget` to `LockScreen`.
  if (oobe_dialog_visible_ != oobe_dialog_was_visible) {
    Shelf* shelf = Shelf::ForWindow(GetWidget()->GetNativeWindow());
    shelf->GetStatusAreaWidget()
        ->status_area_widget_delegate()
        ->NotifyAccessibilityEvent(ax::mojom::Event::kStateChanged, true);
  }
}

void LockContentsView::MaybeUpdateExpandedView(const AccountId& account_id,
                                               const LoginUserInfo& user_info) {
  if (expanded_view_ && expanded_view_->GetVisible() &&
      expanded_view_->current_user().basic_user_info.account_id == account_id) {
    expanded_view_->UpdateForUser(user_info);
  }
}

void LockContentsView::OnFocusLeavingSystemTray(bool reverse) {
  // This function is called when the system tray is losing focus. We want to
  // focus the first or last child in this view, a lock screen app window if
  // one is active (in which case lock contents should not have focus), or the
  // OOBE dialog modal if it's active. In the later cases, still focus lock
  // screen first, to synchronously take focus away from the system shelf (or
  // tray) - lock shelf view expect the focus to be taken when it passes it
  // to lock screen view, and can misbehave in case the focus is kept in it.
  FocusFirstOrLastFocusableChild(this, reverse);

  if (lock_screen_apps_active_) {
    Shell::Get()->login_screen_controller()->FocusLockScreenApps(reverse);
    return;
  }

  if (oobe_dialog_visible_) {
    Shell::Get()->login_screen_controller()->FocusOobeDialog();
  }
}

void LockContentsView::OnDisplayMetricsChanged(const display::Display& display,
                                               uint32_t changed_metrics) {
  // Ignore all metrics except for those listed in |filter|.
  uint32_t filter = DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_WORK_AREA |
                    DISPLAY_METRIC_DEVICE_SCALE_FACTOR |
                    DISPLAY_METRIC_ROTATION | DISPLAY_METRIC_PRIMARY;
  if ((filter & changed_metrics) == 0) {
    return;
  }

  DoLayout();

  // Set bounds here so that the lock screen widget always shows up on the
  // primary display. Sometimes the widget bounds are incorrect in the case
  // where multiple external displays are used. See crbug.com/1031571.
  GetWidget()->SetBounds(
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds());
}

void LockContentsView::OnKeyboardVisibilityChanged(bool is_visible) {
  if (!primary_big_view_ || keyboard_shown_ == is_visible) {
    return;
  }

  keyboard_shown_ = is_visible;
  if (!ongoing_auth_layout_) {
    LayoutAuth(CurrentBigUserView(), nullptr /*opt_to_hide*/, true /*animate*/);
  }
}

void LockContentsView::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  LoginBigUserView* big_user = CurrentBigUserView();
  if (big_user && big_user->auth_user()) {
    big_user->auth_user()->password_view()->Reset();
  }
}

void LockContentsView::OnDeviceEnterpriseInfoChanged() {
  // If feature is enabled, update the boolean kiosk_license_mode_. Otherwise,
  // it's false by default.
  if (!features::IsKioskLoginScreenEnabled()) {
    return;
  }

  kiosk_license_mode_ =
      Shell::Get()
          ->system_tray_model()
          ->enterprise_domain()
          ->management_device_mode() == ManagementDeviceMode::kKioskSku;

  UpdateKioskDefaultMessageVisibility();
}

void LockContentsView::OnEnterpriseAccountDomainChanged() {}

void LockContentsView::FocusNextWidget(bool reverse) {
  Shelf* shelf = Shelf::ForWindow(GetWidget()->GetNativeWindow());
  // Tell the focus direction to the status area or the shelf so they can focus
  // the correct child view.
  if (reverse) {
    shelf->GetStatusAreaWidget()
        ->status_area_widget_delegate()
        ->set_default_last_focusable_child(reverse);
    Shell::Get()->focus_cycler()->FocusWidget(shelf->GetStatusAreaWidget());
  } else {
    shelf->login_shelf_widget()->SetDefaultLastFocusableChild(reverse);
    Shell::Get()->focus_cycler()->FocusWidget(shelf->login_shelf_widget());
  }
}

void LockContentsView::SetLowDensitySpacing(views::View* spacing_middle,
                                            views::View* secondary_view,
                                            int landscape_dist,
                                            int portrait_dist,
                                            bool landscape) {
  int total_width = GetPreferredSize().width();
  int available_width =
      total_width - (primary_big_view_->GetPreferredSize().width() +
                     secondary_view->GetPreferredSize().width());
  if (available_width <= 0) {
    SetPreferredWidthForView(spacing_middle, 0);
    return;
  }

  int desired_width = landscape ? landscape_dist : portrait_dist;
  SetPreferredWidthForView(spacing_middle,
                           std::min(available_width, desired_width));
}

void LockContentsView::SetMediaViewSpacing(bool landscape) {
  int total_width = GetPreferredSize().width();

  int available_width =
      total_width - (primary_big_view_->GetPreferredSize().width() +
                     media_view_->GetPreferredSize().width());
  if (available_width <= 0) {
    SetPreferredWidthForView(middle_spacing_view_, 0);
    return;
  }

  int desired_width;
  if (!landscape || total_width <= kMediaControlsSpacingThreshold) {
    desired_width = available_width / kMediaControlsSmallSpaceFactor;
  } else {
    desired_width = available_width / kMediaControlsLargeSpaceFactor;
  }

  SetPreferredWidthForView(middle_spacing_view_, desired_width);
}

void LockContentsView::CreateMediaView() {
  CHECK(middle_spacing_view_);
  middle_spacing_view_->SetVisible(true);

  CHECK(media_view_);
  media_view_->SetVisible(true);

  // Set |spacing_middle|.
  AddDisplayLayoutAction(base::BindRepeating(
      &LockContentsView::SetMediaViewSpacing, base::Unretained(this)));

  DeprecatedLayoutImmediately();
}

void LockContentsView::HideMediaView() {
  CHECK(middle_spacing_view_);
  middle_spacing_view_->SetVisible(false);

  CHECK(media_view_);
  media_view_->SetVisible(false);

  // Don't allow media keys to be used on lock screen since controls are hidden.
  Shell::Get()->media_controller()->SetMediaControlsDismissed(true);

  DeprecatedLayoutImmediately();
}

bool LockContentsView::AreMediaControlsEnabled() const {
  return screen_type_ == LockScreen::ScreenType::kLock &&
         !expanded_view_->GetVisible() &&
         Shell::Get()->media_controller()->AreLockScreenMediaKeysEnabled();
}

void LockContentsView::OnWillChangeFocus(View* focused_before,
                                         View* focused_now) {}

void LockContentsView::OnDidChangeFocus(View* focused_before,
                                        View* focused_now) {
  if (!focused_before || !focused_now) {
    return;
  }

  if (!auth_error_bubble_ || !auth_error_bubble_->GetVisible()) {
    return;
  }
  views::View* anchor = auth_error_bubble_->GetAnchorView();
  if (!anchor) {
    return;
  }

  if (!widget_) {
    LOG(ERROR) << "Focus change event without widget";
    return;
  }
  views::FocusManager* focus_manager = widget_->GetFocusManager();
  if (!focus_manager) {
    LOG(ERROR) << "Widget misses FocusManager";
    return;
  }

  if (focus_manager->focus_change_reason() !=
      views::FocusManager::FocusChangeReason::kFocusTraversal) {
    return;
  }

  const bool before_in_anchor = anchor->Contains(focused_before);
  if (!before_in_anchor) {
    return;
  }

  LoginBigUserView* big_user = CurrentBigUserView();
  if (!big_user) {
    return;
  }
  LoginAuthUserView* auth_user = big_user->auth_user();
  if (!auth_user) {
    return;
  }
  LoginUserView* user_view = auth_user->user_view();
  if (!user_view) {
    return;
  }

  views::View* dropdown_button = user_view->GetDropdownButton();
  const bool now_in_dropdown_button =
      dropdown_button && dropdown_button->Contains(focused_now);

  views::View* pin_password_toggle = auth_user->pin_password_toggle();
  const bool now_in_pin_password_toggle =
      pin_password_toggle && pin_password_toggle->Contains(focused_now);

  if (!now_in_dropdown_button && !now_in_pin_password_toggle) {
    return;
  }

  FocusFirstOrLastFocusableChild(auth_error_bubble_.get(), /*reverse=*/false);
}

void LockContentsView::CreateLowDensityLayout(
    const std::vector<LoginUserInfo>& users,
    std::unique_ptr<LoginBigUserView> primary_big_view) {
  DCHECK_LE(users.size(), 2u);

  primary_big_view_ = main_view_->AddChildView(std::move(primary_big_view));

  // Space between primary user and the media view.
  middle_spacing_view_ =
      main_view_->AddChildView(std::make_unique<NonAccessibleView>());
  middle_spacing_view_->SetVisible(false);

  // Build the view for media controls. Using base::Unretained(this) is safe
  // here because these callbacks are used by a media view owned by |this|.
  media_view_ = main_view_->AddChildView(std::make_unique<LockScreenMediaView>(
      base::BindRepeating(&LockContentsView::AreMediaControlsEnabled,
                          base::Unretained(this)),
      base::BindRepeating(&LockContentsView::CreateMediaView,
                          base::Unretained(this)),
      base::BindRepeating(&LockContentsView::HideMediaView,
                          base::Unretained(this))));
  media_view_->SetVisible(false);

  if (users.size() > 1) {
    // Space between primary user and secondary user.
    auto* spacing_middle =
        main_view_->AddChildView(std::make_unique<NonAccessibleView>());

    // Build secondary auth user.
    opt_secondary_big_view_ = main_view_->AddChildView(
        AllocateLoginBigUserView(users[1], false /*is_primary*/));

    // Set |spacing_middle|.
    AddDisplayLayoutAction(base::BindRepeating(
        &LockContentsView::SetLowDensitySpacing, base::Unretained(this),
        spacing_middle, opt_secondary_big_view_,
        kLowDensityDistanceBetweenUsersInLandscapeDp,
        kLowDensityDistanceBetweenUsersInPortraitDp));
  }
}

void LockContentsView::CreateMediumDensityLayout(
    const std::vector<LoginUserInfo>& users,
    std::unique_ptr<LoginBigUserView> primary_big_view) {
  // Here is a diagram of this layout:
  //
  //    a A x B y b
  //
  // a, A: spacing_left
  // x: primary_big_view_
  // B: spacing_middle
  // y: users_list_
  // b: spacing_right
  //
  // A and B are fixed-width spaces; a and b are flexible space that consume any
  // additional width.
  //
  // A and B are the reason for custom layout; no layout manager currently
  // supports a fixed-width view that can shrink, but not grow (ie, bounds from
  // [0,x]). Custom layout logic is used instead, which is contained inside of
  // the AddDisplayLayoutAction call below.

  // Construct and add views as described above.
  auto* spacing_left =
      main_view_->AddChildView(std::make_unique<NonAccessibleView>());
  primary_big_view_ = main_view_->AddChildView(std::move(primary_big_view));
  auto* spacing_middle =
      main_view_->AddChildView(std::make_unique<NonAccessibleView>());
  users_list_ =
      main_view_->AddChildView(std::make_unique<ScrollableUsersListView>(
          users,
          base::BindRepeating(&LockContentsView::SwapToBigUser,
                              base::Unretained(this)),
          LoginDisplayStyle::kSmall));
  auto* spacing_right =
      main_view_->AddChildView(std::make_unique<NonAccessibleView>());

  // Set width for the |spacing_*| views.
  AddDisplayLayoutAction(base::BindRepeating(
      [](views::View* host_view, views::View* big_user_view,
         ScrollableUsersListView* users_list, views::View* spacing_left,
         views::View* spacing_middle, views::View* spacing_right,
         bool landscape) {
        // `users_list` has margins that depend on the current orientation.
        // Update these here so that the following calculations see the correct
        // bounds.
        users_list->UpdateUserViewHostLayoutInsets();

        int total_width = host_view->GetPreferredSize().width();
        int available_width =
            total_width - (big_user_view->GetPreferredSize().width() +
                           users_list->GetPreferredSize().width());

        int left_max_fixed_width =
            landscape ? kMediumDensityMarginLeftOfAuthUserLandscapeDp
                      : kMediumDensityMarginLeftOfAuthUserPortraitDp;
        int right_max_fixed_width =
            landscape ? kMediumDensityDistanceBetweenAuthUserAndUsersLandscapeDp
                      : kMediumDensityDistanceBetweenAuthUserAndUsersPortraitDp;

        int left_flex_weight = landscape ? 1 : 2;
        int right_flex_weight = 1;

        MediumViewLayout medium_layout(
            available_width, left_flex_weight, left_max_fixed_width,
            right_max_fixed_width, right_flex_weight);

        SetPreferredWidthForView(
            spacing_left,
            medium_layout.left_flex_width + medium_layout.left_fixed_width);
        SetPreferredWidthForView(spacing_middle,
                                 medium_layout.right_fixed_width);
        SetPreferredWidthForView(spacing_right, medium_layout.right_flex_width);
      },
      this, primary_big_view_, users_list_, spacing_left, spacing_middle,
      spacing_right));
}

void LockContentsView::CreateHighDensityLayout(
    const std::vector<LoginUserInfo>& users,
    views::BoxLayout* main_layout,
    std::unique_ptr<LoginBigUserView> primary_big_view) {
  // Insert spacing before the auth view.
  auto* fill = main_view_->AddChildView(std::make_unique<NonAccessibleView>());
  main_layout->SetFlexForView(fill, 1);

  primary_big_view_ = main_view_->AddChildView(std::move(primary_big_view));

  // Insert spacing after the auth view.
  fill = main_view_->AddChildView(std::make_unique<NonAccessibleView>());
  main_layout->SetFlexForView(fill, 1);

  users_list_ =
      main_view_->AddChildView(std::make_unique<ScrollableUsersListView>(
          users,
          base::BindRepeating(&LockContentsView::SwapToBigUser,
                              base::Unretained(this)),
          LoginDisplayStyle::kExtraSmall));

  // User list size may change after a display metric change.
  AddDisplayLayoutAction(base::BindRepeating(
      [](views::View* view, bool landscape) { view->SizeToPreferredSize(); },
      users_list_));
}

void LockContentsView::DoLayout() {
  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          GetWidget()->GetNativeWindow());

  // Set preferred size before running layout actions, as layout actions may
  // depend on the preferred size to determine layout.
  gfx::Size preferred_size = display.size();
  preferred_size.set_height(preferred_size.height() -
                            keyboard::KeyboardUIController::Get()
                                ->GetWorkspaceOccludedBoundsInScreen()
                                .height());
  SetPreferredSize(preferred_size);

  bool landscape = login_views_utils::ShouldShowLandscape(GetWidget());
  for (auto& action : layout_actions_) {
    action.Run(landscape);
  }

  // SizeToPreferredSize will trigger layout.
  SizeToPreferredSize();
}

void LockContentsView::LayoutTopHeader() {
  int preferred_width = system_info_->GetPreferredSize().width() +
                        note_action_->GetPreferredSize().width();
  int preferred_height = std::max(system_info_->GetPreferredSize().height(),
                                  note_action_->GetPreferredSize().height());
  top_header_->SetPreferredSize(gfx::Size(preferred_width, preferred_height));
  top_header_->SizeToPreferredSize();
  top_header_->DeprecatedLayoutImmediately();
  // Position the top header - the origin is offset to the left from the top
  // right corner of the entire view by the width of this top header view.
  top_header_->SetPosition(GetLocalBounds().top_right() -
                           gfx::Vector2d(preferred_width, 0));
}

void LockContentsView::LayoutBottomStatusIndicator() {
  bottom_status_indicator_->SizeToPreferredSize();

  // Position the warning indicator in the middle above the shelf.
  bottom_status_indicator_->SetPosition(
      GetLocalBounds().bottom_center() -
      gfx::Vector2d(bottom_status_indicator_->width() / 2,
                    ShelfConfig::Get()->shelf_size() +
                        kBottomStatusIndicatorBottomMarginDp +
                        bottom_status_indicator_->height()));

  // If the management bubble is currently displayed, we need to re-layout it as
  // the bottom status indicator is its anchor view.
  if (management_bubble_->GetVisible()) {
    management_bubble_->DeprecatedLayoutImmediately();
  }
}

void LockContentsView::LayoutUserAddingScreenIndicator() {
  if (Shell::Get()->session_controller()->GetSessionState() !=
      session_manager::SessionState::LOGIN_SECONDARY) {
    return;
  }

  // The primary big view may not be ready yet.
  if (!primary_big_view_) {
    return;
  }

  user_adding_screen_indicator_->SizeToPreferredSize();
  // The element is placed at the middle of the screen horizontally. It is
  // placed kDistanceFromBottomOfIndicatorToUserIconDp above the user icon.
  // However, if the screen is too small, it is placed
  // kMinDistanceFromTopOfScreenToIndicatorDp from top of screen.
  int y =
      std::max(kMinDistanceFromTopOfScreenToIndicatorDp,
               primary_big_view_->y() -
                   user_adding_screen_indicator_->GetPreferredSize().height() -
                   kDistanceFromBottomOfIndicatorToUserIconDp);
  gfx::Point position(
      bounds().width() / 2 -
          user_adding_screen_indicator_->GetPreferredSize().width() / 2,
      y);

  user_adding_screen_indicator_->SetPosition(position);
}

void LockContentsView::LayoutPublicSessionView() {
  gfx::Rect bounds = GetContentsBounds();
  bounds.set_height(bounds.height() - ShelfConfig::Get()->shelf_size());
  gfx::Size pref_size = bounds.width() >= bounds.height()
                            ? expanded_view_->GetPreferredSizeLandscape()
                            : expanded_view_->GetPreferredSizePortrait();
  bounds.ClampToCenteredSize(pref_size);
  expanded_view_->SetBoundsRect(bounds);
}

void LockContentsView::AddDisplayLayoutAction(
    const DisplayLayoutAction& layout_action) {
  layout_action.Run(login_views_utils::ShouldShowLandscape(GetWidget()));
  layout_actions_.push_back(layout_action);
}

void LockContentsView::SwapActiveAuthBetweenPrimaryAndSecondary(
    bool is_primary) {
  // Do not allow user-swap during authentication.
  if (Shell::Get()->login_screen_controller()->IsAuthenticating()) {
    return;
  }

  if (is_primary) {
    if (!primary_big_view_->IsAuthEnabled()) {
      LayoutAuth(primary_big_view_, opt_secondary_big_view_, true /*animate*/);
      OnBigUserChanged();
    } else {
      primary_big_view_->RequestFocus();
    }
  } else if (!is_primary && opt_secondary_big_view_) {
    if (!opt_secondary_big_view_->IsAuthEnabled()) {
      LayoutAuth(opt_secondary_big_view_, primary_big_view_, true /*animate*/);
      OnBigUserChanged();
    } else {
      opt_secondary_big_view_->RequestFocus();
    }
  }
}

void LockContentsView::OnAuthenticate(bool auth_success,
                                      bool display_error_messages,
                                      bool authenticated_by_pin) {
  AccountId account_id =
      CurrentBigUserView()->GetCurrentUser().basic_user_info.account_id;
  if (auth_success) {
    HideAuthErrorMessage();

    if (detachable_base_error_bubble_->GetVisible()) {
      detachable_base_error_bubble_->Hide();
    }

    // Now that the user has been authenticated, update the user's last used
    // detachable base (if one is attached). This will prevent further
    // detachable base change notifications from appearing for this base (until
    // the user uses another detachable base).
    if (CurrentBigUserView()->auth_user() &&
        detachable_base_model_->GetPairingStatus() ==
            DetachableBasePairingStatus::kAuthenticated) {
      detachable_base_model_->SetPairedBaseAsLastUsedByUser(
          CurrentBigUserView()->GetCurrentUser().basic_user_info);
    }

    // Times a password was incorrectly entered until user succeeds.
    RecordAndResetPasswordAttempts(
        AuthEventsRecorder::AuthenticationOutcome::kSuccess, account_id);
  } else {
    ++unlock_attempt_by_user_[account_id];
    if (authenticated_by_pin) {
      ++pin_unlock_attempt_by_user_[account_id];
    }
    if (pending_users_change_.has_value()) {
      const std::vector<LoginUserInfo> pending_users_change(
          std::move(*pending_users_change_));
      pending_users_change_.reset();
      LOG(WARNING) << "Running the postponed re-layout.";
      ApplyUserChanges(pending_users_change);
    }

    if (display_error_messages) {
      ShowAuthErrorMessage(authenticated_by_pin);
    }
  }
}

UserState* LockContentsView::FindStateForUser(const AccountId& user) {
  for (UserState& state : users_) {
    if (state.account_id == user) {
      return &state;
    }
  }

  return nullptr;
}

void LockContentsView::LayoutAuth(LoginBigUserView* to_update,
                                  LoginBigUserView* opt_to_hide,
                                  bool animate) {
  DCHECK(to_update);

  auto capture_animation_state_pre_layout = [&](LoginBigUserView* view) {
    if (!view) {
      return;
    }
    if (view->auth_user()) {
      view->auth_user()->CaptureStateForAnimationPreLayout();
    }
  };

  auto enable_auth = [&](LoginBigUserView* view) {
    DCHECK(view);
    if (view->auth_user()) {
      UserState* state = FindStateForUser(
          view->auth_user()->current_user().basic_user_info.account_id);
      uint32_t to_update_auth;
      LoginAuthUserView::AuthMethodsMetadata auth_metadata;
      if (state->time_until_tpm_unlock.has_value()) {
        // TPM is locked
        to_update_auth = LoginAuthUserView::AUTH_DISABLED_TPM_LOCKED;
        auth_metadata.time_until_tpm_unlock =
            state->time_until_tpm_unlock.value();
      } else if (state->force_online_sign_in) {
        to_update_auth = LoginAuthUserView::AUTH_ONLINE_SIGN_IN;
      } else if (state->disable_auth) {
        to_update_auth = LoginAuthUserView::AUTH_DISABLED;
      } else if (state->show_challenge_response_auth) {
        // Currently the challenge-response authentication can't be combined
        // with the password or PIN based one.
        to_update_auth = LoginAuthUserView::AUTH_CHALLENGE_RESPONSE;
      } else if (!state->show_password && !state->show_pin) {
        CHECK(IsTimeInFuture(state->pin_available_at))
            << "Password or pin factor must be present, if pin is not locked";
        to_update_auth = LoginAuthUserView::AUTH_RECOVERY;
        auth_metadata.pin_available_at = state->pin_available_at;
        // The auth error message might be shown at the moment due to previous
        // wrong attempts. We will hide it as it shows similar content as the
        // recover button and the pin delay message.
        HideAuthErrorMessage();
      } else {
        to_update_auth = LoginAuthUserView::AUTH_NONE;
        if (state->show_password) {
          to_update_auth |= LoginAuthUserView::AUTH_PASSWORD;
        }
        // Need to check |GetKeyboardControllerForView| as the keyboard may be
        // visible, but the keyboard is in a different root window or the view
        // has not been added to the widget. In these cases, the keyboard does
        // not interfere with PIN entry.
        auth_metadata.virtual_keyboard_visible =
            GetKeyboardControllerForView() ? keyboard_shown_ : false;
        auth_metadata.show_pinpad_for_pw = state->show_pin_pad_for_password;
        auth_metadata.autosubmit_pin_length = state->autosubmit_pin_length;
        auth_metadata.pin_available_at = state->pin_available_at;
        if (state->show_pin) {
          to_update_auth |= LoginAuthUserView::AUTH_PIN;
        }
        if (state->fingerprint_state != FingerprintState::UNAVAILABLE) {
          to_update_auth |= LoginAuthUserView::AUTH_FINGERPRINT;
        }
        if (state->smart_lock_state != SmartLockState::kDisabled &&
            state->smart_lock_state != SmartLockState::kInactive) {
          to_update_auth |= LoginAuthUserView::AUTH_SMART_LOCK;
        }
        if (state->auth_factor_is_hiding_password) {
          to_update_auth |=
              LoginAuthUserView::AUTH_AUTH_FACTOR_IS_HIDING_PASSWORD;
        }
      }
      view->auth_user()->SetAuthMethods(to_update_auth, auth_metadata);
      if (auth_error_bubble_->GetVisible()) {
        // Update the anchor position in case the active input view changed.
        auth_error_bubble_->SetAnchorView(
            view->auth_user()->GetActiveInputView());
      }
    } else if (view->public_account()) {
      view->public_account()->SetAuthEnabled(true /*enabled*/, animate);
    }
  };

  auto disable_auth = [&](LoginBigUserView* view) {
    if (!view) {
      return;
    }
    if (view->auth_user()) {
      view->auth_user()->SetAuthMethods(LoginAuthUserView::AUTH_NONE);
    } else if (view->public_account()) {
      view->public_account()->SetAuthEnabled(false /*enabled*/, animate);
    }
  };

  auto apply_animation_post_layout = [&](LoginBigUserView* view) {
    if (!view) {
      return;
    }
    if (view->auth_user()) {
      view->auth_user()->ApplyAnimationPostLayout(animate);
    }
  };

  ongoing_auth_layout_ = true;
  // The high-level layout flow:
  capture_animation_state_pre_layout(to_update);
  capture_animation_state_pre_layout(opt_to_hide);
  enable_auth(to_update);
  disable_auth(opt_to_hide);
  DeprecatedLayoutImmediately();
  apply_animation_post_layout(to_update);
  apply_animation_post_layout(opt_to_hide);
  ongoing_auth_layout_ = false;
}

void LockContentsView::SwapToBigUser(int user_index) {
  // Do not allow user-swap during authentication.
  if (Shell::Get()->login_screen_controller()->IsAuthenticating()) {
    return;
  }

  DCHECK(users_list_);
  LoginUserView* view = users_list_->user_view_at(user_index);
  DCHECK(view);
  LoginUserInfo previous_big_user = primary_big_view_->GetCurrentUser();
  LoginUserInfo new_big_user = view->current_user();

  view->UpdateForUser(previous_big_user, true /*animate*/);
  primary_big_view_->UpdateForUser(new_big_user);
  LayoutAuth(primary_big_view_, nullptr, true /*animate*/);
  OnBigUserChanged();
}

void LockContentsView::OnRemoveUserWarningShown(bool is_primary) {
  Shell::Get()->login_screen_controller()->OnRemoveUserWarningShown();
}

void LockContentsView::RemoveUser(bool is_primary) {
  // Do not allow removing a user during authentication, such as if the user
  // tried to remove the currently authenticating user.
  if (Shell::Get()->login_screen_controller()->IsAuthenticating()) {
    return;
  }

  LoginBigUserView* to_remove =
      is_primary ? primary_big_view_.get() : opt_secondary_big_view_.get();
  DCHECK(to_remove->GetCurrentUser().can_remove);
  AccountId user = to_remove->GetCurrentUser().basic_user_info.account_id;

  // Ask chrome to remove the user.
  Shell::Get()->login_screen_controller()->RemoveUser(user);
}

void LockContentsView::OnBigUserChanged() {
  const LoginUserInfo& big_user = CurrentBigUserView()->GetCurrentUser();
  const AccountId big_user_account_id = big_user.basic_user_info.account_id;

  CurrentBigUserView()->RequestFocus();

  Shell::Get()->login_screen_controller()->OnFocusPod(big_user_account_id);

  // The new auth user might have different last used detachable base - make
  // sure the detachable base pairing error is updated if needed.
  OnDetachableBasePairingStatusChanged(
      detachable_base_model_->GetPairingStatus());

  if (!detachable_base_error_bubble_->GetVisible()) {
    CurrentBigUserView()->RequestFocus();
  }
}

LoginBigUserView* LockContentsView::CurrentBigUserView() {
  if (opt_secondary_big_view_ && opt_secondary_big_view_->IsAuthEnabled()) {
    DCHECK(!primary_big_view_ || !primary_big_view_->IsAuthEnabled());
    return opt_secondary_big_view_;
  }

  return primary_big_view_;
}

void LockContentsView::ShowAuthErrorMessage(bool authenticated_by_pin) {
  LoginBigUserView* big_view = CurrentBigUserView();
  if (!big_view->auth_user()) {
    return;
  }

  const AccountId account_id =
      big_view->GetCurrentUser().basic_user_info.account_id;
  int unlock_attempt = unlock_attempt_by_user_[account_id];
  UserState* user_state = FindStateForUser(account_id);

  // Do not show the auth error message when there's no password or pin factor
  // configured. This usually occurs when pin is soft-locked due to multiple
  // wrong attempts.
  if (!user_state->show_password && !user_state->show_pin) {
    return;
  }

  auth_error_bubble_->ShowAuthError(
      /*anchor_view = */ big_view->auth_user()->GetActiveInputView(),
      /*unlock_attempt = */ unlock_attempt,
      /*authenticated_by_pin = */ authenticated_by_pin,
      /*is_login_screen = */ screen_type_ == LockScreen::ScreenType::kLogin);
}

void LockContentsView::HideAuthErrorMessage() {
  if (auth_error_bubble_->GetVisible()) {
    auth_error_bubble_->Hide();
  }
}

void LockContentsView::OnParentAccessValidationFinished(
    const AccountId& account_id,
    bool access_granted) {
  UserState* state = FindStateForUser(account_id);
  Shell::Get()->login_screen_controller()->ShowParentAccessButton(
      state && state->disable_auth && !access_granted);
}

keyboard::KeyboardUIController* LockContentsView::GetKeyboardControllerForView()
    const {
  return GetWidget() ? GetKeyboardControllerForWidget(GetWidget()) : nullptr;
}

void LockContentsView::OnPublicAccountTapped(bool is_primary) {
  const LoginBigUserView* user = CurrentBigUserView();
  // If the pod should not show an expanded view, tapping on it will launch
  // Public Session immediately.
  if (!user->GetCurrentUser().public_account_info->show_expanded_view) {
    std::string default_input_method;
    for (const auto& keyboard :
         user->GetCurrentUser().public_account_info->keyboard_layouts) {
      if (keyboard.selected) {
        default_input_method = keyboard.ime_id;
        break;
      }
    }
    Shell::Get()->login_screen_controller()->LaunchPublicSession(
        user->GetCurrentUser().basic_user_info.account_id,
        user->GetCurrentUser().public_account_info->default_locale,
        default_input_method);
    return;
  }

  // Set the public account user to be the active user.
  SwapActiveAuthBetweenPrimaryAndSecondary(is_primary);

  // Update expanded_view_ in case CurrentBigUserView has changed.
  // 1. It happens when the active big user is changed. For example both
  // primary and secondary big user are public account and user switches from
  // primary to secondary.
  // 2. LoginUserInfo in the big user could be changed if we get updates from
  // OnPublicSessionDisplayNameChanged and OnPublicSessionLocalesChanged.
  expanded_view_->UpdateForUser(user->GetCurrentUser());
  SetDisplayStyle(DisplayStyle::kExclusivePublicAccountExpandedView);
}

void LockContentsView::LearnMoreButtonPressed() {
  Shell::Get()->login_screen_controller()->ShowAccountAccessHelpApp(
      GetWidget()->GetNativeWindow());
  HideAuthErrorMessage();
}

void LockContentsView::RecoverUserButtonPressed() {
  LoginBigUserView* big_view = CurrentBigUserView();
  if (!big_view->auth_user()) {
    LOG(ERROR) << "Recover user button pressed without focused user";
    return;
  }

  const AccountId account_id =
      big_view->auth_user()->current_user().basic_user_info.account_id;
  user_manager::KnownUser(Shell::Get()->local_state())
      .UpdateReauthReason(account_id,
                          static_cast<int>(ReauthReason::kForgotPassword));
  RecordAndResetPasswordAttempts(
      AuthEventsRecorder::AuthenticationOutcome::kRecovery, account_id);
  Shell::Get()->login_screen_controller()->StartUserRecovery(account_id);
  HideAuthErrorMessage();
}

std::unique_ptr<LoginBigUserView> LockContentsView::AllocateLoginBigUserView(
    const LoginUserInfo& user,
    bool is_primary) {
  LoginAuthUserView::Callbacks auth_user_callbacks;
  auth_user_callbacks.on_auth = base::BindRepeating(
      &LockContentsView::OnAuthenticate, base::Unretained(this)),
  auth_user_callbacks.on_tap = base::BindRepeating(
      &LockContentsView::SwapActiveAuthBetweenPrimaryAndSecondary,
      base::Unretained(this), is_primary),
  auth_user_callbacks.on_remove_warning_shown =
      base::BindRepeating(&LockContentsView::OnRemoveUserWarningShown,
                          base::Unretained(this), is_primary);
  auth_user_callbacks.on_remove = base::BindRepeating(
      &LockContentsView::RemoveUser, base::Unretained(this), is_primary);
  auth_user_callbacks.on_auth_factor_is_hiding_password_changed =
      base::BindRepeating(
          &LockContentsView::OnAuthFactorIsHidingPasswordChanged,
          base::Unretained(this), user.basic_user_info.account_id);
  auth_user_callbacks.on_pin_unlock = base::BindRepeating(
      &LockContentsView::OnPinUnlock, base::Unretained(this), is_primary);
  auth_user_callbacks.on_recover_button_pressed = base::BindRepeating(
      &LockContentsView::RecoverUserButtonPressed, base::Unretained(this));

  LoginPublicAccountUserView::Callbacks public_account_callbacks;
  public_account_callbacks.on_tap = auth_user_callbacks.on_tap;
  public_account_callbacks.on_public_account_tapped =
      base::BindRepeating(&LockContentsView::OnPublicAccountTapped,
                          base::Unretained(this), is_primary);

  return std::make_unique<LoginBigUserView>(user, auth_user_callbacks,
                                            public_account_callbacks);
}

LoginBigUserView* LockContentsView::TryToFindBigUser(const AccountId& user,
                                                     bool require_auth_active) {
  LoginBigUserView* view = nullptr;

  // Find auth instance.
  if (primary_big_view_ &&
      primary_big_view_->GetCurrentUser().basic_user_info.account_id == user) {
    view = primary_big_view_;
  } else if (opt_secondary_big_view_ &&
             opt_secondary_big_view_->GetCurrentUser()
                     .basic_user_info.account_id == user) {
    view = opt_secondary_big_view_;
  }

  // Make sure auth instance is active if required.
  if (require_auth_active && view && !view->IsAuthEnabled()) {
    view = nullptr;
  }

  return view;
}

LoginUserView* LockContentsView::TryToFindUserView(const AccountId& user) {
  // Try to find |user| in big user view first.
  LoginBigUserView* big_view =
      TryToFindBigUser(user, false /*require_auth_active*/);
  if (big_view) {
    return big_view->GetUserView();
  }

  // Try to find |user| in users_list_.
  if (users_list_) {
    return users_list_->GetUserView(user);
  }

  return nullptr;
}

void LockContentsView::SetDisplayStyle(DisplayStyle style) {
  const bool show_expanded_view =
      style == DisplayStyle::kExclusivePublicAccountExpandedView;
  expanded_view_->SetVisible(show_expanded_view);
  main_view_->SetVisible(!show_expanded_view);
  top_header_->SetVisible(!show_expanded_view);
  bottom_status_indicator_->SetVisible(!show_expanded_view);
  DeprecatedLayoutImmediately();
}

bool LockContentsView::OnKeyPressed(const ui::KeyEvent& event) {
  switch (event.key_code()) {
    case ui::VKEY_RIGHT:
      FocusNextUser();
      return true;
    case ui::VKEY_LEFT:
      FocusPreviousUser();
      return true;
    default:
      return false;
  }
}

void LockContentsView::RegisterAccelerators() {
  for (size_t i = 0; i < kLoginAcceleratorDataLength; ++i) {
    // We need to register global accelerators and a few additional ones that
    // are handled by the WebUI (and normally registered by the WebUI).
    // When WebUI is loaded on demand, we would need to start WebUI after
    // accelerator is pressed. So we register WebUI acceleratos here
    // and then start WebUI when needed and pass the accelerator.
    if (!kLoginAcceleratorData[i].global &&
        kLoginAcceleratorData[i].action !=
            LoginAcceleratorAction::kCancelScreenAction) {
      continue;
    }
    if ((screen_type_ == LockScreen::ScreenType::kLogin) &&
        !(kLoginAcceleratorData[i].scope & kScopeLogin)) {
      continue;
    }
    if ((screen_type_ == LockScreen::ScreenType::kLock) &&
        !(kLoginAcceleratorData[i].scope & kScopeLock)) {
      continue;
    }
    // Show reset conflicts with rotate screen when --ash-dev-shortcuts is
    // passed. Favor --ash-dev-shortcuts since that is explicitly added.
    if (kLoginAcceleratorData[i].action ==
            LoginAcceleratorAction::kShowResetScreen &&
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kAshDeveloperShortcuts)) {
      continue;
    }

    accel_map_[ui::Accelerator(kLoginAcceleratorData[i].keycode,
                               kLoginAcceleratorData[i].modifiers)] =
        kLoginAcceleratorData[i].action;
  }

  // Register the accelerators.
  AcceleratorControllerImpl* controller =
      Shell::Get()->accelerator_controller();
  for (const auto& item : accel_map_) {
    controller->Register({item.first}, this);
  }
}

void LockContentsView::PerformAction(LoginAcceleratorAction action) {
  if (action == LoginAcceleratorAction::kToggleSystemInfo) {
    ToggleSystemInfo();
    return;
  }
  // Do not allow accelerator action when system modal window is open except
  // `kShowFeedback` which opens feedback tool on top of system modal.
  if (!Shell::IsSystemModalWindowOpen() ||
      action == LoginAcceleratorAction::kShowFeedback) {
    Shell::Get()->login_screen_controller()->HandleAccelerator(action);
  }
}

bool LockContentsView::GetSystemInfoVisibility() const {
  if (enable_system_info_enforced_.has_value()) {
    return enable_system_info_enforced_.value();
  } else {
    return enable_system_info_if_possible_;
  }
}

void LockContentsView::UpdateSystemInfoColors() {
  for (views::View* child : system_info_->children()) {
    views::Label* label = static_cast<views::Label*>(child);
    label->SetEnabledColorId(kColorAshTextColorPrimary);
  }
}

void LockContentsView::UpdateBottomStatusIndicatorColors() {
  const bool jelly_style = chromeos::features::IsJellyrollEnabled();
  switch (bottom_status_indicator_state_) {
    case BottomIndicatorState::kNone:
      return;
    case BottomIndicatorState::kManagedDevice: {
      if (jelly_style) {
        bottom_status_indicator_->SetIcon(chromeos::kEnterpriseIcon,
                                          cros_tokens::kCrosSysOnSurface, 20);
        bottom_status_indicator_->SetEnabledTextColorIds(
            cros_tokens::kCrosSysOnSurface);
        bottom_status_indicator_->SetImageLabelSpacing(16);
      } else {
        bottom_status_indicator_->SetIcon(chromeos::kEnterpriseIcon,
                                          kColorAshIconColorPrimary);
        bottom_status_indicator_->SetEnabledTextColorIds(
            kColorAshTextColorPrimary);
      }
      break;
    }
    case BottomIndicatorState::kAdbSideLoadingEnabled: {
      if (jelly_style) {
        bottom_status_indicator_->SetIcon(kLockScreenAlertIcon,
                                          cros_tokens::kCrosSysError, 20);
        bottom_status_indicator_->SetEnabledTextColorIds(
            cros_tokens::kCrosSysError);
        bottom_status_indicator_->SetImageLabelSpacing(16);
      } else {
        bottom_status_indicator_->SetIcon(kLockScreenAlertIcon,
                                          kColorAshIconColorAlert);
        bottom_status_indicator_->SetEnabledTextColorIds(
            kColorAshTextColorAlert);
      }
      break;
    }
  }
}

void LockContentsView::UpdateBottomStatusIndicatorVisibility() {
  bool visible =
      bottom_status_indicator_state_ ==
          BottomIndicatorState::kAdbSideLoadingEnabled ||
      (bottom_status_indicator_state_ == BottomIndicatorState::kManagedDevice &&
       !extension_ui_visible_);
  bottom_status_indicator_->SetVisible(visible);
}

void LockContentsView::OnBottomStatusIndicatorTapped() {
  if (bottom_status_indicator_state_ != BottomIndicatorState::kManagedDevice) {
    return;
  }

  if (base::FeatureList::IsEnabled(
          ash::features::kImprovedManagementDisclosure)) {
    ShowManagementDisclosureDialog();
  } else {
    // Fallback to original bubble if management_disclosure not enabled.
    management_bubble_->Show();
  }
}

void LockContentsView::OnBackToSigninButtonTapped() {
  // TODO(b/333882432): Remove this log after the bug fixed.
  LOG(WARNING) << "b/333882432: LockContentsView::OnBackToSigninButtonTapped";
  // Prevent starting a gaia signin in a transition state.
  session_manager::SessionState current_state =
      Shell::Get()->session_controller()->GetSessionState();
  if (current_state != session_manager::SessionState::OOBE &&
      current_state != session_manager::SessionState::LOGIN_PRIMARY) {
    LOG(WARNING) << "Back to signin button was called in an unexpected state: "
                 << static_cast<int>(current_state)
                 << " skip to call ShowGaiaSignin.";
    return;
  }
  Shell::Get()->login_screen_controller()->ShowGaiaSignin(
      /*prefilled_account=*/EmptyAccountId());
}

void LockContentsView::UpdateKioskDefaultMessageVisibility() {
  if (!kiosk_license_mode_) {
    return;
  }

  if (!kiosk_default_message_) {
    kiosk_default_message_ =
        AddChildView(std::make_unique<KioskAppDefaultMessage>());
  }

  kiosk_default_message_->SetVisible(!has_kiosk_apps_);
}

void LockContentsView::RecordAndResetPasswordAttempts(
    AuthEventsRecorder::AuthenticationOutcome outcome,
    AccountId account_id) {
  AuthEventsRecorder::Get()->OnExistingUserLoginScreenExit(
      outcome, unlock_attempt_by_user_[account_id]);
  unlock_attempt_by_user_[account_id] = 0;
}

void LockContentsView::OnPinUnlock(bool is_primary) {
  LoginBigUserView* to_update =
      is_primary ? primary_big_view_.get() : opt_secondary_big_view_.get();
  AccountId user = to_update->GetCurrentUser().basic_user_info.account_id;
  data_dispatcher_->SetPinEnabledForUser(user, true,
                                         /*available_at=*/std::nullopt);
  HideAuthErrorMessage();
}

BEGIN_METADATA(LockContentsView)
END_METADATA

}  // namespace ash
