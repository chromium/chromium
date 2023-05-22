// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_auth_user_view.h"

#include <map>
#include <memory>
#include <utility>

#include "ash/login/login_screen_controller.h"
#include "ash/login/resources/grit/login_resources.h"
#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/fingerprint_auth_factor_model.h"
#include "ash/login/ui/horizontal_image_sequence_animation_decoder.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_auth_factors_view.h"
#include "ash/login/ui/login_constants.h"
#include "ash/login/ui/login_display_style.h"
#include "ash/login/ui/login_password_view.h"
#include "ash/login/ui/login_pin_input_view.h"
#include "ash/login/ui/login_pin_view.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/pin_keyboard_animation.h"
#include "ash/login/ui/pin_request_view.h"
#include "ash/login/ui/smart_lock_auth_factor_model.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/smartlock_state.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/color_util.h"
#include "ash/style/pill_button.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/time_of_day.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
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
const int kPinPasswordToggleButtonPaddingBottom = 20;

// The background color id of the button used for switching between pin and
// password. Applied only for Jellyroll.
constexpr ui::ColorId kPinPasswordToggleColorId =
    cros_tokens::kCrosSysSystemOnBase1;

// Distance from the end of pin keyboard to the bottom of the big user view.
const int kDistanceFromPinKeyboardToBigUserViewBottomDp = 50;

// Distance from the top of the user view to the user icon.
constexpr int kDistanceFromTopOfBigUserViewToUserIconDp = 24;

// Date time format containing only the day of the week, for example: "Tuesday".
constexpr char kDayOfWeekOnlyTimeFormat[] = "EEEE";

constexpr int kDistanceBetweenPasswordFieldAndAuthFactorsViewDp = 90;

constexpr base::TimeDelta kChallengeResponseResetAfterFailureDelay =
    base::Seconds(5);
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

constexpr int kAuthFactorHidingPasswordFieldSlideUpDistanceDp = 42;
constexpr base::TimeDelta kAuthFactorHidingPasswordFieldSlideUpDuration =
    base::Milliseconds(600);

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
        if (observer.aborted_count()) {
          return true;
        }

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
        if (observer.aborted_count()) {
          return true;
        }

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
        if (observer.aborted_count()) {
          return true;
        }

        view->NotifyAccessibilityLocationChanged();
        return true;
      },
      view));
}

// The content needed to render the disabled auth message view.
struct LockScreenMessage {
  std::u16string title;
  std::u16string content;
  raw_ptr<const gfx::VectorIcon, ExperimentalAsh> icon;
};

