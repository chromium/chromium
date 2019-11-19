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
#include "ash/login/ui/login_pin_view.h"
#include "ash/login/ui/login_user_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/pin_keyboard_animation.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/login_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/night_light/time_of_day.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "components/user_manager/user.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

namespace ash {
namespace {

constexpr const char kLoginAuthUserViewClassName[] = "LoginAuthUserView";

// Distance between the user view (ie, the icon and name) and the password
// textfield.
const int kDistanceBetweenUserViewAndPasswordDp = 24;

// Distance between the password textfield and the the pin keyboard.
const int kDistanceBetweenPasswordFieldAndPinKeyboardDp = 16;

// Distance from the end of pin keyboard to the bottom of the big user view.
const int kDistanceFromPinKeyboardToBigUserViewBottomDp = 50;

// Distance from the top of the user view to the user icon.
constexpr int kDistanceFromTopOfBigUserViewToUserIconDp = 24;

constexpr SkColor kChallengeResponseArrowBackgroundColor =
    SkColorSetARGB(0x2B, 0xFF, 0xFF, 0xFF);
constexpr SkColor kChallengeResponseErrorColor =
    SkColorSetRGB(0xEE, 0x67, 0x5C);

// The color of the disabled auth message bubble when the color extracted from
// wallpaper is transparent or invalid (i.e. color calculation fails or is
// disabled).
constexpr SkColor kDisabledAuthMessageBubbleColor =
    SkColorSetRGB(0x20, 0x21, 0x24);

// Date time format containing only the day of the week, for example: "Tuesday".
constexpr char kDayOfWeekOnlyTimeFormat[] = "EEEE";

constexpr int kFingerprintIconSizeDp = 32;
constexpr int kResetToDefaultIconDelayMs = 1300;
constexpr int kFingerprintIconTopSpacingDp = 20;
constexpr int kSpacingBetweenFingerprintIconAndLabelDp = 15;
constexpr int kFingerprintViewWidthDp = 204;
constexpr int kDistanceBetweenPasswordFieldAndFingerprintViewDp = 90;
constexpr int kFingerprintFailedAnimationDurationMs = 700;
constexpr int kFingerprintFailedAnimationNumFrames = 45;

constexpr base::TimeDelta kChallengeResponseResetAfterFailureDelay =
    base::TimeDelta::FromSeconds(5);
constexpr int kChallengeResponseArrowSizeDp = 40;
constexpr int kSpacingBetweenChallengeResponseArrowAndIconDp = 64;
constexpr int kSpacingBetweenChallengeResponseIconAndLabelDp = 15;
constexpr int kChallengeResponseIconSizeDp = 32;
constexpr int kDistanceBetweenPasswordFieldAndChallengeResponseViewDp = 0;

constexpr int kDisabledAuthMessageVerticalBorderDp = 16;
constexpr int kDisabledAuthMessageHorizontalBorderDp = 16;
constexpr int kDisabledAuthMessageChildrenSpacingDp = 4;
constexpr int kDisabledAuthMessageWidthDp = 204;
constexpr int kDisabledAuthMessageHeightDp = 98;
constexpr int kDisabledAuthMessageIconSizeDp = 24;
constexpr int kDisabledAuthMessageTitleFontSizeDeltaDp = 3;
constexpr int kDisabledAuthMessageContentsFontSizeDeltaDp = -1;
constexpr int kDisabledAuthMessageRoundedCornerRadiusDp = 8;

constexpr int kNonEmptyWidthDp = 1;

// The color of the required online sign-in  message text.
constexpr SkColor kSystemButtonMessageColor = SK_ColorBLACK;
// The background color of the required online sign-in button.
constexpr SkColor kSystemButtonBackgroundColor =
    SkColorSetA(gfx::kGoogleRed300, SK_AlphaOPAQUE);

constexpr int kUserInfoBubbleWidth = 192;
constexpr int kUserInfoBubbleExternalPadding = 8;
constexpr int kSystemButtonIconSize = 20;
constexpr int kSystemButtonMarginTopBottomDp = 6;
constexpr int kSystemButtonMarginLeftRightDp = 16;
constexpr int kSystemButtonBorderRadius = 16;
constexpr int kSystemButtonImageLabelSpacing = 8;
constexpr int kSystemButtonMaxLabelWidthDp =
    kUserInfoBubbleWidth - 2 * kUserInfoBubbleExternalPadding -
    kSystemButtonIconSize - kSystemButtonImageLabelSpacing -
    2 * kSystemButtonBorderRadius;

// Returns an observer that will hide |view| when it fires. The observer will
// delete itself after firing (by returning true). Make sure to call
// |observer->SetActive()| after attaching it.
ui::CallbackLayerAnimationObserver* BuildObserverToHideView(views::View* view) {
  return new ui::CallbackLayerAnimationObserver(base::Bind(
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
    password_view_->Clear();
    password_view_->SetVisible(false);
    delete this;
  }

 private:
  LoginPasswordView* const password_view_;

  DISALLOW_COPY_AND_ASSIGN(ClearPasswordAndHideAnimationObserver);
};

SkPath GetSystemButtonHighlightPath(const views::View* view) {
  gfx::Rect rect(view->GetLocalBounds());
  return SkPath().addRoundRect(gfx::RectToSkRect(rect),
                               kSystemButtonBorderRadius,
                               kSystemButtonBorderRadius);
}

class SystemButtonHighlightPathGenerator
    : public views::HighlightPathGenerator {
 public:
  SystemButtonHighlightPathGenerator() = default;
  SystemButtonHighlightPathGenerator(
      const SystemButtonHighlightPathGenerator&) = delete;
  SystemButtonHighlightPathGenerator& operator=(
      const SystemButtonHighlightPathGenerator&) = delete;

  // views::HighlightPathGenerator:
  SkPath GetHighlightPath(const views::View* view) override {
    return GetSystemButtonHighlightPath(view);
  }
};

class SystemButton : public views::LabelButton {
 public:
  SystemButton(views::ButtonListener* listener, const base::string16& text)
      : LabelButton(listener, text) {
    SetImageLabelSpacing(kSystemButtonImageLabelSpacing);
    label()->SetMultiLine(true);
    label()->SetMaximumWidth(kSystemButtonMaxLabelWidthDp);
    label()->SetFontList(
        gfx::FontList().DeriveWithWeight(gfx::Font::Weight::MEDIUM));
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetImage(views::Button::STATE_NORMAL,
             CreateVectorIcon(kLockScreenAlertIcon, kSystemButtonMessageColor));
    SetTextSubpixelRenderingEnabled(false);
    SetTextColor(views::Button::STATE_NORMAL, kSystemButtonMessageColor);
    SetTextColor(views::Button::STATE_HOVERED, kSystemButtonMessageColor);
    SetTextColor(views::Button::STATE_PRESSED, kSystemButtonMessageColor);
    views::HighlightPathGenerator::Install(
        this, std::make_unique<SystemButtonHighlightPathGenerator>());
  }

  SystemButton(const SystemButton&) = delete;
  SystemButton& operator=(const SystemButton&) = delete;
  ~SystemButton() override = default;

  // views::LabelButton:
  void PaintButtonContents(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(kSystemButtonBackgroundColor);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawPath(GetSystemButtonHighlightPath(this), flags);
  }

  gfx::Insets GetInsets() const override {
    return gfx::Insets(
        kSystemButtonMarginTopBottomDp, kSystemButtonMarginLeftRightDp,
        kSystemButtonMarginTopBottomDp, kSystemButtonMarginLeftRightDp);
  }
};

// The label shown below the fingerprint icon.
class FingerprintLabel : public views::Label {
 public:
  FingerprintLabel() {
    SetSubpixelRenderingEnabled(false);
    SetAutoColorReadabilityEnabled(false);
    SetEnabledColor(login_constants::kAuthMethodsTextColor);

    SetTextBasedOnState(FingerprintState::AVAILABLE);
  }

  void SetTextBasedOnAuthAttempt(bool success) {
    SetText(l10n_util::GetStringUTF16(
        success ? IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_AUTH_SUCCESS
                : IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_AUTH_FAILED));
    SetAccessibleName(l10n_util::GetStringUTF16(
        success ? IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_ACCESSIBLE_AUTH_SUCCESS
                : IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_ACCESSIBLE_AUTH_FAILED));
  }

  void SetTextBasedOnState(FingerprintState state) {
    auto get_displayed_id = [&]() {
      switch (state) {
        case FingerprintState::UNAVAILABLE:
        case FingerprintState::AVAILABLE:
          return IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_AVAILABLE;
        case FingerprintState::DISABLED_FROM_ATTEMPTS:
          return IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_DISABLED_FROM_ATTEMPTS;
        case FingerprintState::DISABLED_FROM_TIMEOUT:
          return IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_DISABLED_FROM_TIMEOUT;
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
LockScreenMessage GetWindowLimitMessage(const base::Time& unlock_time) {
  LockScreenMessage message;
  message.title = l10n_util::GetStringUTF16(IDS_ASH_LOGIN_TIME_FOR_BED_MESSAGE);

  base::Time local_midnight = base::Time::Now().LocalMidnight();
  const std::string time_of_day = TimeOfDay::FromTime(unlock_time).ToString();

  if (unlock_time < local_midnight + base::TimeDelta::FromDays(1)) {
    // Unlock time is today.
    message.content = l10n_util::GetStringFUTF16(
        IDS_ASH_LOGIN_COME_BACK_MESSAGE, base::UTF8ToUTF16(time_of_day));
  } else if (unlock_time < local_midnight + base::TimeDelta::FromDays(2)) {
    // Unlock time is tomorrow.
    message.content =
        l10n_util::GetStringFUTF16(IDS_ASH_LOGIN_COME_BACK_TOMORROW_MESSAGE,
                                   base::UTF8ToUTF16(time_of_day));
  } else {
    message.content = l10n_util::GetStringFUTF16(
        IDS_ASH_LOGIN_COME_BACK_DAY_OF_WEEK_MESSAGE,
        base::TimeFormatWithPattern(unlock_time, kDayOfWeekOnlyTimeFormat),
        base::UTF8ToUTF16(time_of_day));
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
                                       const base::TimeDelta& used_time) {
  switch (lock_reason) {
    case AuthDisabledReason::kTimeWindowLimit:
      return GetWindowLimitMessage(unlock_time);
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

  void NotifyFingerprintAuthResult(bool success) {
    reset_state_.Stop();
    label_->SetTextBasedOnAuthAttempt(success);

    if (success) {
      icon_->SetImage(gfx::CreateVectorIcon(kLockScreenFingerprintSuccessIcon,
                                            kFingerprintIconSizeDp,
                                            gfx::kGoogleGreenDark500));
    } else {
      SetIcon(FingerprintState::DISABLED_FROM_ATTEMPTS);
      // base::Unretained is safe because reset_state_ is owned by |this|.
      reset_state_.Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kResetToDefaultIconDelayMs),
          base::BindRepeating(&FingerprintView::DisplayCurrentState,
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

 private:
  void DisplayCurrentState() {
    SetVisible(state_ != FingerprintState::UNAVAILABLE &&
               state_ != FingerprintState::DISABLED_FROM_TIMEOUT);
    SetIcon(state_);
    label_->SetTextBasedOnState(state_);
  }

  void FireAlert() {
    label_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert,
                                     true /*send_native_event*/);
  }

  void SetIcon(FingerprintState state) {
    switch (state) {
      case FingerprintState::UNAVAILABLE:
      case FingerprintState::AVAILABLE:
      case FingerprintState::DISABLED_FROM_TIMEOUT:
        icon_->SetImage(gfx::CreateVectorIcon(
            kLockScreenFingerprintIcon, kFingerprintIconSizeDp, SK_ColorWHITE));
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
  FingerprintState state_ = FingerprintState::AVAILABLE;

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

    arrow_button_ = AddChildView(std::make_unique<ArrowButtonView>(
        /*listener=*/this, kChallengeResponseArrowSizeDp));
    arrow_button_->SetBackgroundColor(kChallengeResponseArrowBackgroundColor);
    arrow_button_->SetFocusPainter(nullptr);
    arrow_button_->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_ASH_LOGIN_START_SMART_CARD_AUTH_BUTTON_ACCESSIBLE_NAME));

    arrow_to_icon_spacer_ = AddChildView(std::make_unique<NonAccessibleView>());
    arrow_to_icon_spacer_->SetPreferredSize(
        gfx::Size(0, GetArrowToIconSpacerHeight()));

    icon_ = AddChildView(std::make_unique<views::ImageView>());
    icon_->SetImage(GetImageForIcon());

    auto* icon_to_label_spacer =
        AddChildView(std::make_unique<NonAccessibleView>());
    icon_to_label_spacer->SetPreferredSize(
        gfx::Size(0, kSpacingBetweenChallengeResponseIconAndLabelDp));

    label_ = AddChildView(std::make_unique<views::Label>(
        GetTextForLabel(), views::style::CONTEXT_LABEL,
        views::style::STYLE_PRIMARY));
    label_->SetEnabledColor(SK_ColorWHITE);
    label_->SetSubpixelRenderingEnabled(false);
    label_->SetFontList(views::Label::GetDefaultFontList().Derive(
        /*size_delta=*/1, gfx::Font::FontStyle::ITALIC,
        gfx::Font::Weight::NORMAL));
  }

  ~ChallengeResponseView() override = default;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    if (sender == arrow_button_) {
      DCHECK_NE(state_, State::kAuthenticating);
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
          base::BindRepeating(&ChallengeResponseView::SetState,
                              base::Unretained(this), State::kInitial));
    }

    arrow_button_->SetVisible(state_ != State::kAuthenticating);
    arrow_to_icon_spacer_->SetPreferredSize(
        gfx::Size(0, GetArrowToIconSpacerHeight()));
    icon_->SetImage(GetImageForIcon());
    label_->SetText(GetTextForLabel());

    Layout();
  }

 private:
  int GetArrowToIconSpacerHeight() const {
    int spacer_height = kSpacingBetweenChallengeResponseArrowAndIconDp;
    // During authentication, the arrow button is hidden, so the spacer should
    // consume this space to avoid moving controls below it.
    if (state_ == State::kAuthenticating)
      spacer_height += kChallengeResponseArrowSizeDp;
    return spacer_height;
  }

  gfx::ImageSkia GetImageForIcon() const {
    switch (state_) {
      case State::kInitial:
      case State::kAuthenticating:
        return gfx::CreateVectorIcon(kLockScreenSmartCardIcon,
                                     kChallengeResponseIconSizeDp,
                                     SK_ColorWHITE);
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
  DisabledAuthMessageView() {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        gfx::Insets(kDisabledAuthMessageVerticalBorderDp,
                    kDisabledAuthMessageHorizontalBorderDp),
        kDisabledAuthMessageChildrenSpacingDp));
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetPreferredSize(
        gfx::Size(kDisabledAuthMessageWidthDp, kDisabledAuthMessageHeightDp));
    SetFocusBehavior(FocusBehavior::ALWAYS);
    message_icon_ = new views::ImageView();
    message_icon_->SetPreferredSize(gfx::Size(kDisabledAuthMessageIconSizeDp,
                                              kDisabledAuthMessageIconSizeDp));
    message_icon_->SetImage(
        gfx::CreateVectorIcon(kLockScreenTimeLimitMoonIcon,
                              kDisabledAuthMessageIconSizeDp, SK_ColorWHITE));
    AddChildView(message_icon_);

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
  }

  ~DisabledAuthMessageView() override = default;

  // Set the parameters needed to render the message.
  void SetAuthDisabledMessage(const AuthDisabledData& auth_disabled_data) {
    LockScreenMessage message = GetLockScreenMessage(
        auth_disabled_data.reason, auth_disabled_data.auth_reenabled_time,
        auth_disabled_data.device_used_time);
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
    SkColor color = Shell::Get()->wallpaper_controller()->GetProminentColor(
        color_utils::ColorProfile(color_utils::LumaRange::NORMAL,
                                  color_utils::SaturationRange::MUTED));
    if (color == kInvalidWallpaperColor || color == SK_ColorTRANSPARENT)
      color = kDisabledAuthMessageBubbleColor;
    flags.setColor(color);
    canvas->DrawRoundRect(GetContentsBounds(),
                          kDisabledAuthMessageRoundedCornerRadiusDp, flags);
  }
  void RequestFocus() override { message_title_->RequestFocus(); }

 private:
  views::Label* message_title_;
  views::Label* message_contents_;
  views::ImageView* message_icon_;

  DISALLOW_COPY_AND_ASSIGN(DisabledAuthMessageView);
};

struct LoginAuthUserView::AnimationState {
  explicit AnimationState(LoginAuthUserView* view) {
    non_pin_y_start_in_screen = view->GetBoundsInScreen().y();
    pin_start_in_screen = view->pin_view_->GetBoundsInScreen().origin();

    had_pin = (view->auth_methods() & LoginAuthUserView::AUTH_PIN) != 0;
    had_password =
        (view->auth_methods() & LoginAuthUserView::AUTH_PASSWORD) != 0;
    had_fingerprint =
        (view->auth_methods() & LoginAuthUserView::AUTH_FINGERPRINT) != 0;
  }

  int non_pin_y_start_in_screen = 0;
  gfx::Point pin_start_in_screen;
  bool had_pin = false;
  bool had_password = false;
  bool had_fingerprint = false;
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

views::Button* LoginAuthUserView::TestApi::online_sign_in_message() const {
  return view_->online_sign_in_message_;
}

views::View* LoginAuthUserView::TestApi::disabled_auth_message() const {
  return view_->disabled_auth_message_;
}

views::Button* LoginAuthUserView::TestApi::external_binary_auth_button() const {
  return view_->external_binary_auth_button_;
}

views::Button* LoginAuthUserView::TestApi::external_binary_enrollment_button()
    const {
  return view_->external_binary_enrollment_button_;
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
      LoginDisplayStyle::kLarge, true /*show_dropdown*/, false /*show_domain*/,
      base::BindRepeating(&LoginAuthUserView::OnUserViewTap,
                          base::Unretained(this)),
      callbacks.on_remove_warning_shown, callbacks.on_remove);
  user_view_ = user_view.get();

  auto password_view = std::make_unique<LoginPasswordView>();
  password_view_ = password_view.get();
  password_view->SetPaintToLayer();  // Needed for opacity animation.
  password_view->layer()->SetFillsBoundsOpaquely(false);

  auto pin_view = std::make_unique<LoginPinView>(
      LoginPinView::Style::kAlphanumeric,
      base::BindRepeating(&LoginPasswordView::InsertNumber,
                          base::Unretained(password_view.get())),
      base::BindRepeating(&LoginPasswordView::Backspace,
                          base::Unretained(password_view.get())),
      base::BindRepeating(&LoginAuthUserView::OnPinBack,
                          base::Unretained(this)));
  pin_view_ = pin_view.get();
  DCHECK(pin_view_->layer());

  auto padding_below_password_view = std::make_unique<NonAccessibleView>();
  padding_below_password_view->SetPreferredSize(gfx::Size(
      kNonEmptyWidthDp, kDistanceBetweenPasswordFieldAndPinKeyboardDp));
  padding_below_password_view_ = padding_below_password_view.get();

  // Initialization of |password_view| is deferred because it needs the
  // |pin_view| pointer.
  password_view->Init(
      base::Bind(&LoginAuthUserView::OnAuthSubmit, base::Unretained(this)),
      base::Bind(&LoginPinView::OnPasswordTextChanged,
                 base::Unretained(pin_view.get())),
      callbacks.on_easy_unlock_icon_hovered,
      callbacks.on_easy_unlock_icon_tapped);

  auto online_sign_in_message = std::make_unique<SystemButton>(
      this, l10n_util::GetStringUTF16(IDS_ASH_LOGIN_SIGN_IN_REQUIRED_MESSAGE));
  online_sign_in_message_ = online_sign_in_message.get();

  auto disabled_auth_message = std::make_unique<DisabledAuthMessageView>();
  disabled_auth_message_ = disabled_auth_message.get();

  auto fingerprint_view = std::make_unique<FingerprintView>();
  fingerprint_view_ = fingerprint_view.get();

  auto challenge_response_view =
      std::make_unique<ChallengeResponseView>(base::BindRepeating(
          &LoginAuthUserView::AttemptAuthenticateWithChallengeResponse,
          weak_factory_.GetWeakPtr()));
  challenge_response_view_ = challenge_response_view.get();

  // TODO(jdufault): Implement real UI.
  external_binary_auth_button_ =
      views::MdTextButton::Create(
          this, base::ASCIIToUTF16("Authenticate with external binary"))
          .release();
  external_binary_enrollment_button_ =
      views::MdTextButton::Create(
          this, base::ASCIIToUTF16("Enroll with external binary"))
          .release();

  SetPaintToLayer(ui::LayerType::LAYER_NOT_DRAWN);

  // Wrap the password view with a container having the fill layout, so that
  // it's possible to hide the password view while continuing to consume the
  // same amount of space, which prevents the user view from shrinking. In the
  // cases when other controls need to be rendered in this space, the whole
  // container gets hidden.
  auto password_view_container = std::make_unique<NonAccessibleView>();
  password_view_container->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  password_view_container->AddChildView(std::move(password_view));
  password_view_container_ = password_view_container.get();

  // Build layout.
  auto wrapped_password_view = std::make_unique<NonAccessibleView>();
  wrapped_password_view->SetLayoutManager(
      std::make_unique<views::FlexLayout>());
  wrapped_password_view->AddChildView(std::move(password_view_container));
  auto wrapped_online_sign_in_message_view =
      login_views_utils::WrapViewForPreferredSize(
          std::move(online_sign_in_message));
  auto wrapped_disabled_auth_message_view =
      login_views_utils::WrapViewForPreferredSize(
          std::move(disabled_auth_message));
  auto wrapped_user_view =
      login_views_utils::WrapViewForPreferredSize(std::move(user_view));
  auto wrapped_pin_view =
      login_views_utils::WrapViewForPreferredSize(std::move(pin_view));
  auto wrapped_fingerprint_view =
      login_views_utils::WrapViewForPreferredSize(std::move(fingerprint_view));
  auto wrapped_challenge_response_view =
      login_views_utils::WrapViewForPreferredSize(
          std::move(challenge_response_view));
  auto wrapped_external_binary_view =
      login_views_utils::WrapViewForPreferredSize(
          base::WrapUnique(external_binary_auth_button_));
  auto wrapped_external_binary_enrollment_view =
      login_views_utils::WrapViewForPreferredSize(
          base::WrapUnique(external_binary_enrollment_button_));
  auto wrapped_padding_below_password_view =
      login_views_utils::WrapViewForPreferredSize(
          std::move(padding_below_password_view));

  // Add views in tabbing order; they are rendered in a different order below.
  views::View* wrapped_password_view_ptr =
      AddChildView(std::move(wrapped_password_view));
  views::View* wrapped_online_sign_in_message_view_ptr =
      AddChildView(std::move(wrapped_online_sign_in_message_view));
  views::View* wrapped_disabled_auth_message_view_ptr =
      AddChildView(std::move(wrapped_disabled_auth_message_view));
  views::View* wrapped_pin_view_ptr = AddChildView(std::move(wrapped_pin_view));
  views::View* wrapped_fingerprint_view_ptr =
      AddChildView(std::move(wrapped_fingerprint_view));
  views::View* wrapped_challenge_response_view_ptr =
      AddChildView(std::move(wrapped_challenge_response_view));
  views::View* wrapped_external_binary_view_ptr =
      AddChildView(std::move(wrapped_external_binary_view));
  views::View* wrapped_external_binary_enrollment_view_ptr =
      AddChildView(std::move(wrapped_external_binary_enrollment_view));
  views::View* wrapped_user_view_ptr =
      AddChildView(std::move(wrapped_user_view));
  views::View* wrapped_padding_below_password_view_ptr =
      AddChildView(std::move(wrapped_padding_below_password_view));

  // Use views::GridLayout instead of views::BoxLayout because views::BoxLayout
  // lays out children according to the view->children order.
  views::GridLayout* grid_layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  views::ColumnSet* column_set = grid_layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::CENTER, views::GridLayout::LEADING,
                        0 /*resize_percent*/, views::GridLayout::USE_PREF,
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
  add_padding(kDistanceBetweenUserViewAndPasswordDp);
  add_view(wrapped_password_view_ptr);
  add_view(wrapped_online_sign_in_message_view_ptr);
  add_view(wrapped_disabled_auth_message_view_ptr);
  add_view(wrapped_padding_below_password_view_ptr);
  add_view(wrapped_pin_view_ptr);
  add_view(wrapped_fingerprint_view_ptr);
  add_view(wrapped_challenge_response_view_ptr);
  add_view(wrapped_external_binary_view_ptr);
  add_view(wrapped_external_binary_enrollment_view_ptr);
  add_padding(kDistanceFromPinKeyboardToBigUserViewBottomDp);

  // Update authentication UI.
  SetAuthMethods(auth_methods_, false /*can_use_pin*/);
  user_view_->UpdateForUser(user, false /*animate*/);
}

LoginAuthUserView::~LoginAuthUserView() {
  // Abort the unfinished security token PIN request, if there's one, so that
  // the callers can do all necessary cleanup.
  AbortSecurityTokenPinRequest();
}

void LoginAuthUserView::SetAuthMethods(uint32_t auth_methods,
                                       bool can_use_pin) {
  can_use_pin_ = can_use_pin;
  bool had_password = HasAuthMethod(AUTH_PASSWORD);

  auth_methods_ = static_cast<AuthMethods>(auth_methods);
  bool has_password = HasAuthMethod(AUTH_PASSWORD);
  bool has_pin_pad = HasAuthMethod(AUTH_PIN);
  bool has_tap = HasAuthMethod(AUTH_TAP);
  bool force_online_sign_in = HasAuthMethod(AUTH_ONLINE_SIGN_IN);
  bool has_fingerprint = HasAuthMethod(AUTH_FINGERPRINT);
  bool has_external_binary = HasAuthMethod(AUTH_EXTERNAL_BINARY);
  bool has_challenge_response = HasAuthMethod(AUTH_CHALLENGE_RESPONSE);
  bool auth_disabled = HasAuthMethod(AUTH_DISABLED);

  if (auth_disabled) {
    // The PIN UI cannot be displayed, so abort the security token PIN request,
    // if there's one.
    AbortSecurityTokenPinRequest();
  }

  if (security_token_pin_request_) {
    // The security token PIN request is a special mode that uses the password
    // and the PIN views, regardless of the user's auth methods.
    has_password = true;
    has_pin_pad = true;
    has_tap = false;
    force_online_sign_in = false;
    has_fingerprint = false;
    has_external_binary = false;
    has_challenge_response = false;
  }

  bool hide_auth = auth_disabled || force_online_sign_in;

  online_sign_in_message_->SetVisible(force_online_sign_in);
  disabled_auth_message_->SetVisible(auth_disabled);
  if (auth_disabled)
    disabled_auth_message_->RequestFocus();

  // Adjust the PIN keyboard visibility before the password textfield's one, so
  // that when both are about to be hidden the focus doesn't jump to the "1"
  // keyboard button, causing unexpected accessibility effects.
  pin_view_->SetVisible(has_pin_pad);

  pin_view_->SetBackButtonVisible(security_token_pin_request_.has_value());

  password_view_->SetEnabled(has_password);
  password_view_->SetEnabledOnEmptyPassword(has_tap);
  password_view_->SetFocusEnabledForChildViews(has_password);
  password_view_->SetVisible(!hide_auth && has_password);
  password_view_->layer()->SetOpacity(has_password ? 1 : 0);
  password_view_container_->SetVisible(has_password || !has_challenge_response);

  if (!had_password && has_password)
    password_view_->RequestFocus();

  fingerprint_view_->SetVisible(has_fingerprint);
  challenge_response_view_->SetVisible(has_challenge_response);
  external_binary_auth_button_->SetVisible(has_external_binary);
  external_binary_enrollment_button_->SetVisible(has_external_binary);

  if (has_external_binary) {
    power_manager_client_observer_.Add(chromeos::PowerManagerClient::Get());
  }

  int padding_view_height = kDistanceBetweenPasswordFieldAndPinKeyboardDp;
  if (has_fingerprint && !has_pin_pad) {
    padding_view_height = kDistanceBetweenPasswordFieldAndFingerprintViewDp;
  } else if (has_challenge_response && !has_pin_pad) {
    padding_view_height =
        kDistanceBetweenPasswordFieldAndChallengeResponseViewDp;
  }
  padding_below_password_view_->SetPreferredSize(
      gfx::Size(kNonEmptyWidthDp, padding_view_height));

  // Note: both |security_token_pin_request_| and |has_tap| must have higher
  // priority than |has_pin_pad| when determining the placeholder.
  if (security_token_pin_request_) {
    password_view_->SetPlaceholderText(l10n_util::GetStringUTF16(
        IDS_ASH_LOGIN_POD_PASSWORD_SMART_CARD_PIN_PLACEHOLDER));
  } else if (has_tap) {
    password_view_->SetPlaceholderText(
        l10n_util::GetStringUTF16(IDS_ASH_LOGIN_POD_PASSWORD_TAP_PLACEHOLDER));
  } else if (can_use_pin) {
    password_view_->SetPlaceholderText(
        l10n_util::GetStringUTF16(IDS_ASH_LOGIN_POD_PASSWORD_PIN_PLACEHOLDER));
  } else {
    password_view_->SetPlaceholderText(
        l10n_util::GetStringUTF16(IDS_ASH_LOGIN_POD_PASSWORD_PLACEHOLDER));
  }
  const std::string& user_display_email =
      current_user().basic_user_info.display_email;
  if (security_token_pin_request_) {
    password_view_->SetAccessibleName(l10n_util::GetStringFUTF16(
        IDS_ASH_LOGIN_POD_SMART_CARD_PIN_FIELD_ACCESSIBLE_NAME,
        base::UTF8ToUTF16(user_display_email)));
  } else {
    password_view_->SetAccessibleName(l10n_util::GetStringFUTF16(
        IDS_ASH_LOGIN_POD_PASSWORD_FIELD_ACCESSIBLE_NAME,
        base::UTF8ToUTF16(user_display_email)));
  }

  // Only the active auth user view has a password displayed. If that is the
  // case, then render the user view as if it was always focused, since clicking
  // on it will not do anything (such as swapping users).
  user_view_->SetForceOpaque(has_password || hide_auth);
  user_view_->SetTapEnabled(!has_password);
  // Tapping the user view will trigger the online sign-in flow when
  // |force_online_sign_in| is true.
  if (force_online_sign_in)
    user_view_->RequestFocus();

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

  DCHECK(!cached_animation_state_);
  cached_animation_state_ = std::make_unique<AnimationState>(this);
}

void LoginAuthUserView::ApplyAnimationPostLayout() {
  DCHECK(cached_animation_state_);

  bool has_password = (auth_methods() & AUTH_PASSWORD) != 0;
  bool has_pin = (auth_methods() & AUTH_PIN) != 0;
  bool has_fingerprint = (auth_methods() & AUTH_FINGERPRINT) != 0;

  ////////
  // Animate the user info (ie, icon, name) up or down the screen.

  int non_pin_y_end_in_screen = GetBoundsInScreen().y();

  // Transform the layer so the user view renders where it used to be. This
  // requires a y offset.
  // Note: Doing this animation via ui::ScopedLayerAnimationSettings works, but
  // it seems that the timing gets slightly out of sync with the PIN animation.
  auto move_to_center = std::make_unique<ui::InterpolatedTranslation>(
      gfx::PointF(0, cached_animation_state_->non_pin_y_start_in_screen -
                         non_pin_y_end_in_screen),
      gfx::PointF());
  auto transition =
      ui::LayerAnimationElement::CreateInterpolatedTransformElement(
          std::move(move_to_center),
          base::TimeDelta::FromMilliseconds(
              login_constants::kChangeUserAnimationDurationMs));
  transition->set_tween_type(gfx::Tween::Type::FAST_OUT_SLOW_IN);
  layer()->GetAnimator()->StartAnimation(
      new ui::LayerAnimationSequence(std::move(transition)));

  ////////
  // Fade the password view if it is being hidden or shown.

  if (cached_animation_state_->had_password != has_password) {
    float opacity_start = 0, opacity_end = 1;
    if (!has_password)
      std::swap(opacity_start, opacity_end);

    if (cached_animation_state_->had_password)
      password_view_->SetVisible(true);

    password_view_->layer()->SetOpacity(opacity_start);

    {
      ui::ScopedLayerAnimationSettings settings(
          password_view_->layer()->GetAnimator());
      settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
          login_constants::kChangeUserAnimationDurationMs));
      settings.SetTweenType(gfx::Tween::Type::FAST_OUT_SLOW_IN);
      if (cached_animation_state_->had_password && !has_password) {
        settings.AddObserver(
            new ClearPasswordAndHideAnimationObserver(password_view_));
      }

      password_view_->layer()->SetOpacity(opacity_end);
    }
  }

  ////////
  // Grow/shrink the PIN keyboard if it is being hidden or shown.

  if (cached_animation_state_->had_pin != has_pin) {
    if (!has_pin) {
      gfx::Point pin_end_in_screen = pin_view_->GetBoundsInScreen().origin();
      gfx::Rect pin_bounds = pin_view_->bounds();
      pin_bounds.set_x(cached_animation_state_->pin_start_in_screen.x() -
                       pin_end_in_screen.x());
      pin_bounds.set_y(cached_animation_state_->pin_start_in_screen.y() -
                       pin_end_in_screen.y());

      // Since PIN is disabled, the previous Layout() hid the PIN keyboard.
      // We need to redisplay it where it used to be.
      pin_view_->SetVisible(true);
      pin_view_->SetBoundsRect(pin_bounds);
    }

    auto transition = std::make_unique<PinKeyboardAnimation>(
        has_pin /*grow*/, pin_view_->height(),
        // TODO(https://crbug.com/955119): Implement proper animation.
        base::TimeDelta::FromMilliseconds(
            login_constants::kChangeUserAnimationDurationMs / 2.0f),
        gfx::Tween::FAST_OUT_SLOW_IN);
    auto* sequence = new ui::LayerAnimationSequence(std::move(transition));

    // Hide the PIN keyboard after animation if needed.
    if (!has_pin) {
      auto* observer = BuildObserverToHideView(pin_view_);
      sequence->AddObserver(observer);
      observer->SetActive();
    }

    pin_view_->layer()->GetAnimator()->ScheduleAnimation(sequence);
  }

  ////////
  // Fade the fingerprint view if it is being hidden or shown.

  if (cached_animation_state_->had_fingerprint != has_fingerprint) {
    float opacity_start = 0, opacity_end = 1;
    if (!has_fingerprint)
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

  cached_animation_state_.reset();
}

void LoginAuthUserView::UpdateForUser(const LoginUserInfo& user) {
  // Abort the security token PIN request associated with the previous user, if
  // there was any.
  AbortSecurityTokenPinRequest();
  const bool user_changed = current_user().basic_user_info.account_id !=
                            user.basic_user_info.account_id;
  user_view_->UpdateForUser(user, true /*animate*/);
  if (user_changed)
    password_view_->Clear();
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
  disabled_auth_message_->SetAuthDisabledMessage(auth_disabled_data);
  Layout();
}

void LoginAuthUserView::RequestSecurityTokenPin(
    SecurityTokenPinRequest request) {
  // The caller must prevent an overlapping PIN request before the previous one
  // completed.
  DCHECK(!security_token_pin_request_ ||
         !security_token_pin_request_->pin_entered_callback);

  security_token_pin_request_ = std::move(request);
  password_view_->Clear();
  password_view_->SetReadOnly(false);
  // Trigger SetAuthMethods() with the same parameters but after
  // |on_security_token_pin_requested_| has been set, so that it updates the UI
  // elements in order to show the security token PIN request.
  SetAuthMethods(auth_methods_, can_use_pin_);
}

void LoginAuthUserView::ClearSecurityTokenPinRequest() {
  AbortSecurityTokenPinRequest();

  // Revert the UI back to the normal user authentication controls.
  password_view_->Clear();
  password_view_->SetReadOnly(false);
  SetAuthMethods(auth_methods_, can_use_pin_);
}

const LoginUserInfo& LoginAuthUserView::current_user() const {
  return user_view_->current_user();
}

gfx::Size LoginAuthUserView::CalculatePreferredSize() const {
  gfx::Size size = views::View::CalculatePreferredSize();
  // Make sure we are at least as big as the user view. If we do not do this the
  // view will be below minimum size when no auth methods are displayed.
  size.SetToMax(user_view_->GetPreferredSize());
  return size;
}

void LoginAuthUserView::RequestFocus() {
  password_view_->RequestFocus();
}

void LoginAuthUserView::ButtonPressed(views::Button* sender,
                                      const ui::Event& event) {
  DCHECK(sender == online_sign_in_message_ ||
         sender == external_binary_auth_button_ ||
         sender == external_binary_enrollment_button_);
  if (sender == online_sign_in_message_) {
    OnOnlineSignInMessageTap();
  } else if (sender == external_binary_auth_button_) {
    AttemptAuthenticateWithExternalBinary();
  } else if (sender == external_binary_enrollment_button_) {
    password_view_->SetReadOnly(true);
    external_binary_auth_button_->SetEnabled(false);
    external_binary_enrollment_button_->SetEnabled(false);
    Shell::Get()->login_screen_controller()->EnrollUserWithExternalBinary(
        base::BindOnce(&LoginAuthUserView::OnEnrollmentComplete,
                       weak_factory_.GetWeakPtr()));
  }
}

void LoginAuthUserView::LidEventReceived(
    chromeos::PowerManagerClient::LidState state,
    const base::TimeTicks& timestamp) {
  if (state == chromeos::PowerManagerClient::LidState::OPEN)
    AttemptAuthenticateWithExternalBinary();
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
  if (security_token_pin_request_) {
    std::move(security_token_pin_request_->pin_entered_callback)
        .Run(base::UTF16ToUTF8(password));
  } else {
    Shell::Get()->login_screen_controller()->AuthenticateUserWithPasswordOrPin(
        current_user().basic_user_info.account_id, base::UTF16ToUTF8(password),
        can_use_pin_,
        base::BindOnce(&LoginAuthUserView::OnAuthComplete,
                       weak_factory_.GetWeakPtr()));
  }
}

void LoginAuthUserView::OnAuthComplete(base::Optional<bool> auth_success) {
  // Clear the password only if auth fails. Make sure to keep the password view
  // disabled even if auth succeededs, as if the user submits a password while
  // animating the next lock screen will not work as expected. See
  // https://crbug.com/808486.
  if (!auth_success.has_value() || !auth_success.value()) {
    password_view_->Clear();
    password_view_->SetReadOnly(false);
    external_binary_auth_button_->SetEnabled(true);
    external_binary_enrollment_button_->SetEnabled(true);
  }

  on_auth_.Run(auth_success.value(), /*display_error_messages=*/true);
}

void LoginAuthUserView::OnChallengeResponseAuthComplete(
    base::Optional<bool> auth_success) {
  if (!auth_success.has_value() || !auth_success.value()) {
    password_view_->Clear();
    password_view_->SetReadOnly(false);
    challenge_response_view_->SetState(ChallengeResponseView::State::kFailure);
  }

  on_auth_.Run(auth_success.value_or(false), /*display_error_messages=*/false);
}

void LoginAuthUserView::OnEnrollmentComplete(
    base::Optional<bool> enrollment_success) {
  password_view_->SetReadOnly(false);
  external_binary_auth_button_->SetEnabled(true);
  external_binary_enrollment_button_->SetEnabled(true);

  std::string result_message;
  if (!enrollment_success.has_value()) {
    result_message = "Enrollment attempt failed to received response.";
  } else {
    result_message = enrollment_success.value() ? "Enrollment successful."
                                                : "Enrollment failed.";
  }

  ToastData toast_data("EnrollmentToast", base::ASCIIToUTF16(result_message),
                       2000, base::nullopt, true /*visible_on_lock_screen*/);
  Shell::Get()->toast_manager()->Show(toast_data);
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
      true /*can_close*/, current_user().basic_user_info.account_id);
}

void LoginAuthUserView::OnPinBack() {
  // Exiting from the PIN keyboard during a security token PIN request should
  // abort it. Note that the back button isn't shown when the PIN view is used
  // in other contexts.
  DCHECK(security_token_pin_request_);
  ClearSecurityTokenPinRequest();
}

bool LoginAuthUserView::HasAuthMethod(AuthMethods auth_method) const {
  return (auth_methods_ & auth_method) != 0;
}

void LoginAuthUserView::AttemptAuthenticateWithExternalBinary() {
  password_view_->SetReadOnly(true);
  external_binary_auth_button_->SetEnabled(false);
  external_binary_enrollment_button_->SetEnabled(false);
  Shell::Get()->login_screen_controller()->AuthenticateUserWithExternalBinary(
      current_user().basic_user_info.account_id,
      base::BindOnce(&LoginAuthUserView::OnAuthComplete,
                     weak_factory_.GetWeakPtr()));
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

void LoginAuthUserView::AbortSecurityTokenPinRequest() {
  if (!security_token_pin_request_)
    return;
  std::move(security_token_pin_request_->pin_ui_closed_callback).Run();
  security_token_pin_request_.reset();
}

}  // namespace ash
