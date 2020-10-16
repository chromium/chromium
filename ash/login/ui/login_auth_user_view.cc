// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_auth_user_view.h"

#include <map>
#include <memory>
#include <utility>

#include "ash/login/login_screen_controller.h"
#include "ash/login/resources/grit/login_resources.h"
#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/horizontal_image_sequence_animation_decoder.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_display_style.h"
#include "ash/login/ui/login_password_view.h"
#include "ash/login/ui/login_pin_input_view.h"
#include "ash/login/ui/login_pin_view.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/pin_keyboard_animation.h"
#include "ash/login/ui/pin_request_view.h"
#include "ash/login/ui/system_label_button.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/login_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/night_light/time_of_day.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/user_manager/user.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

namespace ash {
namespace {

constexpr const char kLoginAuthUserViewClassName[] = "LoginAuthUserView";

// Distance between the user view (ie, the icon and name) and other elements
const int kDistanceBetweenUserViewAndPasswordDp = 24;
const int kDistanceBetweenUserViewAndPinInputDp = 32;
const int kDistanceBetweenUserViewAndOnlineSigninDp = 24;
const int kDistanceBetweenUserViewAndChallengeResponseDp = 32;

// Distance between the password textfield and the the pin keyboard.
const int kDistanceBetweenPasswordFieldAndPinKeyboardDp = 16;

// Height of button used for switching between pin and password authentication.
const int kPinPasswordToggleButtonHeight = 32;
const int kPinPasswordToggleButtonPaddingTop = 24;

// Distance from the end of pin keyboard to the bottom of the big user view.
const int kDistanceFromPinKeyboardToBigUserViewBottomDp = 50;

// Distance from the top of the user view to the user icon.
constexpr int kDistanceFromTopOfBigUserViewToUserIconDp = 24;

constexpr SkColor kChallengeResponseSmartCardIconColor = gfx::kGoogleGrey200;
constexpr SkColor kChallengeResponseArrowBackgroundColor =
    SkColorSetARGB(0x2B, 0xFF, 0xFF, 0xFF);
constexpr SkColor kChallengeResponseErrorColor = gfx::kGoogleRed300;

// 38% opacity.
constexpr SkColor kDisabledFingerprintIconColor =
    SkColorSetA(SK_ColorWHITE, 97);

// Date time format containing only the day of the week, for example: "Tuesday".
constexpr char kDayOfWeekOnlyTimeFormat[] = "EEEE";

constexpr int kFingerprintIconSizeDp = 28;
constexpr int kResetToDefaultIconDelayMs = 1300;
constexpr base::TimeDelta kResetToDefaultMessageDelayMs =
    base::TimeDelta::FromMilliseconds(3000);
constexpr int kFingerprintIconTopSpacingDp = 20;
constexpr int kSpacingBetweenFingerprintIconAndLabelDp = 15;
constexpr int kFingerprintViewWidthDp = 204;
constexpr int kDistanceBetweenPasswordFieldAndFingerprintViewDp = 90;
constexpr int kFingerprintFailedAnimationDurationMs = 700;
constexpr int kFingerprintFailedAnimationNumFrames = 45;

constexpr base::TimeDelta kChallengeResponseResetAfterFailureDelay =
    base::TimeDelta::FromSeconds(5);
constexpr int kChallengeResponseArrowSizeDp = 48;
constexpr int kSpacingBetweenChallengeResponseArrowAndIconDp = 100;
constexpr int kSpacingBetweenChallengeResponseIconAndLabelDp = 15;
constexpr int kChallengeResponseIconSizeDp = 28;
constexpr int kDistanceBetweenPwdFieldAndChallengeResponseViewDp = 0;

constexpr int kDisabledAuthMessageVerticalBorderDp = 16;
constexpr int kDisabledAuthMessageHorizontalBorderDp = 16;
constexpr int kDisabledAuthMessageChildrenSpacingDp = 4;
constexpr int kDisabledAuthMessageTimeWidthDp = 204;
constexpr int kDisabledAuthMessageMultiprofileWidthDp = 304;
constexpr int kDisabledAuthMessageHeightDp = 98;
constexpr int kDisabledAuthMessageIconSizeDp = 24;
constexpr int kDisabledAuthMessageTitleFontSizeDeltaDp = 3;
constexpr int kDisabledAuthMessageContentsFontSizeDeltaDp = -1;
constexpr int kDisabledAuthMessageRoundedCornerRadiusDp = 8;

constexpr int kLockedTpmMessageVerticalBorderDp = 16;
constexpr int kLockedTpmMessageHorizontalBorderDp = 16;
constexpr int kLockedTpmMessageChildrenSpacingDp = 4;
constexpr int kLockedTpmMessageWidthDp = 360;
constexpr int kLockedTpmMessageHeightDp = 108;
constexpr int kLockedTpmMessageIconSizeDp = 24;
constexpr int kLockedTpmMessageDeltaDp = 0;
constexpr int kLockedTpmMessageRoundedCornerRadiusDp = 8;

constexpr int kNonEmptyWidthDp = 1;
gfx::Size SizeFromHeight(int height) {
  return gfx::Size(kNonEmptyWidthDp, height);
}

// Returns an observer that will hide |view| when it fires. The observer will
// delete itself after firing (by returning true). Make sure to call
// |observer->SetActive()| after attaching it.
ui::CallbackLayerAnimationObserver* BuildObserverToHideView(views::View* view) {
  return new ui::CallbackLayerAnimationObserver(base::BindRepeating(
      [](views::View* view,
         const ui::CallbackLayerAnimationObserver& observer) {
        // Don't hide the view if the animation is aborted, as |view| may no
        // longer be valid.
        if (observer.aborted_count())
          return true;

        view->SetVisible(false);
        return true;
      },
      view));
}

ui::CallbackLayerAnimationObserver* BuildObserverToNotifyA11yLocationChanged(
    views::View* view) {
  return new ui::CallbackLayerAnimationObserver(base::BindRepeating(
      [](views::View* view,
         const ui::CallbackLayerAnimationObserver& observer) {
        // Don't notify a11y event if the animation is aborted, as |view| may no
        // longer be valid.
        if (observer.aborted_count())
          return true;

        view->NotifyAccessibilityEvent(ax::mojom::Event::kLocationChanged,
                                       false /*send_native_event*/);
        return true;
      },
      view));
}

ui::CallbackLayerAnimationObserver* BuildObserverToNotifyA11yLocationChanged(
    LoginPinView* view) {
  return new ui::CallbackLayerAnimationObserver(base::BindRepeating(
      [](LoginPinView* view,
         const ui::CallbackLayerAnimationObserver& observer) {
        // Don't notify a11y event if the animation is aborted, as |view| may no
        // longer be valid.
        if (observer.aborted_count())
          return true;

        view->NotifyAccessibilityLocationChanged();
        return true;
      },
      view));
}

// Clears the password for the given |LoginPasswordView| instance, hides it, and
// then deletes itself.
class ClearPasswordAndHideAnimationObserver
    : public ui::ImplicitAnimationObserver {
 public:
  explicit ClearPasswordAndHideAnimationObserver(LoginPasswordView* view)
      : password_view_(view) {}
  ~ClearPasswordAndHideAnimationObserver() override = default;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    password_view_->Reset();
    password_view_->SetVisible(false);
    delete this;
  }

 private:
  LoginPasswordView* const password_view_;

  DISALLOW_COPY_AND_ASSIGN(ClearPasswordAndHideAnimationObserver);
};

// The label shown below the fingerprint icon.
class FingerprintLabel : public views::Label {
 public:
  FingerprintLabel() {
    SetSubpixelRenderingEnabled(false);
    SetAutoColorReadabilityEnabled(false);
    SetEnabledColor(login_constants::kAuthMethodsTextColor);
    SetMultiLine(true);

    SetTextBasedOnState(FingerprintState::AVAILABLE_DEFAULT,
                        false /*can_use_pin*/);
  }

  void SetTextBasedOnAuthAttempt(bool success) {
    SetText(l10n_util::GetStringUTF16(
        success ? IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_AUTH_SUCCESS
                : IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_AUTH_FAILED));
    SetAccessibleName(l10n_util::GetStringUTF16(
        success ? IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_ACCESSIBLE_AUTH_SUCCESS
                : IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_ACCESSIBLE_AUTH_FAILED));
  }

  void SetTextBasedOnState(FingerprintState state, bool can_use_pin) {
    auto get_displayed_id = [&]() {
      switch (state) {
        case FingerprintState::UNAVAILABLE:
        case FingerprintState::AVAILABLE_DEFAULT:
          return IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_AVAILABLE;
        case FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING:
          return IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_TOUCH_SENSOR;
        case FingerprintState::DISABLED_FROM_ATTEMPTS:
          return IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_DISABLED_FROM_ATTEMPTS;
        case FingerprintState::DISABLED_FROM_TIMEOUT:
          if (can_use_pin)
            return IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_PIN_OR_PASSWORD_REQUIRED;
          return IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_PASSWORD_REQUIRED;
      }
      NOTREACHED();
    };

    auto get_accessible_id = [&]() {
      if (state == FingerprintState::DISABLED_FROM_ATTEMPTS)
        return IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_ACCESSIBLE_AUTH_DISABLED_FROM_ATTEMPTS;
      return get_displayed_id();
    };

    SetText(l10n_util::GetStringUTF16(get_displayed_id()));
    SetAccessibleName(l10n_util::GetStringUTF16(get_accessible_id()));
  }

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = ax::mojom::Role::kStaticText;
    node_data->SetName(accessible_name_);
  }