// Returns the message used when the device was locked due to a time window
// limit.
LockScreenMessage GetWindowLimitMessage(const base::Time& unlock_time,
                                        bool use_24hour_clock) {
  LockScreenMessage message;
  message.title = l10n_util::GetStringUTF16(IDS_ASH_LOGIN_TIME_FOR_BED_MESSAGE);

  base::Time local_midnight = base::Time::Now().LocalMidnight();

  std::u16string time_to_display;
  if (use_24hour_clock) {
    time_to_display = base::TimeFormatTimeOfDayWithHourClockType(
        unlock_time, base::k24HourClock, base::kDropAmPm);
  } else {
    time_to_display = base::TimeFormatTimeOfDayWithHourClockType(
        unlock_time, base::k12HourClock, base::kKeepAmPm);
  }

  if (unlock_time < local_midnight + base::Days(1)) {
    // Unlock time is today.
    message.content = l10n_util::GetStringFUTF16(
        IDS_ASH_LOGIN_COME_BACK_MESSAGE, time_to_display);
  } else if (unlock_time < local_midnight + base::Days(2)) {
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
  if (used_time < base::Minutes(1)) {
    // The device was locked all day.
    message.title = l10n_util::GetStringUTF16(IDS_ASH_LOGIN_TAKE_BREAK_MESSAGE);
    message.content =
        l10n_util::GetStringUTF16(IDS_ASH_LOGIN_LOCKED_ALL_DAY_MESSAGE);
  } else {
    // The usage limit is over.
    message.title = l10n_util::GetStringUTF16(IDS_ASH_LOGIN_TIME_IS_UP_MESSAGE);

    const std::u16string used_time_string = ui::TimeFormat::Detailed(
        ui::TimeFormat::Format::FORMAT_DURATION,
        ui::TimeFormat::Length::LENGTH_LONG, /*cutoff=*/3, used_time);
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

// Consists of challenge-response icon view and a label.
class LoginAuthUserView::ChallengeResponseView : public views::View {
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
        base::BindRepeating(&ChallengeResponseView::ArrowButtonPressed,
                            base::Unretained(this)),
        kChallengeResponseArrowSizeDp);
    arrow_button_view->SetInstallFocusRingOnFocus(true);
    views::InstallCircleHighlightPathGenerator(arrow_button_view.get());
    arrow_button_ = AddChildView(std::move(arrow_button_view));
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
    label_->SetEnabledColorId(kColorAshTextColorSecondary);
    label_->SetSubpixelRenderingEnabled(false);
    label_->SetFontList(views::Label::GetDefaultFontList().Derive(
        /*size_delta=*/1, gfx::Font::FontStyle::ITALIC,
        gfx::Font::Weight::NORMAL));
  }

  ChallengeResponseView(const ChallengeResponseView&) = delete;
  ChallengeResponseView& operator=(const ChallengeResponseView&) = delete;

  ~ChallengeResponseView() override = default;

  void SetState(State state) {
    if (state_ == state) {
      return;
    }
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

  // views::View:
  void RequestFocus() override { arrow_button_->RequestFocus(); }

  views::Button* GetButtonForTesting() { return arrow_button_; }
  views::Label* GetLabelForTesting() { return label_; }

 private:
  ui::ImageModel GetImageForIcon() const {
    switch (state_) {
      case State::kInitial:
      case State::kAuthenticating:
        return ui::ImageModel::FromVectorIcon(kLockScreenSmartCardIcon,
                                              kColorAshIconColorPrimary,
                                              kChallengeResponseIconSizeDp);
      case State::kFailure:
        return ui::ImageModel::FromVectorIcon(kLockScreenSmartCardFailureIcon,
                                              kColorAshIconColorAlert,
                                              kChallengeResponseIconSizeDp);
    }
  }

  std::u16string GetTextForLabel() const {
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

  void ArrowButtonPressed() {
    // Ignore further clicks while handling the previous one.
    if (state_ != State::kAuthenticating) {
      on_start_tap_.Run();
    }
  }

  base::RepeatingClosure on_start_tap_;
  State state_ = State::kInitial;
  raw_ptr<ArrowButtonView, ExperimentalAsh> arrow_button_ = nullptr;
  raw_ptr<NonAccessibleView, ExperimentalAsh> arrow_to_icon_spacer_ = nullptr;
  raw_ptr<views::ImageView, ExperimentalAsh> icon_ = nullptr;
  raw_ptr<views::Label, ExperimentalAsh> label_ = nullptr;
  base::OneShotTimer reset_state_timer_;
};

// The message shown to the user when auth is disabled. This could happen when:
// -auth method is |AUTH_DISABLED|
// -auth method is |AUTH_ONLINE_SIGN_IN| and the user is on the secondary login
//  screen
// -the selected account has a restricted multiprofile policy set and the user
// is on the secondary login screen
class LoginAuthUserView::DisabledAuthMessageView : public views::View {
 public:
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(DisabledAuthMessageView* view) : view_(view) {}
    ~TestApi() = default;

    const std::u16string& GetDisabledAuthMessageContent() const {
      return view_->message_contents_->GetText();
    }

   private:
    const raw_ptr<DisabledAuthMessageView, ExperimentalAsh> view_;
  };

  DisabledAuthMessageView() {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        gfx::Insets::VH(kDisabledAuthMessageVerticalBorderDp,
                        kDisabledAuthMessageHorizontalBorderDp),
        kDisabledAuthMessageChildrenSpacingDp));
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetFocusBehavior(FocusBehavior::ALWAYS);

    // The icon size has to be defined later if the image will be visible.
    message_icon_ = AddChildView(std::make_unique<views::ImageView>());
    message_icon_->SetImageSize(gfx::Size(kDisabledAuthMessageIconSizeDp,
                                          kDisabledAuthMessageIconSizeDp));

    auto decorate_label = [](views::Label* label) {
      label->SetSubpixelRenderingEnabled(false);
      label->SetAutoColorReadabilityEnabled(false);
      label->SetFocusBehavior(FocusBehavior::ALWAYS);
    };
    message_title_ = AddChildView(std::make_unique<views::Label>(
        std::u16string(), views::style::CONTEXT_LABEL,
        views::style::STYLE_PRIMARY));
    message_title_->SetFontList(
        gfx::FontList().Derive(kDisabledAuthMessageTitleFontSizeDeltaDp,
                               gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
    decorate_label(message_title_);

    message_contents_ = AddChildView(std::make_unique<views::Label>(
        std::u16string(), views::style::CONTEXT_LABEL,
        views::style::STYLE_PRIMARY));
    message_contents_->SetFontList(
        gfx::FontList().Derive(kDisabledAuthMessageContentsFontSizeDeltaDp,
                               gfx::Font::NORMAL, gfx::Font::Weight::NORMAL));
    decorate_label(message_contents_);
    message_contents_->SetMultiLine(true);
  }

  DisabledAuthMessageView(const DisabledAuthMessageView&) = delete;
  DisabledAuthMessageView& operator=(const DisabledAuthMessageView&) = delete;

  ~DisabledAuthMessageView() override = default;

  // Set the parameters needed to render the message.
  void SetAuthDisabledMessage(const AuthDisabledData& auth_disabled_data,
                              bool use_24hour_clock) {
    LockScreenMessage message = GetLockScreenMessage(
        auth_disabled_data.reason, auth_disabled_data.auth_reenabled_time,
        auth_disabled_data.device_used_time, use_24hour_clock);
    SetPreferredSize(gfx::Size(kDisabledAuthMessageTimeWidthDp,
                               kDisabledAuthMessageHeightDp));
    message_icon_->SetVisible(true);
    message_vector_icon_ = message.icon;
    message_title_->SetText(message.title);
    message_contents_->SetText(message.content);
    UpdateColors();
  }

  void SetAuthDisabledMessage(const std::u16string& title,
                              const std::u16string& content) {
    SetPreferredSize(gfx::Size(kDisabledAuthMessageMultiprofileWidthDp,
                               kDisabledAuthMessageHeightDp));
    message_icon_->SetVisible(false);
    message_title_->SetText(title);
    message_contents_->SetText(content);
    UpdateColors();
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

  // views::View:
  void RequestFocus() override { message_title_->RequestFocus(); }

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    // Any view which claims to be focusable is expected to have an accessible
    // name and accessible role so that screen readers know what to present to
    // the user when it gains focus. In the case of this particular view,
    // `RequestFocus` gives focus to the `message_title_` label. As a result,
    // this view is not end-user focusable. However, its official focusability
    // will cause the accessibility paint checks to fail. Give this view a
    // container role. If the `message_title_` has text, set the accessible
    // name to that text; otherwise set the name explicitly empty to prevent
    // the paint check from failing during tests.
    node_data->role = ax::mojom::Role::kPane;
    if (message_title_->GetText().empty()) {
      node_data->SetNameExplicitlyEmpty();
    } else {
      node_data->SetNameChecked(message_title_->GetText());
    }
  }

 private:
  void UpdateColors() {
    message_title_->SetEnabledColorId(kColorAshTextColorPrimary);
    message_contents_->SetEnabledColorId(kColorAshTextColorPrimary);
    if (message_vector_icon_) {
      message_icon_->SetImage(ui::ImageModel::FromVectorIcon(
          *message_vector_icon_, kColorAshIconColorPrimary,
          kDisabledAuthMessageIconSizeDp));
    }
  }

  raw_ptr<views::Label, ExperimentalAsh> message_title_;
  raw_ptr<views::Label, ExperimentalAsh> message_contents_;
  raw_ptr<views::ImageView, ExperimentalAsh> message_icon_;
  raw_ptr<const gfx::VectorIcon, ExperimentalAsh> message_vector_icon_ =
      nullptr;
};

// The message shown to user when TPM is locked.
class LoginAuthUserView::LockedTpmMessageView : public views::View {
 public:
  LockedTpmMessageView() {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        gfx::Insets::VH(kLockedTpmMessageVerticalBorderDp,
                        kLockedTpmMessageHorizontalBorderDp),
        kLockedTpmMessageChildrenSpacingDp));
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetPreferredSize(
        gfx::Size(kLockedTpmMessageWidthDp, kLockedTpmMessageHeightDp));
    SetFocusBehavior(FocusBehavior::ALWAYS);

    message_icon_ = AddChildView(std::make_unique<views::ImageView>());
    message_icon_->SetImage(ui::ImageModel::FromVectorIcon(
        kLockScreenAlertIcon, kColorAshIconColorPrimary,
        kLockedTpmMessageIconSizeDp));

    message_warning_ = CreateLabel();
    message_warning_->SetEnabledColorId(kColorAshTextColorPrimary);

    message_description_ = CreateLabel();

    // Set content.
    std::u16string message_description = l10n_util::GetStringUTF16(
        IDS_ASH_LOGIN_POD_TPM_LOCKED_ISSUE_DESCRIPTION);
    message_description_->SetText(message_description);
    message_description_->SetEnabledColorId(kColorAshTextColorPrimary);
  }

  LockedTpmMessageView(const LockedTpmMessageView&) = delete;
  LockedTpmMessageView& operator=(const LockedTpmMessageView&) = delete;
  ~LockedTpmMessageView() override = default;

  // Set the parameters needed to render the message.
  void SetRemainingTime(base::TimeDelta time_left) {
    std::u16string time_left_message;
    if (base::TimeDurationFormatWithSeconds(
            time_left, base::DurationFormatWidth::DURATION_WIDTH_WIDE,
            &time_left_message)) {
      std::u16string message_warning = l10n_util::GetStringFUTF16(
          IDS_ASH_LOGIN_POD_TPM_LOCKED_ISSUE_WARNING, time_left_message);
      message_warning_->SetText(message_warning);

      if (time_left.InMinutes() != prev_time_left_.InMinutes()) {
        message_warning_->SetAccessibleName(message_warning);
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

  // views::View:
  void RequestFocus() override { message_warning_->RequestFocus(); }

 private:
  views::Label* CreateLabel() {
    auto label = std::make_unique<views::Label>(std::u16string(),
                                                views::style::CONTEXT_LABEL,
                                                views::style::STYLE_PRIMARY);
    label->SetFontList(gfx::FontList().Derive(kLockedTpmMessageDeltaDp,
                                              gfx::Font::NORMAL,
                                              gfx::Font::Weight::NORMAL));
    label->SetSubpixelRenderingEnabled(false);
    label->SetAutoColorReadabilityEnabled(false);
    label->SetFocusBehavior(FocusBehavior::ALWAYS);
    label->SetMultiLine(true);
    return AddChildView(std::move(label));
  }

  base::TimeDelta prev_time_left_;
  raw_ptr<views::Label, ExperimentalAsh> message_warning_;
  raw_ptr<views::Label, ExperimentalAsh> message_description_;
  raw_ptr<views::ImageView, ExperimentalAsh> message_icon_;
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
    has_smart_lock = view->HasAuthMethod(LoginAuthUserView::AUTH_SMART_LOCK);
    has_challenge_response =
        view->HasAuthMethod(LoginAuthUserView::AUTH_CHALLENGE_RESPONSE);
    auth_disabled = view->HasAuthMethod(LoginAuthUserView::AUTH_DISABLED);
    tpm_is_locked =
        view->HasAuthMethod(LoginAuthUserView::AUTH_DISABLED_TPM_LOCKED);
    force_online_sign_in =
        view->HasAuthMethod(LoginAuthUserView::AUTH_ONLINE_SIGN_IN);
    auth_factor_is_hiding_password = view->HasAuthMethod(
        LoginAuthUserView::AUTH_AUTH_FACTOR_IS_HIDING_PASSWORD);
    non_pin_y_start_in_screen = view->GetBoundsInScreen().y();
    pin_start_in_screen = view->pin_view_->GetBoundsInScreen().origin();
  }

  bool has_password = false;
  bool has_pin_input = false;
  bool has_pinpad = false;
  bool has_toggle = false;
  bool has_fingerprint = false;
  bool has_smart_lock = false;
  bool has_challenge_response = false;
  bool auth_disabled = false;
  bool tpm_is_locked = false;
  bool force_online_sign_in = false;
  bool auth_factor_is_hiding_password = false;
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
  return view_->online_sign_in_button_;
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

LoginAuthFactorsView* LoginAuthUserView::TestApi::auth_factors_view() const {
  return view_->auth_factors_view_;
}

AuthFactorModel* LoginAuthUserView::TestApi::fingerprint_auth_factor_model()
    const {
  return view_->fingerprint_auth_factor_model_;
}

AuthFactorModel* LoginAuthUserView::TestApi::smart_lock_auth_factor_model()
    const {
  return view_->smart_lock_auth_factor_model_;
}

bool LoginAuthUserView::TestApi::HasAuthMethod(AuthMethods auth_method) const {
  return view_->HasAuthMethod(auth_method);
}

void LoginAuthUserView::TestApi::SetFingerprintState(
    FingerprintState state) const {
  return view_->SetFingerprintState(state);
}

void LoginAuthUserView::TestApi::SetSmartLockState(SmartLockState state) const {
  return view_->SetSmartLockState(state);
}

const std::u16string&
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
  DCHECK(callbacks.on_auth_factor_is_hiding_password_changed);
  DCHECK_NE(user.basic_user_info.type, user_manager::USER_TYPE_PUBLIC_ACCOUNT);
  if (Shell::Get()->login_screen_controller()->IsAuthenticating()) {
    // TODO(b/276246832): We should avoid re-layouting during Authentication.
    LOG(WARNING)
        << "LoginAuthUserView::LoginAuthUserView called during Authentication.";
  }

  // Build child views.
  auto user_view = std::make_unique<LoginUserView>(
      LoginDisplayStyle::kLarge, true /*show_dropdown*/,
      base::BindRepeating(&LoginAuthUserView::OnUserViewTap,
                          base::Unretained(this)),
      callbacks.on_remove_warning_shown, callbacks.on_remove);
  user_view_ = user_view.get();

  auto password_view = std::make_unique<LoginPasswordView>();
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
      callbacks.on_easy_unlock_icon_hovered);

  auto pin_input_view = std::make_unique<LoginPinInputView>();
  pin_input_view_ = pin_input_view.get();
  pin_input_view->Init(base::BindRepeating(&LoginAuthUserView::OnAuthSubmit,
                                           base::Unretained(this)),
                       base::BindRepeating(&LoginAuthUserView::OnPinTextChanged,
                                           base::Unretained(this)));

  auto toggle_container = std::make_unique<NonAccessibleView>();
  toggle_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::TLBR(kPinPasswordToggleButtonPaddingTop, 0,
                        kPinPasswordToggleButtonPaddingBottom, 0)));
  pin_password_toggle_ =
      toggle_container->AddChildView(std::make_unique<PillButton>(
          base::BindRepeating(&LoginAuthUserView::OnSwitchButtonClicked,
                              base::Unretained(this)),
          GetPinPasswordToggleText()));
  pin_password_toggle_->SetMaxSize(
      gfx::Size(/*ignored*/ 0, kPinPasswordToggleButtonHeight));

  if (chromeos::features::IsJellyrollEnabled()) {
    pin_password_toggle_->SetBackgroundColorId(kPinPasswordToggleColorId);
  }

  auto pin_view = std::make_unique<LoginPinView>(
      LoginPinView::Style::kNumeric,
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

  std::u16string button_message =
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_ONLINE_SIGN_IN_MESSAGE);
  if (user.is_signed_in) {
    button_message =
        l10n_util::GetStringUTF16(IDS_ASH_LOCK_SCREEN_VERIFY_ACCOUNT_MESSAGE);
  }

  auto online_sign_in_button = std::make_unique<PillButton>(
      base::BindRepeating(&LoginAuthUserView::OnOnlineSignInMessageTap,
                          base::Unretained(this)),
      button_message);
  online_sign_in_button_ = online_sign_in_button.get();

  auto disabled_auth_message = std::make_unique<DisabledAuthMessageView>();
  disabled_auth_message_ = disabled_auth_message.get();

  auto locked_tpm_message_view = std::make_unique<LockedTpmMessageView>();
  locked_tpm_message_view_ = locked_tpm_message_view.get();

  auto fingerprint_auth_factor_model =
      FingerprintAuthFactorModel::Factory::Create(user.fingerprint_state);
  fingerprint_auth_factor_model_ = fingerprint_auth_factor_model.get();
  auto smart_lock_auth_factor_model = SmartLockAuthFactorModel::Factory::Create(
      user.smart_lock_state,
      base::BindRepeating(&LoginAuthUserView::OnSmartLockArrowButtonTapped,
                          base::Unretained(this)));
  smart_lock_auth_factor_model_ = smart_lock_auth_factor_model.get();

  // Note: at the moment, between Fingerprint and Smart Lock, Smart Lock
  // is the only auth factor which considers an "arrow button" tap event.
  auto auth_factors_view = std::make_unique<LoginAuthFactorsView>(
      base::BindRepeating(
          &SmartLockAuthFactorModel::OnArrowButtonTapOrClickEvent,
          base::Unretained(smart_lock_auth_factor_model_)),
      callbacks.on_auth_factor_is_hiding_password_changed);

  auth_factors_view_ = auth_factors_view.get();
  auth_factors_view_->AddAuthFactor(std::move(fingerprint_auth_factor_model));
  auth_factors_view_->AddAuthFactor(std::move(smart_lock_auth_factor_model));

  // Needed for up/down sliding animation.
  auth_factors_view_->SetPaintToLayer();
  auth_factors_view_->layer()->SetFillsBoundsOpaquely(false);

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
          std::move(online_sign_in_button));
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
  auto wrapped_auth_factors_view =
      login_views_utils::WrapViewForPreferredSize(std::move(auth_factors_view));
  auto wrapped_challenge_response_view =
      login_views_utils::WrapViewForPreferredSize(
          std::move(challenge_response_view));
  auto wrapped_padding_below_password_view =
      login_views_utils::WrapViewForPreferredSize(
          std::move(padding_below_password_view));
  auto wrapped_padding_below_user_view =
      login_views_utils::WrapViewForPreferredSize(
          std::move(padding_below_user_view));

  auto* box_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::TLBR(kDistanceFromTopOfBigUserViewToUserIconDp, 0,
                        kDistanceFromPinKeyboardToBigUserViewBottomDp, 0)));
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Add views in rendering order.
  auto* user_ptr = AddChildView(std::move(wrapped_user_view));
  AddChildView(std::move(wrapped_padding_below_user_view));
  auto* tpm_message_ptr =
      AddChildView(std::move(wrapped_locked_tpm_message_view));
  AddChildView(std::move(wrapped_password_view));
  AddChildView(std::move(wrapped_online_sign_in_message_view));
  auto* auth_message_ptr =
      AddChildView(std::move(wrapped_disabled_auth_message_view));
  AddChildView(std::move(wrapped_pin_input_view));
  AddChildView(std::move(wrapped_padding_below_password_view));
  AddChildView(std::move(wrapped_pin_view));
  AddChildView(std::move(wrapped_pin_password_toggle_view));
  AddChildView(std::move(wrapped_auth_factors_view));
  auto* challenge_ptr =
      AddChildView(std::move(wrapped_challenge_response_view));

  // Set up taborder.
  tpm_message_ptr->InsertAfterInFocusList(auth_message_ptr);
  user_ptr->InsertAfterInFocusList(challenge_ptr);

  // The user needs to be set before SetAuthMethods is called.
  user_view_->UpdateForUser(user, /*animate*/ false);

  // Update authentication UI.
  CaptureStateForAnimationPreLayout();
  SetAuthMethods(auth_methods_);
  ApplyAnimationPostLayout(/*animate*/ false);
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

  auto user = current_user();
  const std::u16string user_display_email =
      base::UTF8ToUTF16(user.basic_user_info.display_email);
  bool is_secondary_login =
      Shell::Get()->session_controller()->GetSessionState() ==
      session_manager::SessionState::LOGIN_SECONDARY;

  if (is_secondary_login && !user.is_multiprofile_allowed) {
    online_sign_in_button_->SetVisible(false);
    disabled_auth_message_->SetVisible(true);
    disabled_auth_message_->SetAuthDisabledMessage(
        l10n_util::GetStringUTF16(
            IDS_ASH_LOGIN_MULTI_PROFILES_RESTRICTED_POLICY_TITLE),
        GetMultiprofileDisableAuthMessage());
  }
  // We do not want the online sign in button to be visible on the secondary
  // login screen since we can not call OOBE there. In such a case, we indicate
  // the user to return on the login screen to go through online sign in.
  else if (is_secondary_login && current_state.force_online_sign_in) {
    online_sign_in_button_->SetVisible(false);
    disabled_auth_message_->SetVisible(true);
    disabled_auth_message_->SetAuthDisabledMessage(
        l10n_util::GetStringUTF16(IDS_ASH_LOGIN_SIGN_IN_REQUIRED_DIALOG_TITLE),
        l10n_util::GetStringFUTF16(
            IDS_ASH_LOGIN_SIGN_IN_REQUIRED_SECONDARY_LOGIN_MESSAGE,
            user_display_email, user_display_email));
  } else {
    online_sign_in_button_->SetVisible(current_state.force_online_sign_in);
    disabled_auth_message_->SetVisible(current_state.auth_disabled);
  }

  locked_tpm_message_view_->SetVisible(current_state.tpm_is_locked);
  if (current_state.tpm_is_locked &&
      auth_metadata.time_until_tpm_unlock.has_value()) {
    locked_tpm_message_view_->SetRemainingTime(
        auth_metadata.time_until_tpm_unlock.value());
  }

  // Adjust the PIN keyboard visibility before the password textfield's one, so
  // that when both are about to be hidden the focus doesn't jump to the "1"
  // keyboard button, causing unexpected accessibility effects.
  pin_view_->SetVisible(current_state.has_pinpad);

  password_view_->SetEnabled(current_state.has_password);
  password_view_->SetEnabledOnEmptyPassword(false);
  password_view_->SetFocusEnabledForTextfield(current_state.has_password);
  password_view_->SetVisible(current_state.has_password);
  password_view_->layer()->SetOpacity(current_state.has_password);

  pin_input_view_->UpdateLength(auth_metadata_.autosubmit_pin_length);
  pin_input_view_->SetAuthenticateWithEmptyPinOnReturnKey(false);
  pin_input_view_->SetVisible(current_state.has_pin_input);

  pin_password_toggle_->SetVisible(current_state.has_toggle);
  pin_password_toggle_->SetText(GetPinPasswordToggleText());

  // TODO(b/219978360): Should investigate why setting
  // |fingerprint_auth_factor_model_| availability here is necessary if state
  // management within LockContentsView is working properly.
  fingerprint_auth_factor_model_->set_available(current_state.has_fingerprint);
  auth_factors_view_->SetCanUsePin(HasAuthMethod(AUTH_PIN));
  auth_factors_view_->SetVisible(current_state.has_fingerprint ||
                                 current_state.has_smart_lock);

  challenge_response_view_->SetVisible(current_state.has_challenge_response);

  padding_below_user_view_->SetPreferredSize(GetPaddingBelowUserView());
  padding_below_password_view_->SetPreferredSize(GetPaddingBelowPasswordView());

  password_view_->SetPlaceholderText(GetPasswordViewPlaceholder());
  password_view_->SetAccessibleName(l10n_util::GetStringFUTF16(
      IDS_ASH_LOGIN_POD_PASSWORD_FIELD_ACCESSIBLE_NAME, user_display_email));

  // Only the active auth user view has authentication methods. If that is the
  // case, then render the user view as if it was always focused, since clicking
  // on it will not do anything (such as swapping users).
  user_view_->SetForceOpaque(auth_methods_ != AUTH_NONE);
  user_view_->SetTapEnabled(auth_methods_ == AUTH_NONE);

  UpdateFocus();
  PreferredSizeChanged();
}