 private:
  void SetAccessibleName(const base::string16& name) {
    accessible_name_ = name;
    NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged,
                             true /*send_native_event*/);
  }

  base::string16 accessible_name_;

  DISALLOW_COPY_AND_ASSIGN(FingerprintLabel);
};

// The content needed to render the disabled auth message view.
struct LockScreenMessage {
  base::string16 title;
  base::string16 content;
  const gfx::VectorIcon* icon;
};

// Returns the message used when the device was locked due to a time window
// limit.
LockScreenMessage GetWindowLimitMessage(const base::Time& unlock_time,
                                        bool use_24hour_clock) {
  LockScreenMessage message;
  message.title = l10n_util::GetStringUTF16(IDS_ASH_LOGIN_TIME_FOR_BED_MESSAGE);

  base::Time local_midnight = base::Time::Now().LocalMidnight();

  base::string16 time_to_display;
  if (use_24hour_clock) {
    time_to_display = base::TimeFormatTimeOfDayWithHourClockType(
        unlock_time, base::k24HourClock, base::kDropAmPm);
  } else {
    time_to_display = base::TimeFormatTimeOfDayWithHourClockType(
        unlock_time, base::k12HourClock, base::kKeepAmPm);
  }

  if (unlock_time < local_midnight + base::TimeDelta::FromDays(1)) {
    // Unlock time is today.
    message.content = l10n_util::GetStringFUTF16(
        IDS_ASH_LOGIN_COME_BACK_MESSAGE, time_to_display);
  } else if (unlock_time < local_midnight + base::TimeDelta::FromDays(2)) {
    // Unlock time is tomorrow.
    message.content = l10n_util::GetStringFUTF16(
        IDS_ASH_LOGIN_COME_BACK_TOMORROW_MESSAGE, time_to_display);
  } else {
    message.content = l10n_util::GetStringFUTF16(
        IDS_ASH_LOGIN_COME_BACK_DAY_OF_WEEK_MESSAGE,
        base::TimeFormatWithPattern(unlock_time, kDayOfWeekOnlyTimeFormat),
        time_to_display);
  }
  message.icon = &kLockScreenTimeLimitMoonIcon;
  return message;
}

// Returns the message used when the device was locked due to a time usage
// limit.
LockScreenMessage GetUsageLimitMessage(const base::TimeDelta& used_time) {
  LockScreenMessage message;

  // 1 minute is used instead of 0, because the device is used for a few
  // milliseconds before locking.
  if (used_time < base::TimeDelta::FromMinutes(1)) {
    // The device was locked all day.
    message.title = l10n_util::GetStringUTF16(IDS_ASH_LOGIN_TAKE_BREAK_MESSAGE);
    message.content =
        l10n_util::GetStringUTF16(IDS_ASH_LOGIN_LOCKED_ALL_DAY_MESSAGE);
  } else {
    // The usage limit is over.
    message.title = l10n_util::GetStringUTF16(IDS_ASH_LOGIN_TIME_IS_UP_MESSAGE);

    // TODO(933973): Stop displaying the hours part of the string when duration
    // is less than 1 hour. Example: change "0 hours, 7 minutes" to "7 minutes".
    base::string16 used_time_string;
    if (!base::TimeDurationFormat(
            used_time, base::DurationFormatWidth::DURATION_WIDTH_WIDE,
            &used_time_string)) {
      LOG(ERROR) << "Failed to generate time duration string.";
      return message;
    }

    message.content = l10n_util::GetStringFUTF16(
        IDS_ASH_LOGIN_SCREEN_TIME_USED_MESSAGE, used_time_string);
  }
  message.icon = &kLockScreenTimeLimitTimerIcon;
  return message;
}

// Returns the message used when the device was locked due to a time limit
// override.
LockScreenMessage GetOverrideMessage() {
  LockScreenMessage message;
  message.title =
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_TIME_FOR_A_BREAK_MESSAGE);
  message.content =
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_MANUAL_LOCK_MESSAGE);
  message.icon = &kLockScreenTimeLimitLockIcon;
  return message;
}

LockScreenMessage GetLockScreenMessage(AuthDisabledReason lock_reason,
                                       const base::Time& unlock_time,
                                       const base::TimeDelta& used_time,
                                       bool use_24hour_clock) {
  switch (lock_reason) {
    case AuthDisabledReason::kTimeWindowLimit:
      return GetWindowLimitMessage(unlock_time, use_24hour_clock);
    case AuthDisabledReason::kTimeUsageLimit:
      return GetUsageLimitMessage(used_time);
    case AuthDisabledReason::kTimeLimitOverride:
      return GetOverrideMessage();
    default:
      NOTREACHED();
  }
}

}  // namespace

// Consists of fingerprint icon view and a label.
class LoginAuthUserView::FingerprintView : public views::View {
 public:
  FingerprintView() {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetBorder(views::CreateEmptyBorder(kFingerprintIconTopSpacingDp, 0, 0, 0));

    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(),
        kSpacingBetweenFingerprintIconAndLabelDp));
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);

    icon_ = new AnimatedRoundedImageView(
        gfx::Size(kFingerprintIconSizeDp, kFingerprintIconSizeDp),
        0 /*corner_radius*/);
    icon_->SetImage(gfx::CreateVectorIcon(
        kLockScreenFingerprintIcon, kFingerprintIconSizeDp, SK_ColorWHITE));
    AddChildView(icon_);

    label_ = new FingerprintLabel();
    AddChildView(label_);

    DisplayCurrentState();
  }

  ~FingerprintView() override = default;

  void SetState(FingerprintState state) {
    if (state_ == state)
      return;

    reset_state_.Stop();
    state_ = state;

    DisplayCurrentState();

    if (ShouldFireChromeVoxAlert(state))
      FireAlert();
  }

  void SetCanUsePin(bool value) {
    if (can_use_pin_ == value)
      return;

    can_use_pin_ = value;
    label_->SetTextBasedOnState(state_, can_use_pin_);
  }

  void NotifyFingerprintAuthResult(bool success) {
    reset_state_.Stop();
    label_->SetTextBasedOnAuthAttempt(success);

    if (success) {
      icon_->SetImage(gfx::CreateVectorIcon(kLockScreenFingerprintSuccessIcon,
                                            kFingerprintIconSizeDp,
                                            gfx::kGoogleGreen300));
    } else {
      SetIcon(FingerprintState::DISABLED_FROM_ATTEMPTS);
      // base::Unretained is safe because reset_state_ is owned by |this|.
      reset_state_.Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kResetToDefaultIconDelayMs),
          base::BindOnce(&FingerprintView::DisplayCurrentState,
                         base::Unretained(this)));

      FireAlert();
    }
  }

  // views::View:
  gfx::Size CalculatePreferredSize() const override {
    gfx::Size size = views::View::CalculatePreferredSize();
    size.set_width(kFingerprintViewWidthDp);
    return size;
  }

  // views::View:
  void OnGestureEvent(ui::GestureEvent* event) override {
    if (event->type() != ui::ET_GESTURE_TAP)
      return;
    if (state_ == FingerprintState::AVAILABLE_DEFAULT ||
        state_ == FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING) {
      SetState(FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING);
      reset_state_.Start(
          FROM_HERE, kResetToDefaultMessageDelayMs,
          base::BindOnce(&FingerprintView::SetState, base::Unretained(this),
                         FingerprintState::AVAILABLE_DEFAULT));
    }
  }

 private:
  void DisplayCurrentState() {
    SetVisible(state_ != FingerprintState::UNAVAILABLE);
    SetIcon(state_);
    label_->SetTextBasedOnState(state_, can_use_pin_);
  }

  void FireAlert() {
    label_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert,
                                     true /*send_native_event*/);
  }

  void SetIcon(FingerprintState state) {
    const SkColor color =
        (state == FingerprintState::AVAILABLE_DEFAULT ||
                 state == FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING
             ? SK_ColorWHITE
             : kDisabledFingerprintIconColor);
    switch (state) {
      case FingerprintState::UNAVAILABLE:
      case FingerprintState::AVAILABLE_DEFAULT:
      case FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING:
      case FingerprintState::DISABLED_FROM_TIMEOUT:
        icon_->SetImage(gfx::CreateVectorIcon(kLockScreenFingerprintIcon,
                                              kFingerprintIconSizeDp, color));
        break;
      case FingerprintState::DISABLED_FROM_ATTEMPTS:
        icon_->SetAnimationDecoder(
            std::make_unique<HorizontalImageSequenceAnimationDecoder>(
                *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                    IDR_LOGIN_FINGERPRINT_UNLOCK_SPINNER),
                base::TimeDelta::FromMilliseconds(
                    kFingerprintFailedAnimationDurationMs),
                kFingerprintFailedAnimationNumFrames),
            AnimatedRoundedImageView::Playback::kSingle);
        break;
    }
  }

  bool ShouldFireChromeVoxAlert(FingerprintState state) {
    return state == FingerprintState::DISABLED_FROM_ATTEMPTS ||
           state == FingerprintState::DISABLED_FROM_TIMEOUT;
  }

  FingerprintLabel* label_ = nullptr;
  AnimatedRoundedImageView* icon_ = nullptr;
  base::OneShotTimer reset_state_;
  FingerprintState state_ = FingerprintState::AVAILABLE_DEFAULT;

  // Affects DISABLED_FROM_TIMEOUT message.
  bool can_use_pin_ = false;

  DISALLOW_COPY_AND_ASSIGN(FingerprintView);
};