void LoginAuthUserView::SetEasyUnlockIcon(
    EasyUnlockIconState icon_state,
    const std::u16string& accessibility_label) {
  // TODO(b/281542383): remove this function
}

void LoginAuthUserView::CaptureStateForAnimationPreLayout() {
  auto stop_animation = [](views::View* view) {
    if (view->layer()->GetAnimator()->is_animating()) {
      view->layer()->GetAnimator()->StopAnimating();
    }
  };

  // Stop any running animation scheduled in ApplyAnimationPostLayout.
  stop_animation(this);
  stop_animation(password_view_);
  stop_animation(pin_view_);
  stop_animation(challenge_response_view_);
  stop_animation(pin_password_toggle_);
  stop_animation(auth_factors_view_);

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
    auto prev_transform_y =
        layer()->GetAnimator()->GetTargetTransform().To2dTranslation().y();

    // Transform the layer so the user view renders where it used to be. This
    // requires a y offset.
    // Note: Doing this animation via ui::ScopedLayerAnimationSettings works,
    // but it seems that the timing gets slightly out of sync with the PIN
    // animation.
    auto move_to_center = std::make_unique<ui::InterpolatedTranslation>(
        gfx::PointF(0, previous_state_->non_pin_y_start_in_screen +
                           prev_transform_y - non_pin_y_end_in_screen),
        gfx::PointF());
    auto transition =
        ui::LayerAnimationElement::CreateInterpolatedTransformElement(
            std::move(move_to_center),
            base::Milliseconds(login::kChangeUserAnimationDurationMs));
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
    if (!current_state.has_password) {
      std::swap(opacity_start, opacity_end);
    }

    password_view_->layer()->SetOpacity(opacity_start);

    {
      ui::ScopedLayerAnimationSettings settings(
          password_view_->layer()->GetAnimator());
      settings.SetTransitionDuration(
          base::Milliseconds(login::kChangeUserAnimationDurationMs));
      settings.SetTweenType(gfx::Tween::Type::FAST_OUT_SLOW_IN);
      if (previous_state_->has_password && !current_state.has_password) {
        settings.AddObserver(password_view_);
      }

      password_view_->layer()->SetOpacity(opacity_end);
    }
  }

  ////////
  // Fade the pin/pwd toggle if its being hidden or shown.
  if (previous_state_->has_toggle != current_state.has_toggle) {
    float opacity_start = 0, opacity_end = 1;
    if (!current_state.has_toggle) {
      std::swap(opacity_start, opacity_end);
    }

    pin_password_toggle_->layer()->SetOpacity(opacity_start);

    {
      ui::ScopedLayerAnimationSettings settings(
          pin_password_toggle_->layer()->GetAnimator());
      settings.SetTransitionDuration(
          base::Milliseconds(login::kChangeUserAnimationDurationMs));
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
        base::Milliseconds(login::kChangeUserAnimationDurationMs / 2.0f),
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
  // Slide the auth factors view up/down when entering/leaving a state of
  // |auth_factors_view_| that requests the password field to be hidden.
  if (previous_state_->auth_factor_is_hiding_password !=
      current_state.auth_factor_is_hiding_password) {
    CHECK(auth_factors_view_);
    ui::ScopedLayerAnimationSettings settings(
        auth_factors_view_->layer()->GetAnimator());
    settings.SetTransitionDuration(
        kAuthFactorHidingPasswordFieldSlideUpDuration);
    settings.SetTweenType(gfx::Tween::Type::ACCEL_20_DECEL_100);

    gfx::Transform transform;
    transform.Translate(/*x=*/0,
                        /*y=*/current_state.auth_factor_is_hiding_password
                            ? -kAuthFactorHidingPasswordFieldSlideUpDistanceDp
                            : 0);
    auth_factors_view_->layer()->SetTransform(transform);
  }

  // Translate the user view to its previous position when in the auth factor
  // view requests to hide the password field. This prevents the user view
  // from moving when the password view collapses. Note that this transform is
  // applied even if |auth_factor_is_hiding_password| hasn't changed; the
  // user view should not move on subsequent LayoutAuth() calls if an auth
  // factor still wants to hide the password.
  if (current_state.auth_factor_is_hiding_password) {
    layer()->GetAnimator()->StopAnimating();
    int non_pin_y_end_in_screen = GetBoundsInScreen().y();
    gfx::Transform transform;
    transform.Translate(/*x=*/0,
                        /*y=*/previous_state_->non_pin_y_start_in_screen -
                            non_pin_y_end_in_screen);
    layer()->SetTransform(transform);
  }

  ////////
  // Fade the challenge response (Smart Card) if it is being hidden or shown.
  if (previous_state_->has_challenge_response !=
      current_state.has_challenge_response) {
    float opacity_start = 0, opacity_end = 1;
    if (!current_state.has_challenge_response) {
      std::swap(opacity_start, opacity_end);
    }

    challenge_response_view_->layer()->SetOpacity(opacity_start);

    {
      ui::ScopedLayerAnimationSettings settings(
          challenge_response_view_->layer()->GetAnimator());
      settings.SetTransitionDuration(
          base::Milliseconds(login::kChangeUserAnimationDurationMs));
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
    pin_input_view_->Reset();
    password_view_->Reset();
    password_view_->SetDisplayPasswordButtonVisible(
        user.show_display_password_button);
  }
  online_sign_in_button_->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_ONLINE_SIGN_IN_MESSAGE));
}