// Consists of challenge-response icon view and a label.
class LoginAuthUserView::ChallengeResponseView : public views::View,
                                                 public views::ButtonListener {
 public:
  enum class State { kInitial, kAuthenticating, kFailure };

  explicit ChallengeResponseView(base::RepeatingClosure on_start_tap)
      : on_start_tap_(std::move(on_start_tap)) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);

    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    auto arrow_button_view = std::make_unique<ArrowButtonView>(
        /*listener=*/this, kChallengeResponseArrowSizeDp);
    arrow_button_view->SetInstallFocusRingOnFocus(true);
    views::InstallCircleHighlightPathGenerator(arrow_button_view.get());
    arrow_button_ = AddChildView(std::move(arrow_button_view));
    arrow_button_->SetBackgroundColor(kChallengeResponseArrowBackgroundColor);
    arrow_button_->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_ASH_LOGIN_START_SMART_CARD_AUTH_BUTTON_ACCESSIBLE_NAME));

    arrow_to_icon_spacer_ = AddChildView(std::make_unique<NonAccessibleView>());
    arrow_to_icon_spacer_->SetPreferredSize(
        gfx::Size(0, kSpacingBetweenChallengeResponseArrowAndIconDp));

    icon_ = AddChildView(std::make_unique<views::ImageView>());
    icon_->SetImage(GetImageForIcon());

    auto* icon_to_label_spacer =
        AddChildView(std::make_unique<NonAccessibleView>());
    icon_to_label_spacer->SetPreferredSize(
        gfx::Size(0, kSpacingBetweenChallengeResponseIconAndLabelDp));

    label_ = AddChildView(std::make_unique<views::Label>(
        GetTextForLabel(), views::style::CONTEXT_LABEL,
        views::style::STYLE_PRIMARY));
    label_->SetAutoColorReadabilityEnabled(false);
    label_->SetEnabledColor(login_constants::kAuthMethodsTextColor);
    label_->SetSubpixelRenderingEnabled(false);
    label_->SetFontList(views::Label::GetDefaultFontList().Derive(
        /*size_delta=*/1, gfx::Font::FontStyle::ITALIC,
        gfx::Font::Weight::NORMAL));
  }

  ~ChallengeResponseView() override = default;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    if (sender == arrow_button_) {
      // Ignore further clicks while handling the previous one.
      if (state_ != State::kAuthenticating)
        on_start_tap_.Run();
    } else {
      NOTREACHED();
    }
  }

  void SetState(State state) {
    if (state_ == state)
      return;
    state_ = state;

    reset_state_timer_.Stop();
    if (state == State::kFailure) {
      reset_state_timer_.Start(
          FROM_HERE, kChallengeResponseResetAfterFailureDelay,
          base::BindOnce(&ChallengeResponseView::SetState,
                         base::Unretained(this), State::kInitial));
    }

    arrow_button_->EnableLoadingAnimation(state == State::kAuthenticating);
    icon_->SetImage(GetImageForIcon());
    label_->SetText(GetTextForLabel());

    if (state == State::kFailure) {
      label_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert,
                                       /*send_native_event=*/true);
    }

    Layout();
  }

  void RequestFocus() override { arrow_button_->RequestFocus(); }

  views::Button* GetButtonForTesting() { return arrow_button_; }
  views::Label* GetLabelForTesting() { return label_; }

 private:
  gfx::ImageSkia GetImageForIcon() const {
    switch (state_) {
      case State::kInitial:
      case State::kAuthenticating:
        return gfx::CreateVectorIcon(kLockScreenSmartCardIcon,
                                     kChallengeResponseIconSizeDp,
                                     kChallengeResponseSmartCardIconColor);
      case State::kFailure:
        return gfx::CreateVectorIcon(kLockScreenSmartCardFailureIcon,
                                     kChallengeResponseIconSizeDp,
                                     kChallengeResponseErrorColor);
    }
  }

  base::string16 GetTextForLabel() const {
    switch (state_) {
      case State::kInitial:
      case State::kAuthenticating:
        return l10n_util::GetStringUTF16(
            IDS_ASH_LOGIN_SMART_CARD_SIGN_IN_MESSAGE);
      case State::kFailure:
        return l10n_util::GetStringUTF16(
            IDS_ASH_LOGIN_SMART_CARD_SIGN_IN_FAILURE_MESSAGE);
    }
  }

  base::RepeatingClosure on_start_tap_;
  State state_ = State::kInitial;
  ArrowButtonView* arrow_button_ = nullptr;
  NonAccessibleView* arrow_to_icon_spacer_ = nullptr;
  views::ImageView* icon_ = nullptr;
  views::Label* label_ = nullptr;
  base::OneShotTimer reset_state_timer_;

  DISALLOW_COPY_AND_ASSIGN(ChallengeResponseView);
};

// The message shown to user when the auth method is |AUTH_DISABLED|.
class LoginAuthUserView::DisabledAuthMessageView : public views::View {
 public:
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(DisabledAuthMessageView* view) : view_(view) {}
    ~TestApi() = default;

    const base::string16& GetDisabledAuthMessageContent() const {
      return view_->message_contents_->GetText();
    }

   private:
    DisabledAuthMessageView* const view_;
  };

  // If the reason of disabled auth is multiprofile policy, then we can already
  // set the text and message. Otherwise, in case of disabled auth because of
  // time limit exceeded on child account, we wait for SetAuthDisabledMessage to
  // be called.
  DisabledAuthMessageView(bool shown_because_of_multiprofile_policy,
                          MultiProfileUserBehavior multiprofile_policy)
      : shown_because_of_multiprofile_policy_(
            shown_because_of_multiprofile_policy) {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        gfx::Insets(kDisabledAuthMessageVerticalBorderDp,
                    kDisabledAuthMessageHorizontalBorderDp),
        kDisabledAuthMessageChildrenSpacingDp));
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetPreferredSize(gfx::Size(shown_because_of_multiprofile_policy
                                   ? kDisabledAuthMessageMultiprofileWidthDp
                                   : kDisabledAuthMessageTimeWidthDp,
                               kDisabledAuthMessageHeightDp));
    SetFocusBehavior(FocusBehavior::ALWAYS);
    if (!shown_because_of_multiprofile_policy) {
      message_icon_ = new views::ImageView();
      message_icon_->SetPreferredSize(gfx::Size(
          kDisabledAuthMessageIconSizeDp, kDisabledAuthMessageIconSizeDp));
      message_icon_->SetImage(
          gfx::CreateVectorIcon(kLockScreenTimeLimitMoonIcon,
                                kDisabledAuthMessageIconSizeDp, SK_ColorWHITE));
      AddChildView(message_icon_);
    }

    auto decorate_label = [](views::Label* label) {
      label->SetSubpixelRenderingEnabled(false);
      label->SetAutoColorReadabilityEnabled(false);
      label->SetEnabledColor(SK_ColorWHITE);
      label->SetFocusBehavior(FocusBehavior::ALWAYS);
    };
    message_title_ =
        new views::Label(base::string16(), views::style::CONTEXT_LABEL,
                         views::style::STYLE_PRIMARY);
    message_title_->SetFontList(
        gfx::FontList().Derive(kDisabledAuthMessageTitleFontSizeDeltaDp,
                               gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
    decorate_label(message_title_);
    AddChildView(message_title_);

    message_contents_ =
        new views::Label(base::string16(), views::style::CONTEXT_LABEL,
                         views::style::STYLE_PRIMARY);
    message_contents_->SetFontList(
        gfx::FontList().Derive(kDisabledAuthMessageContentsFontSizeDeltaDp,
                               gfx::Font::NORMAL, gfx::Font::Weight::NORMAL));
    decorate_label(message_contents_);
    message_contents_->SetMultiLine(true);
    AddChildView(message_contents_);

    if (shown_because_of_multiprofile_policy) {
      message_title_->SetText(l10n_util::GetStringUTF16(
          IDS_ASH_LOGIN_MULTI_PROFILES_RESTRICTED_POLICY_TITLE));
      switch (multiprofile_policy) {
        case MultiProfileUserBehavior::PRIMARY_ONLY:
          message_contents_->SetText(l10n_util::GetStringUTF16(
              IDS_ASH_LOGIN_MULTI_PROFILES_PRIMARY_ONLY_POLICY_MSG));
          break;
        case MultiProfileUserBehavior::NOT_ALLOWED:
          message_contents_->SetText(l10n_util::GetStringUTF16(
              IDS_ASH_LOGIN_MULTI_PROFILES_NOT_ALLOWED_POLICY_MSG));
          break;
        case MultiProfileUserBehavior::OWNER_PRIMARY_ONLY:
          message_contents_->SetText(l10n_util::GetStringUTF16(
              IDS_ASH_LOGIN_MULTI_PROFILES_OWNER_PRIMARY_ONLY_MSG));
          break;
        default:
          NOTREACHED();
      }
    }
  }

  ~DisabledAuthMessageView() override = default;

  // Set the parameters needed to render the message.
  void SetAuthDisabledMessage(const AuthDisabledData& auth_disabled_data,
                              bool use_24hour_clock) {
    // Do not do anything if message is already shown.
    if (shown_because_of_multiprofile_policy_)
      return;
    LockScreenMessage message = GetLockScreenMessage(
        auth_disabled_data.reason, auth_disabled_data.auth_reenabled_time,
        auth_disabled_data.device_used_time, use_24hour_clock);
    message_icon_->SetImage(gfx::CreateVectorIcon(
        *message.icon, kDisabledAuthMessageIconSizeDp, SK_ColorWHITE));
    message_title_->SetText(message.title);
    message_contents_->SetText(message.content);
    Layout();
  }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);

    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(
        PinRequestView::GetChildUserDialogColor(false /*using blur*/));
    canvas->DrawRoundRect(GetContentsBounds(),
                          kDisabledAuthMessageRoundedCornerRadiusDp, flags);
  }
  void RequestFocus() override { message_title_->RequestFocus(); }

 private:
  views::Label* message_title_;
  views::Label* message_contents_;
  views::ImageView* message_icon_;
  // Used in case a child account has triggered the disabled auth message
  // because of time limit exceeded while it also has disabled auth by
  // multiprofile policy.
  bool shown_because_of_multiprofile_policy_ = false;

  DISALLOW_COPY_AND_ASSIGN(DisabledAuthMessageView);
};

// The message shown to user when TPM is locked.
class LoginAuthUserView::LockedTpmMessageView : public views::View {
 public:
  LockedTpmMessageView() {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        gfx::Insets(kLockedTpmMessageVerticalBorderDp,
                    kLockedTpmMessageHorizontalBorderDp),
        kLockedTpmMessageChildrenSpacingDp));
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetPreferredSize(
        gfx::Size(kLockedTpmMessageWidthDp, kLockedTpmMessageHeightDp));
    SetFocusBehavior(FocusBehavior::ALWAYS);

    auto message_icon = std::make_unique<views::ImageView>();
    message_icon->SetPreferredSize(
        gfx::Size(kLockedTpmMessageIconSizeDp, kLockedTpmMessageIconSizeDp));
    message_icon->SetImage(
        gfx::CreateVectorIcon(kLockScreenAlertIcon, SK_ColorWHITE));
    message_icon_ = AddChildView(std::move(message_icon));

    message_warning_ = CreateLabel();
    message_description_ = CreateLabel();

    // Set content.
    base::string16 message_description = l10n_util::GetStringUTF16(
        IDS_ASH_LOGIN_POD_TPM_LOCKED_ISSUE_DESCRIPTION);
    message_description_->SetText(message_description);
  }

  LockedTpmMessageView(const LockedTpmMessageView&) = delete;
  LockedTpmMessageView& operator=(const LockedTpmMessageView&) = delete;
  ~LockedTpmMessageView() override = default;

  // Set the parameters needed to render the message.
  void SetRemainingTime(base::TimeDelta time_left) {
    base::string16 time_left_message;
    if (base::TimeDurationFormatWithSeconds(
            time_left, base::DurationFormatWidth::DURATION_WIDTH_WIDE,
            &time_left_message)) {
      base::string16 message_warning = l10n_util::GetStringFUTF16(
          IDS_ASH_LOGIN_POD_TPM_LOCKED_ISSUE_WARNING, time_left_message);
      message_warning_->SetText(message_warning);

      if (time_left.InMinutes() != prev_time_left_.InMinutes()) {
        message_warning_->NotifyAccessibilityEvent(
            ax::mojom::Event::kTextChanged, true);
      }
      prev_time_left_ = time_left;
    }
  }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);

    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(
        PinRequestView::GetChildUserDialogColor(false /*using blur*/));
    canvas->DrawRoundRect(GetContentsBounds(),
                          kLockedTpmMessageRoundedCornerRadiusDp, flags);
  }
  void RequestFocus() override { message_warning_->RequestFocus(); }

 private:
  views::Label* CreateLabel() {
    auto label = std::make_unique<views::Label>(base::string16(),
                                                views::style::CONTEXT_LABEL,
                                                views::style::STYLE_PRIMARY);
    label->SetFontList(gfx::FontList().Derive(kLockedTpmMessageDeltaDp,
                                              gfx::Font::NORMAL,
                                              gfx::Font::Weight::NORMAL));
    label->SetSubpixelRenderingEnabled(false);
    label->SetAutoColorReadabilityEnabled(false);
    label->SetEnabledColor(SK_ColorWHITE);
    label->SetFocusBehavior(FocusBehavior::ALWAYS);
    label->SetMultiLine(true);
    return AddChildView(std::move(label));
  }

  base::TimeDelta prev_time_left_;
  views::Label* message_warning_;
  views::Label* message_description_;
  views::ImageView* message_icon_;
};

LoginAuthUserView::AuthMethodsMetadata::AuthMethodsMetadata() = default;
LoginAuthUserView::AuthMethodsMetadata::~AuthMethodsMetadata() = default;
LoginAuthUserView::AuthMethodsMetadata::AuthMethodsMetadata(
    const AuthMethodsMetadata&) = default;

struct LoginAuthUserView::UiState {
  explicit UiState(const LoginAuthUserView* view) {
    has_password = view->ShouldShowPasswordField();
    has_pin_input = view->ShouldShowPinInputField();
    has_pinpad = view->ShouldShowPinPad();
    has_toggle = view->ShouldShowToggle();
    has_fingerprint = view->HasAuthMethod(LoginAuthUserView::AUTH_FINGERPRINT);
    has_challenge_response =
        view->HasAuthMethod(LoginAuthUserView::AUTH_CHALLENGE_RESPONSE);
    auth_disabled = view->HasAuthMethod(LoginAuthUserView::AUTH_DISABLED);
    tpm_is_locked =
        view->HasAuthMethod(LoginAuthUserView::AUTH_DISABLED_TPM_LOCKED);
    force_online_sign_in =
        view->HasAuthMethod(LoginAuthUserView::AUTH_ONLINE_SIGN_IN);

    non_pin_y_start_in_screen = view->GetBoundsInScreen().y();
    pin_start_in_screen = view->pin_view_->GetBoundsInScreen().origin();
  }

  bool has_password = false;
  bool has_pin_input = false;
  bool has_pinpad = false;
  bool has_toggle = false;
  bool has_fingerprint = false;
  bool has_challenge_response = false;
  bool auth_disabled = false;
  bool tpm_is_locked = false;
  bool force_online_sign_in = false;
  // Used for this view's animation in `ApplyAnimationPostLayout`.
  int non_pin_y_start_in_screen = 0;
  gfx::Point pin_start_in_screen;
};

LoginAuthUserView::TestApi::TestApi(LoginAuthUserView* view) : view_(view) {}

LoginAuthUserView::TestApi::~TestApi() = default;

LoginUserView* LoginAuthUserView::TestApi::user_view() const {
  return view_->user_view_;
}

LoginPasswordView* LoginAuthUserView::TestApi::password_view() const {
  return view_->password_view_;
}

LoginPinView* LoginAuthUserView::TestApi::pin_view() const {
  return view_->pin_view_;
}

LoginPinInputView* LoginAuthUserView::TestApi::pin_input_view() const {
  return view_->pin_input_view_;
}

views::Button* LoginAuthUserView::TestApi::pin_password_toggle() const {
  return view_->pin_password_toggle_;
}

views::Button* LoginAuthUserView::TestApi::online_sign_in_message() const {
  return view_->online_sign_in_message_;
}

views::View* LoginAuthUserView::TestApi::disabled_auth_message() const {
  return view_->disabled_auth_message_;
}

views::Button* LoginAuthUserView::TestApi::challenge_response_button() {
  return view_->challenge_response_view_->GetButtonForTesting();
}