void LoginAuthUserView::SetFingerprintState(FingerprintState state) {
  CHECK(fingerprint_auth_factor_model_);
  fingerprint_auth_factor_model_->SetFingerprintState(state);
}

void LoginAuthUserView::ResetFingerprintUIState() {
  CHECK(fingerprint_auth_factor_model_);
  fingerprint_auth_factor_model_->ResetUIState();
}

void LoginAuthUserView::NotifyFingerprintAuthResult(bool success) {
  CHECK(fingerprint_auth_factor_model_);
  fingerprint_auth_factor_model_->NotifyFingerprintAuthResult(success);
}

void LoginAuthUserView::SetSmartLockState(SmartLockState state) {
  CHECK(smart_lock_auth_factor_model_);
  smart_lock_auth_factor_model_->SetSmartLockState(state);
}

void LoginAuthUserView::NotifySmartLockAuthResult(bool success) {
  CHECK(smart_lock_auth_factor_model_);
  smart_lock_auth_factor_model_->NotifySmartLockAuthResult(success);
}

void LoginAuthUserView::SetAuthDisabledMessage(
    const AuthDisabledData& auth_disabled_data) {
  disabled_auth_message_->SetAuthDisabledMessage(
      auth_disabled_data, current_user().use_24hour_clock);
}