views::Label* LoginAuthUserView::TestApi::challenge_response_label() {
  return view_->challenge_response_view_->GetLabelForTesting();
}

bool LoginAuthUserView::TestApi::HasAuthMethod(AuthMethods auth_method) const {
  return view_->HasAuthMethod(auth_method);
}

const base::string16&
LoginAuthUserView::TestApi::GetDisabledAuthMessageContent() const {
  return LoginAuthUserView::DisabledAuthMessageView::TestApi(
             view_->disabled_auth_message_)
      .GetDisabledAuthMessageContent();
}

LoginAuthUserView::Callbacks::Callbacks() = default;

LoginAuthUserView::Callbacks::Callbacks(const Callbacks& other) = default;

LoginAuthUserView::Callbacks::~Callbacks() = default;

LoginAuthUserView::LoginAuthUserView(const LoginUserInfo& user,
                                     const Callbacks& callbacks)
    : NonAccessibleView(kLoginAuthUserViewClassName),
      on_auth_(callbacks.on_auth),
      on_tap_(callbacks.on_tap) {
  DCHECK(callbacks.on_auth);
  DCHECK(callbacks.on_tap);
  DCHECK(callbacks.on_remove_warning_shown);
  DCHECK(callbacks.on_remove);
  DCHECK(callbacks.on_easy_unlock_icon_hovered);
  DCHECK(callbacks.on_easy_unlock_icon_tapped);
  DCHECK_NE(user.basic_user_info.type, user_manager::USER_TYPE_PUBLIC_ACCOUNT);

  // Build child views.
  auto user_view = std::make_unique<LoginUserView>(
      LoginDisplayStyle::kLarge, true /*show_dropdown*/,
      base::BindRepeating(&LoginAuthUserView::OnUserViewTap,
                          base::Unretained(this)),
      callbacks.on_remove_warning_shown, callbacks.on_remove);
  user_view_ = user_view.get();

  const LoginPalette palette = CreateDefaultLoginPalette();

  auto password_view = std::make_unique<LoginPasswordView>(palette);
  password_view_ = password_view.get();
  password_view->SetPaintToLayer();  // Needed for opacity animation.
  password_view->layer()->SetFillsBoundsOpaquely(false);
  password_view_->SetDisplayPasswordButtonVisible(
      user.show_display_password_button);
  password_view->Init(
      base::BindRepeating(&LoginAuthUserView::OnAuthSubmit,
                          base::Unretained(this)),
      base::BindRepeating(&LoginAuthUserView::OnPasswordTextChanged,
                          base::Unretained(this)),
      callbacks.on_easy_unlock_icon_hovered,
      callbacks.on_easy_unlock_icon_tapped);

  auto pin_input_view = std::make_unique<LoginPinInputView>();
  pin_input_view_ = pin_input_view.get();
  pin_input_view->Init(base::BindRepeating(&LoginAuthUserView::OnAuthSubmit,
                                           base::Unretained(this)),
                       base::BindRepeating(&LoginAuthUserView::OnPinTextChanged,
                                           base::Unretained(this)));

  auto toggle_container = std::make_unique<NonAccessibleView>();
  toggle_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(kPinPasswordToggleButtonPaddingTop, 0, 0, 0)));
  pin_password_toggle_ =
      toggle_container->AddChildView(std::make_unique<SystemLabelButton>(
          this, GetPinPasswordToggleText(),
          SystemLabelButton::DisplayType::DEFAULT,
          /*multiline*/ false));
  pin_password_toggle_->SetMaxSize(
      gfx::Size(/*ignored*/ 0, kPinPasswordToggleButtonHeight));

  auto pin_view = std::make_unique<LoginPinView>(
      LoginPinView::Style::kAlphanumeric, palette,
      base::BindRepeating(&LoginAuthUserView::OnPinPadInsertDigit,
                          base::Unretained(this)),
      base::BindRepeating(&LoginAuthUserView::OnPinPadBackspace,
                          base::Unretained(this)));
  pin_view_ = pin_view.get();
  DCHECK(pin_view_->layer());

  auto padding_below_password_view = std::make_unique<NonAccessibleView>();
  padding_below_password_view->SetPreferredSize(gfx::Size(
      kNonEmptyWidthDp, kDistanceBetweenPasswordFieldAndPinKeyboardDp));
  padding_below_password_view_ = padding_below_password_view.get();

  auto padding_below_user_view = std::make_unique<NonAccessibleView>();
  padding_below_user_view->SetPreferredSize(
      gfx::Size(kNonEmptyWidthDp, kDistanceBetweenUserViewAndPasswordDp));
  padding_below_user_view_ = padding_below_user_view.get();

  base::string16 button_message =
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_SIGN_IN_REQUIRED_MESSAGE);
  if (user.is_signed_in) {
    button_message =
        l10n_util::GetStringUTF16(IDS_ASH_LOCK_SCREEN_VERIFY_ACCOUNT_MESSAGE);
  }
  auto online_sign_in_message = std::make_unique<SystemLabelButton>(
      this, button_message, SystemLabelButton::DisplayType::ALERT_WITH_ICON,
      /*multiline*/ false);
  online_sign_in_message_ = online_sign_in_message.get();

  bool shown_because_of_multiprofile_policy =
      !user.is_multiprofile_allowed &&
      Shell::Get()->session_controller()->GetSessionState() ==
          session_manager::SessionState::LOGIN_SECONDARY;
  auto disabled_auth_message = std::make_unique<DisabledAuthMessageView>(
      shown_because_of_multiprofile_policy, user.multiprofile_policy);
  disabled_auth_message_ = disabled_auth_message.get();

  auto locked_tpm_message_view = std::make_unique<LockedTpmMessageView>();
  locked_tpm_message_view_ = locked_tpm_message_view.get();

  auto fingerprint_view = std::make_unique<FingerprintView>();
  fingerprint_view_ = fingerprint_view.get();

  auto challenge_response_view =
      std::make_unique<ChallengeResponseView>(base::BindRepeating(
          &LoginAuthUserView::AttemptAuthenticateWithChallengeResponse,
          weak_factory_.GetWeakPtr()));
  challenge_response_view_ = challenge_response_view.get();

  SetPaintToLayer(ui::LayerType::LAYER_NOT_DRAWN);

  // Build layout.
  auto wrapped_password_view =
      login_views_utils::WrapViewForPreferredSize(std::move(password_view));
  auto wrapped_online_sign_in_message_view =
      login_views_utils::WrapViewForPreferredSize(
          std::move(online_sign_in_message));
  auto wrapped_disabled_auth_message_view =
      login_views_utils::WrapViewForPreferredSize(
          std::move(disabled_auth_message));
  auto wrapped_locked_tpm_message_view =
      login_views_utils::WrapViewForPreferredSize(
          std::move(locked_tpm_message_view));
  auto wrapped_user_view =
      login_views_utils::WrapViewForPreferredSize(std::move(user_view));
  auto wrapped_pin_view =
      login_views_utils::WrapViewForPreferredSize(std::move(pin_view));
  auto wrapped_pin_input_view =
      login_views_utils::WrapViewForPreferredSize(std::move(pin_input_view));
  auto wrapped_pin_password_toggle_view =
      login_views_utils::WrapViewForPreferredSize(std::move(toggle_container));
  auto wrapped_fingerprint_view =
      login_views_utils::WrapViewForPreferredSize(std::move(fingerprint_view));
  auto wrapped_challenge_response_view =
      login_views_utils::WrapViewForPreferredSize(
          std::move(challenge_response_view));
  auto wrapped_padding_below_password_view =
      login_views_utils::WrapViewForPreferredSize(
          std::move(padding_below_password_view));
  auto wrapped_padding_below_user_view =
      login_views_utils::WrapViewForPreferredSize(
          std::move(padding_below_user_view));

  // Add views in tabbing order; they are rendered in a different order below.
  views::View* wrapped_password_view_ptr =
      AddChildView(std::move(wrapped_password_view));
  views::View* wrapped_online_sign_in_message_view_ptr =
      AddChildView(std::move(wrapped_online_sign_in_message_view));
  views::View* wrapped_disabled_auth_message_view_ptr =
      AddChildView(std::move(wrapped_disabled_auth_message_view));
  views::View* wrapped_locked_tpm_message_view_ptr =
      AddChildView(std::move(wrapped_locked_tpm_message_view));
  views::View* wrapped_pin_input_view_ptr =
      AddChildView(std::move(wrapped_pin_input_view));
  views::View* wrapped_pin_view_ptr = AddChildView(std::move(wrapped_pin_view));
  views::View* wrapped_pin_password_toggle_view_ptr =
      AddChildView(std::move(wrapped_pin_password_toggle_view));
  views::View* wrapped_fingerprint_view_ptr =
      AddChildView(std::move(wrapped_fingerprint_view));
  views::View* wrapped_challenge_response_view_ptr =
      AddChildView(std::move(wrapped_challenge_response_view));
  views::View* wrapped_user_view_ptr =
      AddChildView(std::move(wrapped_user_view));
  views::View* wrapped_padding_below_password_view_ptr =
      AddChildView(std::move(wrapped_padding_below_password_view));
  views::View* wrapped_padding_below_user_view_ptr =
      AddChildView(std::move(wrapped_padding_below_user_view));

  // Use views::GridLayout instead of views::BoxLayout because views::BoxLayout
  // lays out children according to the view->children order.
  views::GridLayout* grid_layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* column_set = grid_layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::CENTER, views::GridLayout::LEADING,
                        0 /*resize_percent*/,
                        views::GridLayout::ColumnSize::kUsePreferred,
                        0 /*fixed_width*/, 0 /*min_width*/);
  auto add_view = [&](views::View* view) {
    grid_layout->StartRow(0 /*vertical_resize*/, 0 /*column_set_id*/);
    grid_layout->AddExistingView(view);
  };
  auto add_padding = [&](int amount) {
    grid_layout->AddPaddingRow(0 /*vertical_resize*/, amount /*size*/);
  };

  // Add views in rendering order.
  add_padding(kDistanceFromTopOfBigUserViewToUserIconDp);
  add_view(wrapped_user_view_ptr);
  add_view(wrapped_padding_below_user_view_ptr);
  add_view(wrapped_locked_tpm_message_view_ptr);
  add_view(wrapped_password_view_ptr);
  add_view(wrapped_online_sign_in_message_view_ptr);
  add_view(wrapped_disabled_auth_message_view_ptr);
  add_view(wrapped_pin_input_view_ptr);
  add_view(wrapped_padding_below_password_view_ptr);
  add_view(wrapped_pin_view_ptr);
  add_view(wrapped_pin_password_toggle_view_ptr);
  add_view(wrapped_fingerprint_view_ptr);
  add_view(wrapped_challenge_response_view_ptr);
  add_padding(kDistanceFromPinKeyboardToBigUserViewBottomDp);

  // Update authentication UI.
  CaptureStateForAnimationPreLayout();
  SetAuthMethods(auth_methods_);
  ApplyAnimationPostLayout(/*animate*/ false);
  user_view_->UpdateForUser(user, /*animate*/ false);
}