const LoginUserInfo& LoginAuthUserView::current_user() const {
  return user_view_->current_user();
}

base::WeakPtr<views::View> LoginAuthUserView::GetActiveInputView() {
  if (input_field_mode_ == InputFieldMode::PIN_WITH_TOGGLE) {
    return pin_input_view_ != nullptr ? pin_input_view_->AsWeakPtr() : nullptr;
  }

  return password_view_ != nullptr ? password_view_->AsWeakPtr() : nullptr;
}

gfx::Size LoginAuthUserView::CalculatePreferredSize() const {
  gfx::Size size = views::View::CalculatePreferredSize();
  // Make sure we are at least as big as the user view. If we do not do this
  // the view will be below minimum size when no auth methods are displayed.
  size.SetToMax(user_view_->GetPreferredSize());
  return size;
}

void LoginAuthUserView::RequestFocus() {
  if (input_field_mode_ == InputFieldMode::PIN_WITH_TOGGLE) {
    pin_input_view_->RequestFocus();
  } else if (password_view_->GetEnabled()) {
    RequestFocusOnPasswordView();
  }
}

void LoginAuthUserView::OnGestureEvent(ui::GestureEvent* event) {
  // The textfield area may be too small for the user to successfully tap
  // inside it at their first attempt. We also want to bring up the virtual
  // keyboard if the user taps in the auth user view area.
  RequestFocus();
}