LoginAuthUserView::~LoginAuthUserView() = default;

void LoginAuthUserView::SetAuthMethods(
    uint32_t auth_methods,
    const AuthMethodsMetadata& auth_metadata) {
  // It is an error to call this method without storing the previous state.
  DCHECK(previous_state_);

  // Apply changes and determine the new state of input fields.
  auth_methods_ = static_cast<AuthMethods>(auth_methods);
  auth_metadata_ = auth_metadata;
  UpdateInputFieldMode();
  const UiState current_state{this};

  online_sign_in_message_->SetVisible(current_state.force_online_sign_in);
  disabled_auth_message_->SetVisible(current_state.auth_disabled);
  locked_tpm_message_view_->SetVisible(current_state.tpm_is_locked);
  if (current_state.tpm_is_locked &&
      auth_metadata.time_until_tpm_unlock.has_value())
    locked_tpm_message_view_->SetRemainingTime(
        auth_metadata.time_until_tpm_unlock.value());

  // Adjust the PIN keyboard visibility before the password textfield's one, so
  // that when both are about to be hidden the focus doesn't jump to the "1"
  // keyboard button, causing unexpected accessibility effects.
  pin_view_->SetVisible(current_state.has_pinpad);

  password_view_->SetEnabled(current_state.has_password);
  password_view_->SetEnabledOnEmptyPassword(HasAuthMethod(AUTH_TAP));
  password_view_->SetFocusEnabledForChildViews(current_state.has_password);
  password_view_->SetVisible(current_state.has_password);
  password_view_->layer()->SetOpacity(current_state.has_password ? 1 : 0);

  pin_input_view_->UpdateLength(auth_metadata_.autosubmit_pin_length);
  pin_input_view_->SetVisible(current_state.has_pin_input);

  pin_password_toggle_->SetVisible(current_state.has_toggle);
  pin_password_toggle_->SetText(GetPinPasswordToggleText());

  fingerprint_view_->SetVisible(current_state.has_fingerprint);
  fingerprint_view_->SetCanUsePin(HasAuthMethod(AUTH_PIN));
  challenge_response_view_->SetVisible(current_state.has_challenge_response);

  padding_below_user_view_->SetPreferredSize(GetPaddingBelowUserView());
  padding_below_password_view_->SetPreferredSize(GetPaddingBelowPasswordView());

  password_view_->SetPlaceholderText(GetPasswordViewPlaceholder());
  const std::string& user_display_email =
      current_user().basic_user_info.display_email;
  password_view_->SetAccessibleName(l10n_util::GetStringFUTF16(
      IDS_ASH_LOGIN_POD_PASSWORD_FIELD_ACCESSIBLE_NAME,
      base::UTF8ToUTF16(user_display_email)));

  // Only the active auth user view has authentication methods. If that is the
  // case, then render the user view as if it was always focused, since clicking
  // on it will not do anything (such as swapping users).
  user_view_->SetForceOpaque(auth_methods_ != AUTH_NONE);
  user_view_->SetTapEnabled(auth_methods_ == AUTH_NONE);

  UpdateFocus();
  PreferredSizeChanged();
}

void LoginAuthUserView::SetEasyUnlockIcon(
    EasyUnlockIconId id,
    const base::string16& accessibility_label) {
  password_view_->SetEasyUnlockIcon(id, accessibility_label);

  const std::string& user_display_email =
      current_user().basic_user_info.display_email;
  if (id == EasyUnlockIconId::UNLOCKED) {
    password_view_->SetAccessibleName(l10n_util::GetStringFUTF16(
        IDS_ASH_LOGIN_POD_AUTH_TAP_PASSWORD_FIELD_ACCESSIBLE_NAME,
        base::UTF8ToUTF16(user_display_email)));
  } else {
    password_view_->SetAccessibleName(l10n_util::GetStringFUTF16(
        IDS_ASH_LOGIN_POD_PASSWORD_FIELD_ACCESSIBLE_NAME,
        base::UTF8ToUTF16(user_display_email)));
  }
}

void LoginAuthUserView::CaptureStateForAnimationPreLayout() {
  auto stop_animation = [](views::View* view) {
    if (view->layer()->GetAnimator()->is_animating())
      view->layer()->GetAnimator()->StopAnimating();
  };

  // Stop any running animation scheduled in ApplyAnimationPostLayout.
  stop_animation(this);
  stop_animation(password_view_);
  stop_animation(pin_view_);
  stop_animation(fingerprint_view_);
  stop_animation(challenge_response_view_);
  stop_animation(pin_password_toggle_);

  DCHECK(!previous_state_);
  previous_state_ = std::make_unique<UiState>(this);
}