void LoginAuthUserView::OnAuthSubmit(const std::u16string& password) {
  LOG(WARNING) << "crbug.com/1339004 : AuthSubmit "
               << password_view_->IsReadOnly() << " / "
               << pin_input_view_->IsReadOnly();

  password_view_->SetReadOnly(true);
  pin_input_view_->SetReadOnly(true);

  // Checking if the password is only formed of numbers with base::StringToInt
  // will easily fail due to numeric limits. ContainsOnlyChars is used instead.
  const bool authenticated_by_pin =
      ShouldAuthenticateWithPin() &&
      base::ContainsOnlyChars(base::UTF16ToUTF8(password), "0123456789");

  Shell::Get()->login_screen_controller()->AuthenticateUserWithPasswordOrPin(
      current_user().basic_user_info.account_id, base::UTF16ToUTF8(password),
      authenticated_by_pin,
      base::BindOnce(&LoginAuthUserView::OnAuthComplete,
                     weak_factory_.GetWeakPtr(), authenticated_by_pin));
}

void LoginAuthUserView::OnAuthComplete(bool authenticated_by_pin,
                                       absl::optional<bool> auth_success) {
  bool failed = !auth_success.value_or(false);
  LOG(WARNING) << "crbug.com/1339004 : OnAuthComplete " << failed;

  // Clear the password only if auth fails. Make sure to keep the password
  // view disabled even if auth succeededs, as if the user submits a password
  // while animating the next lock screen will not work as expected. See
  // https://crbug.com/808486.
  if (failed) {
    password_view_->Reset();
    password_view_->SetReadOnly(false);
    pin_input_view_->Reset();
    pin_input_view_->SetReadOnly(false);
  }

  on_auth_.Run(auth_success.value(), /*display_error_messages=*/true,
               authenticated_by_pin);
}

void LoginAuthUserView::OnChallengeResponseAuthComplete(
    absl::optional<bool> auth_success) {
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

  on_auth_.Run(auth_success.value_or(false),
               /*display_error_messages=*/false,
               /*authenticated_by_pin*/ false);
}

void LoginAuthUserView::OnSmartLockArrowButtonTapped() {
  Shell::Get()->login_screen_controller()->AuthenticateUserWithEasyUnlock(
      current_user().basic_user_info.account_id);
}

void LoginAuthUserView::OnUserViewTap() {
  if (HasAuthMethod(AUTH_ONLINE_SIGN_IN)) {
    // Tapping anywhere in the user view is the same with tapping the message.
    OnOnlineSignInMessageTap();
  } else {
    on_tap_.Run();
  }
}

void LoginAuthUserView::OnOnlineSignInMessageTap() {
  // Do not show on secondary login screen as there is no OOBE there.
  if (Shell::Get()->session_controller()->GetSessionState() ==
      session_manager::SessionState::LOGIN_SECONDARY) {
    return;
  }

  user_manager::KnownUser known_user(Shell::Get()->local_state());
  int reauth_reason =
      known_user.FindReauthReason(current_user().basic_user_info.account_id)
          .value_or(-1);
  LOG(WARNING) << "Showing online GAIA signin, the reauth reason was: "
               << reauth_reason;

  Shell::Get()->login_screen_controller()->ShowGaiaSignin(
      current_user().basic_user_info.account_id);
}

void LoginAuthUserView::OnPinPadBackspace() {
  DCHECK(pin_input_view_);
  DCHECK(password_view_);
  if (input_field_mode_ == InputFieldMode::PIN_WITH_TOGGLE) {
    pin_input_view_->Backspace();
  } else {
    password_view_->Backspace();
  }
}

void LoginAuthUserView::OnPinPadInsertDigit(int digit) {
  DCHECK(pin_input_view_);
  DCHECK(password_view_);
  if (input_field_mode_ == InputFieldMode::PIN_WITH_TOGGLE) {
    pin_input_view_->InsertDigit(digit);
  } else {
    password_view_->InsertNumber(digit);
  }
}