void LoginAuthUserView::ApplyAnimationPostLayout(bool animate) {
  DCHECK(previous_state_);
  // Release the previous state if no animation should be performed.
  if (!animate) {
    previous_state_.reset();
    return;
  }

  const UiState current_state{this};

  ////////
  // Animate the user info (ie, icon, name) up or down the screen.
  {
    int non_pin_y_end_in_screen = GetBoundsInScreen().y();

    // Transform the layer so the user view renders where it used to be. This
    // requires a y offset.
    // Note: Doing this animation via ui::ScopedLayerAnimationSettings works,
    // but it seems that the timing gets slightly out of sync with the PIN
    // animation.
    auto move_to_center = std::make_unique<ui::InterpolatedTranslation>(
        gfx::PointF(0, previous_state_->non_pin_y_start_in_screen -
                           non_pin_y_end_in_screen),
        gfx::PointF());
    auto transition =
        ui::LayerAnimationElement::CreateInterpolatedTransformElement(
            std::move(move_to_center),
            base::TimeDelta::FromMilliseconds(
                login_constants::kChangeUserAnimationDurationMs));
    transition->set_tween_type(gfx::Tween::Type::FAST_OUT_SLOW_IN);
    auto* sequence = new ui::LayerAnimationSequence(std::move(transition));
    auto* observer = BuildObserverToNotifyA11yLocationChanged(this);
    sequence->AddObserver(observer);
    observer->SetActive();
    layer()->GetAnimator()->StartAnimation(sequence);
  }

  ////////
  // Fade the password view if it is being hidden or shown.

  if (current_state.has_password != previous_state_->has_password) {
    float opacity_start = 0, opacity_end = 1;
    if (!current_state.has_password)
      std::swap(opacity_start, opacity_end);

    password_view_->layer()->SetOpacity(opacity_start);

    {
      ui::ScopedLayerAnimationSettings settings(
          password_view_->layer()->GetAnimator());
      settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
          login_constants::kChangeUserAnimationDurationMs));
      settings.SetTweenType(gfx::Tween::Type::FAST_OUT_SLOW_IN);
      if (previous_state_->has_password && !current_state.has_password) {
        settings.AddObserver(
            new ClearPasswordAndHideAnimationObserver(password_view_));
      }

      password_view_->layer()->SetOpacity(opacity_end);
    }
  }

  ////////
  // Fade the pin/pwd toggle if its being hidden or shown.
  if (previous_state_->has_toggle != current_state.has_toggle) {
    float opacity_start = 0, opacity_end = 1;
    if (!current_state.has_toggle)
      std::swap(opacity_start, opacity_end);

    pin_password_toggle_->layer()->SetOpacity(opacity_start);

    {
      ui::ScopedLayerAnimationSettings settings(
          pin_password_toggle_->layer()->GetAnimator());
      settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
          login_constants::kChangeUserAnimationDurationMs));
      settings.SetTweenType(gfx::Tween::Type::FAST_OUT_SLOW_IN);
      pin_password_toggle_->layer()->SetOpacity(opacity_end);
    }
  }

  ////////
  // Grow/shrink the PIN keyboard if it is being hidden or shown.

  if (previous_state_->has_pinpad != current_state.has_pinpad) {
    if (!current_state.has_pinpad) {
      gfx::Point pin_end_in_screen = pin_view_->GetBoundsInScreen().origin();
      gfx::Rect pin_bounds = pin_view_->bounds();
      pin_bounds.set_x(previous_state_->pin_start_in_screen.x() -
                       pin_end_in_screen.x());
      pin_bounds.set_y(previous_state_->pin_start_in_screen.y() -
                       pin_end_in_screen.y());

      // Since PIN is disabled, the previous Layout() hid the PIN keyboard.
      // We need to redisplay it where it used to be.
      // pin_view_->SetVisible(true);
      pin_view_->SetBoundsRect(pin_bounds);
    }

    auto transition = std::make_unique<PinKeyboardAnimation>(
        current_state.has_pinpad /*grow*/, pin_view_->height(),
        // TODO(https://crbug.com/955119): Implement proper animation.
        base::TimeDelta::FromMilliseconds(
            login_constants::kChangeUserAnimationDurationMs / 2.0f),
        gfx::Tween::FAST_OUT_SLOW_IN);
    auto* sequence = new ui::LayerAnimationSequence(std::move(transition));

    // Hide the PIN keyboard after animation if needed.
    if (!current_state.has_pinpad) {
      auto* observer = BuildObserverToHideView(pin_view_);
      sequence->AddObserver(observer);
      observer->SetActive();
    }
    auto* observer = BuildObserverToNotifyA11yLocationChanged(pin_view_);
    sequence->AddObserver(observer);
    observer->SetActive();
    pin_view_->layer()->GetAnimator()->ScheduleAnimation(sequence);
  }

  ////////
  // Fade the fingerprint view if it is being hidden or shown.

  if (previous_state_->has_fingerprint != current_state.has_fingerprint) {
    float opacity_start = 0, opacity_end = 1;
    if (!current_state.has_fingerprint)
      std::swap(opacity_start, opacity_end);

    fingerprint_view_->layer()->SetOpacity(opacity_start);

    {
      ui::ScopedLayerAnimationSettings settings(
          fingerprint_view_->layer()->GetAnimator());
      settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
          login_constants::kChangeUserAnimationDurationMs));
      settings.SetTweenType(gfx::Tween::Type::FAST_OUT_SLOW_IN);
      fingerprint_view_->layer()->SetOpacity(opacity_end);
    }
  }

  ////////
  // Fade the challenge response (Smart Card) if it is being hidden or shown.
  if (previous_state_->has_challenge_response !=
      current_state.has_challenge_response) {
    float opacity_start = 0, opacity_end = 1;
    if (!current_state.has_challenge_response)
      std::swap(opacity_start, opacity_end);

    challenge_response_view_->layer()->SetOpacity(opacity_start);

    {
      ui::ScopedLayerAnimationSettings settings(
          challenge_response_view_->layer()->GetAnimator());
      settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
          login_constants::kChangeUserAnimationDurationMs));
      settings.SetTweenType(gfx::Tween::Type::FAST_OUT_SLOW_IN);
      challenge_response_view_->layer()->SetOpacity(opacity_end);
    }
  }

  previous_state_.reset();
}

void LoginAuthUserView::UpdateForUser(const LoginUserInfo& user) {
  const bool user_changed = current_user().basic_user_info.account_id !=
                            user.basic_user_info.account_id;
  user_view_->UpdateForUser(user, true /*animate*/);
  if (user_changed) {
    password_view_->Reset();
    password_view_->SetDisplayPasswordButtonVisible(
        user.show_display_password_button);
  }
  online_sign_in_message_->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_SIGN_IN_REQUIRED_MESSAGE));
}

void LoginAuthUserView::SetFingerprintState(FingerprintState state) {
  fingerprint_view_->SetState(state);
}

void LoginAuthUserView::NotifyFingerprintAuthResult(bool success) {
  fingerprint_view_->NotifyFingerprintAuthResult(success);
}

void LoginAuthUserView::SetAuthDisabledMessage(
    const AuthDisabledData& auth_disabled_data) {
  disabled_auth_message_->SetAuthDisabledMessage(
      auth_disabled_data, current_user().use_24hour_clock);
  Layout();
}

const LoginUserInfo& LoginAuthUserView::current_user() const {
  return user_view_->current_user();
}

views::View* LoginAuthUserView::GetActiveInputView() {
  if (input_field_mode_ == InputFieldMode::PIN_WITH_TOGGLE)
    return pin_input_view_;

  return password_view_;
}

gfx::Size LoginAuthUserView::CalculatePreferredSize() const {
  gfx::Size size = views::View::CalculatePreferredSize();
  // Make sure we are at least as big as the user view. If we do not do this the
  // view will be below minimum size when no auth methods are displayed.
  size.SetToMax(user_view_->GetPreferredSize());
  return size;
}

void LoginAuthUserView::RequestFocus() {
  if (input_field_mode_ == InputFieldMode::PIN_WITH_TOGGLE)
    pin_input_view_->RequestFocus();
  else
    password_view_->RequestFocus();
}

void LoginAuthUserView::ButtonPressed(views::Button* sender,
                                      const ui::Event& event) {
  DCHECK(sender == online_sign_in_message_ || sender == pin_password_toggle_);
  if (sender == online_sign_in_message_) {
    OnOnlineSignInMessageTap();
  } else if (sender == pin_password_toggle_) {
    OnSwitchButtonClicked();
  }
}

void LoginAuthUserView::OnAuthSubmit(const base::string16& password) {
  // Pressing enter when the password field is empty and tap-to-unlock is
  // enabled should attempt unlock.
  if (HasAuthMethod(AUTH_TAP) && password.empty()) {
    Shell::Get()->login_screen_controller()->AuthenticateUserWithEasyUnlock(
        current_user().basic_user_info.account_id);
    return;
  }

  password_view_->SetReadOnly(true);
  pin_input_view_->SetReadOnly(true);

  Shell::Get()->login_screen_controller()->AuthenticateUserWithPasswordOrPin(
      current_user().basic_user_info.account_id, base::UTF16ToUTF8(password),
      HasAuthMethod(AUTH_PIN),
      base::BindOnce(&LoginAuthUserView::OnAuthComplete,
                     weak_factory_.GetWeakPtr()));
}

void LoginAuthUserView::OnAuthComplete(base::Optional<bool> auth_success) {
  // Clear the password only if auth fails. Make sure to keep the password view
  // disabled even if auth succeededs, as if the user submits a password while
  // animating the next lock screen will not work as expected. See
  // https://crbug.com/808486.
  if (!auth_success.has_value() || !auth_success.value()) {
    password_view_->Reset();
    password_view_->SetReadOnly(false);
    pin_input_view_->Reset();
    pin_input_view_->SetReadOnly(false);
  }

  on_auth_.Run(auth_success.value(), /*display_error_messages=*/true);
}

void LoginAuthUserView::OnChallengeResponseAuthComplete(
    base::Optional<bool> auth_success) {
  if (!auth_success.has_value() || !auth_success.value()) {
    password_view_->Reset();
    password_view_->SetReadOnly(false);
    // If the user canceled the PIN request during ChallengeResponse,
    // ChallengeResponse will fail with an unknown error. Since this is
    // expected, we do not show this error.
    if (Shell::Get()
            ->login_screen_controller()
            ->GetSecurityTokenPinRequestCanceled()) {
      challenge_response_view_->SetState(
          ChallengeResponseView::State::kInitial);
    } else {
      challenge_response_view_->SetState(
          ChallengeResponseView::State::kFailure);
    }
  }

  on_auth_.Run(auth_success.value_or(false), /*display_error_messages=*/false);
}

void LoginAuthUserView::OnUserViewTap() {
  if (HasAuthMethod(AUTH_TAP)) {
    Shell::Get()->login_screen_controller()->AuthenticateUserWithEasyUnlock(
        current_user().basic_user_info.account_id);
  } else if (HasAuthMethod(AUTH_ONLINE_SIGN_IN)) {
    // Tapping anywhere in the user view is the same with tapping the message.
    OnOnlineSignInMessageTap();
  } else {
    on_tap_.Run();
  }
}

void LoginAuthUserView::OnOnlineSignInMessageTap() {
  Shell::Get()->login_screen_controller()->ShowGaiaSignin(
      current_user().basic_user_info.account_id);
}

void LoginAuthUserView::OnPinPadBackspace() {
  DCHECK(pin_input_view_);
  DCHECK(password_view_);
  if (input_field_mode_ == InputFieldMode::PIN_WITH_TOGGLE)
    pin_input_view_->Backspace();
  else
    password_view_->Backspace();
}

void LoginAuthUserView::OnPinPadInsertDigit(int digit) {
  DCHECK(pin_input_view_);
  DCHECK(password_view_);
  if (input_field_mode_ == InputFieldMode::PIN_WITH_TOGGLE)
    pin_input_view_->InsertDigit(digit);
  else
    password_view_->InsertNumber(digit);
}

void LoginAuthUserView::OnPasswordTextChanged(bool is_empty) {
  DCHECK(pin_view_);
  if (input_field_mode_ != InputFieldMode::PIN_WITH_TOGGLE)
    pin_view_->OnPasswordTextChanged(is_empty);
}

void LoginAuthUserView::OnPinTextChanged(bool is_empty) {
  DCHECK(pin_view_);
  if (input_field_mode_ == InputFieldMode::PIN_WITH_TOGGLE)
    pin_view_->OnPasswordTextChanged(is_empty);
}

bool LoginAuthUserView::HasAuthMethod(AuthMethods auth_method) const {
  return (auth_methods_ & auth_method) != 0;
}

void LoginAuthUserView::AttemptAuthenticateWithChallengeResponse() {
  challenge_response_view_->SetState(
      ChallengeResponseView::State::kAuthenticating);
  Shell::Get()
      ->login_screen_controller()
      ->AuthenticateUserWithChallengeResponse(
          current_user().basic_user_info.account_id,
          base::BindOnce(&LoginAuthUserView::OnChallengeResponseAuthComplete,
                         weak_factory_.GetWeakPtr()));
}

void LoginAuthUserView::UpdateFocus() {
  DCHECK(previous_state_);
  const UiState current_state{this};

  if (current_state.tpm_is_locked) {
    locked_tpm_message_view_->RequestFocus();
    return;
  }
  // All further states are exclusive.
  if (current_state.auth_disabled)
    disabled_auth_message_->RequestFocus();
  if (current_state.has_challenge_response)
    challenge_response_view_->RequestFocus();
  if (current_state.has_password && !previous_state_->has_password)
    password_view_->RequestFocus();
  if (current_state.has_pin_input)
    pin_input_view_->RequestFocus();
  // Tapping the user view will trigger the online sign-in flow when
  // |force_online_sign_in| is true.
  if (current_state.force_online_sign_in)
    user_view_->RequestFocus();
}

void LoginAuthUserView::OnSwitchButtonClicked() {
  // Ignore events from the switch button if no longer present.
  if (input_field_mode_ != InputFieldMode::PIN_WITH_TOGGLE &&
      input_field_mode_ != InputFieldMode::PWD_WITH_TOGGLE) {
    return;
  }

  // Clear both input fields.
  password_view_->Reset();
  pin_input_view_->Reset();
  // Cache the current state of the UI.
  CaptureStateForAnimationPreLayout();
  // Same auth methods, but the input field mode has changed.
  input_field_mode_ = (input_field_mode_ == InputFieldMode::PIN_WITH_TOGGLE)
                          ? InputFieldMode::PWD_WITH_TOGGLE
                          : InputFieldMode::PIN_WITH_TOGGLE;
  SetAuthMethods(auth_methods_, auth_metadata_);
  // Layout and animate.
  Layout();
  ApplyAnimationPostLayout(/*animate*/ true);
}

void LoginAuthUserView::UpdateInputFieldMode() {
  // There isn't an input field when any of the following is true:
  // - Challenge response is active (Smart Card)
  // - Online sign in message shown
  // - Disabled message shown
  // - No password auth available
  if (HasAuthMethod(AUTH_CHALLENGE_RESPONSE) ||
      HasAuthMethod(AUTH_ONLINE_SIGN_IN) ||
      HasAuthMethod(AUTH_DISABLED) ||
      !HasAuthMethod(AUTH_PASSWORD)) {
    input_field_mode_ = InputFieldMode::NONE;
    return;
  }

  if (!HasAuthMethod(AUTH_PIN)) {
    input_field_mode_ = InputFieldMode::PASSWORD_ONLY;
    return;
  }

  // Default to combined password/pin if autosubmit is disabled.
  const int pin_length = auth_metadata_.autosubmit_pin_length;
  if (!LoginPinInputView::IsAutosubmitSupported(pin_length)) {
    input_field_mode_ = InputFieldMode::PIN_AND_PASSWORD;
    return;
  }

  // Defaults to PIN + switch button if not showing the switch button already.
  if (input_field_mode_ != InputFieldMode::PIN_WITH_TOGGLE &&
      input_field_mode_ != InputFieldMode::PWD_WITH_TOGGLE) {
    input_field_mode_ = InputFieldMode::PIN_WITH_TOGGLE;
    return;
  }
}

bool LoginAuthUserView::ShouldShowPinPad() const {
  if (auth_metadata_.virtual_keyboard_visible)
    return false;
  switch (input_field_mode_) {
    case InputFieldMode::NONE:
      return false;
    case InputFieldMode::PASSWORD_ONLY:
    case InputFieldMode::PWD_WITH_TOGGLE:
      return auth_metadata_.show_pinpad_for_pw;
    case InputFieldMode::PIN_AND_PASSWORD:
    case InputFieldMode::PIN_WITH_TOGGLE:
      return true;
  }
}

bool LoginAuthUserView::ShouldShowPasswordField() const {
  return input_field_mode_ == InputFieldMode::PASSWORD_ONLY ||
         input_field_mode_ == InputFieldMode::PIN_AND_PASSWORD ||
         input_field_mode_ == InputFieldMode::PWD_WITH_TOGGLE;
}

bool LoginAuthUserView::ShouldShowPinInputField() const {
  return input_field_mode_ == InputFieldMode::PIN_WITH_TOGGLE;
}

bool LoginAuthUserView::ShouldShowToggle() const {
  return input_field_mode_ == InputFieldMode::PIN_WITH_TOGGLE ||
         input_field_mode_ == InputFieldMode::PWD_WITH_TOGGLE;
}

gfx::Size LoginAuthUserView::GetPaddingBelowUserView() const {
  const UiState state{this};

  if (state.has_password)
    return SizeFromHeight(kDistanceBetweenUserViewAndPasswordDp);
  if (state.has_pin_input)
    return SizeFromHeight(kDistanceBetweenUserViewAndPinInputDp);
  if (state.force_online_sign_in)
    return SizeFromHeight(kDistanceBetweenUserViewAndOnlineSigninDp);
  if (state.has_challenge_response)
    return SizeFromHeight(kDistanceBetweenUserViewAndChallengeResponseDp);

  return SizeFromHeight(0);
}

gfx::Size LoginAuthUserView::GetPaddingBelowPasswordView() const {
  const UiState state{this};

  if (state.has_pinpad)
    return SizeFromHeight(kDistanceBetweenPasswordFieldAndPinKeyboardDp);
  if (state.has_fingerprint)
    return SizeFromHeight(kDistanceBetweenPasswordFieldAndFingerprintViewDp);
  if (state.has_challenge_response)
    return SizeFromHeight(kDistanceBetweenPwdFieldAndChallengeResponseViewDp);

  return SizeFromHeight(0);
}

base::string16 LoginAuthUserView::GetPinPasswordToggleText() {
  if (input_field_mode_ == InputFieldMode::PWD_WITH_TOGGLE)
    return l10n_util::GetStringUTF16(IDS_ASH_LOGIN_SWITCH_TO_PIN);
  else
    return l10n_util::GetStringUTF16(IDS_ASH_LOGIN_SWITCH_TO_PASSWORD);
}

base::string16 LoginAuthUserView::GetPasswordViewPlaceholder() const {
  // Note: |AUTH_TAP| must have higher priority than |AUTH_PIN| when
  // determining the placeholder.
  if (HasAuthMethod(AUTH_TAP))
    return l10n_util::GetStringUTF16(
        IDS_ASH_LOGIN_POD_PASSWORD_TAP_PLACEHOLDER);
  if (input_field_mode_ == InputFieldMode::PIN_AND_PASSWORD)
    return l10n_util::GetStringUTF16(
        IDS_ASH_LOGIN_POD_PASSWORD_PIN_PLACEHOLDER);

  return l10n_util::GetStringUTF16(IDS_ASH_LOGIN_POD_PASSWORD_PLACEHOLDER);
}

}  // namespace ash