void LoginAuthUserView::OnPasswordTextChanged(bool is_empty) {
  DCHECK(pin_view_);
  if (input_field_mode_ != InputFieldMode::PIN_WITH_TOGGLE) {
    pin_view_->OnPasswordTextChanged(is_empty);
  }
}

void LoginAuthUserView::OnPinTextChanged(bool is_empty) {
  DCHECK(pin_view_);
  if (input_field_mode_ == InputFieldMode::PIN_WITH_TOGGLE) {
    pin_view_->OnPasswordTextChanged(is_empty);
  }
}

bool LoginAuthUserView::HasAuthMethod(AuthMethods auth_method) const {
  return (auth_methods_ & auth_method) != 0;
}

bool LoginAuthUserView::ShouldAuthenticateWithPin() const {
  return input_field_mode_ == InputFieldMode::PIN_AND_PASSWORD ||
         input_field_mode_ == InputFieldMode::PIN_WITH_TOGGLE;
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

void LoginAuthUserView::RequestFocusOnPasswordView() {
  password_view_->RequestFocus();

  const UiState current_state{this};
  // Bring up the virtual keyboard if enabled as soon as we get the focus.
  // This way, the user does not have to type twice (on the user pod and
  // additionally on the textfield or user view).
  if (GetInputMethod() && !current_state.has_pinpad) {
    GetInputMethod()->SetVirtualKeyboardVisibilityIfEnabled(true);
  }
}

void LoginAuthUserView::UpdateFocus() {
  DCHECK(previous_state_);
  const UiState current_state{this};

  if (current_state.tpm_is_locked) {
    locked_tpm_message_view_->RequestFocus();
    return;
  }
  // All further states are exclusive.
  if (current_state.auth_disabled) {
    disabled_auth_message_->RequestFocus();
  }
  if (current_state.has_challenge_response) {
    challenge_response_view_->RequestFocus();
  }
  if (current_state.has_password && !previous_state_->has_password) {
    RequestFocusOnPasswordView();
  }
  if (current_state.has_pin_input) {
    pin_input_view_->RequestFocus();
  }
  // Tapping the user view will trigger the online sign-in flow when
  // |force_online_sign_in| is true.
  if (current_state.force_online_sign_in) {
    user_view_->RequestFocus();
  }
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
  // - Auth factors view is requesting to hide the password/PIN field
  if (HasAuthMethod(AUTH_CHALLENGE_RESPONSE) ||
      HasAuthMethod(AUTH_ONLINE_SIGN_IN) || HasAuthMethod(AUTH_DISABLED) ||
      !HasAuthMethod(AUTH_PASSWORD) ||
      HasAuthMethod(AUTH_AUTH_FACTOR_IS_HIDING_PASSWORD)) {
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
  if (auth_metadata_.virtual_keyboard_visible) {
    return false;
  }
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

  if (state.has_password) {
    return SizeFromHeight(kDistanceBetweenUserViewAndPasswordDp);
  }
  if (state.has_pin_input) {
    return SizeFromHeight(kDistanceBetweenUserViewAndPinInputDp);
  }
  if (state.force_online_sign_in) {
    return SizeFromHeight(kDistanceBetweenUserViewAndOnlineSigninDp);
  }
  if (state.has_challenge_response) {
    return SizeFromHeight(kDistanceBetweenUserViewAndChallengeResponseDp);
  }

  return SizeFromHeight(0);
}

gfx::Size LoginAuthUserView::GetPaddingBelowPasswordView() const {
  const UiState state{this};

  if (state.has_pinpad) {
    return SizeFromHeight(kDistanceBetweenPasswordFieldAndPinKeyboardDp);
  }
  if (state.has_fingerprint ||
      (auth_factors_view_ && auth_factors_view_->GetVisible())) {
    return SizeFromHeight(kDistanceBetweenPasswordFieldAndAuthFactorsViewDp);
  }
  if (state.has_challenge_response) {
    return SizeFromHeight(kDistanceBetweenPwdFieldAndChallengeResponseViewDp);
  }

  return SizeFromHeight(0);
}

std::u16string LoginAuthUserView::GetPinPasswordToggleText() const {
  if (input_field_mode_ == InputFieldMode::PWD_WITH_TOGGLE) {
    return l10n_util::GetStringUTF16(IDS_ASH_LOGIN_SWITCH_TO_PIN);
  } else {
    return l10n_util::GetStringUTF16(IDS_ASH_LOGIN_SWITCH_TO_PASSWORD);
  }
}

std::u16string LoginAuthUserView::GetPasswordViewPlaceholder() const {
  if (input_field_mode_ == InputFieldMode::PIN_AND_PASSWORD) {
    return l10n_util::GetStringUTF16(
        IDS_ASH_LOGIN_POD_PASSWORD_PIN_PLACEHOLDER);
  }

  return l10n_util::GetStringUTF16(IDS_ASH_LOGIN_POD_PASSWORD_PLACEHOLDER);
}

std::u16string LoginAuthUserView::GetMultiprofileDisableAuthMessage() const {
  int message_id;
  switch (current_user().multiprofile_policy) {
    case MultiProfileUserBehavior::PRIMARY_ONLY:
      message_id = IDS_ASH_LOGIN_MULTI_PROFILES_PRIMARY_ONLY_POLICY_MSG;
      break;
    case MultiProfileUserBehavior::NOT_ALLOWED:
      message_id = IDS_ASH_LOGIN_MULTI_PROFILES_NOT_ALLOWED_POLICY_MSG;
      break;
    case MultiProfileUserBehavior::OWNER_PRIMARY_ONLY:
      message_id = IDS_ASH_LOGIN_MULTI_PROFILES_OWNER_PRIMARY_ONLY_MSG;
      break;
    case MultiProfileUserBehavior::UNRESTRICTED:
      NOTREACHED();
      message_id = 0;
      break;
  }
  return l10n_util::GetStringUTF16(message_id);
}

}  // namespace ash
