// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_auth_user_view.h"

#include <map>
#include <memory>

#include "ash/login/login_screen_controller.h"
#include "ash/login/resources/grit/login_resources.h"
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
#include "ash/system/toast/toast_manager.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/user_manager/user.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

namespace ash {
namespace {

constexpr const char kLoginAuthUserViewClassName[] = "LoginAuthUserView";

// Distance between the user view (ie, the icon and name) and the password
// textfield.
const int kDistanceBetweenUserViewAndPasswordDp = 28;

// Distance between the password textfield and the the pin keyboard.
const int kDistanceBetweenPasswordFieldAndPinKeyboardDp = 20;

// Distance from the end of pin keyboard to the bottom of the big user view.
const int kDistanceFromPinKeyboardToBigUserViewBottomDp = 50;

// Distance from the top of the user view to the user icon.
constexpr int kDistanceFromTopOfBigUserViewToUserIconDp = 54;

// The color of the online sign-in message text.
constexpr SkColor kOnlineSignInMessageColor = SkColorSetRGB(0xE6, 0x7C, 0x73);

// The color of the disabled auth message bubble when the color extracted from
// wallpaper is transparent or invalid (i.e. color calculation fails or is
// disabled).
constexpr SkColor kDisabledAuthMessageBubbleColor =
    SkColorSetRGB(0x20, 0x21, 0x24);

constexpr int kFingerprintIconSizeDp = 32;
constexpr int kResetToDefaultIconDelayMs = 1300;
constexpr int kFingerprintIconTopSpacingDp = 20;
constexpr int kSpacingBetweenFingerprintIconAndLabelDp = 15;
constexpr int kFingerprintViewWidthDp = 204;
constexpr int kDistanceBetweenPasswordFieldAndFingerprintViewDp = 90;
constexpr int kFingerprintFailedAnimationDurationMs = 700;
constexpr int kFingerprintFailedAnimationNumFrames = 45;

constexpr int kDisabledAuthMessageVerticalBorderDp = 14;
constexpr int kDisabledAuthMessageHorizontalBorderDp = 0;
constexpr int kDisabledAuthMessageChildrenSpacingDp = 4;
constexpr int kDisabledAuthMessageWidthDp = 204;
constexpr int kDisabledAuthMessageHeightDp = 98;
constexpr int kDisabledAuthMessageIconSizeDp = 24;
constexpr int kDisabledAuthMessageTitleFontSizeDeltaDp = 3;
constexpr int kDisabledAuthMessageContentsFontSizeDeltaDp = -1;
constexpr int kDisabledAuthMessageRoundedCornerRadiusDp = 8;

constexpr int kNonEmptyWidthDp = 1;

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

void DecorateOnlineSignInMessage(views::LabelButton* label_button) {
  label_button->SetPaintToLayer();
  label_button->layer()->SetFillsBoundsOpaquely(false);
  label_button->SetImage(
      views::Button::STATE_NORMAL,
      CreateVectorIcon(kLockScreenAlertIcon, kOnlineSignInMessageColor));
  label_button->SetTextSubpixelRenderingEnabled(false);
  label_button->SetTextColor(views::Button::STATE_NORMAL,
                             kOnlineSignInMessageColor);
  label_button->SetTextColor(views::Button::STATE_HOVERED,
                             kOnlineSignInMessageColor);
  label_button->SetTextColor(views::Button::STATE_PRESSED,
                             kOnlineSignInMessageColor);
  label_button->SetBorder(views::CreateEmptyBorder(gfx::Insets(9, 0)));
}

// The label shown below the fingerprint icon.
class FingerprintLabel : public views::Label {
 public:
  FingerprintLabel() {
    SetSubpixelRenderingEnabled(false);
    SetAutoColorReadabilityEnabled(false);
    SetEnabledColor(login_constants::kAuthMethodsTextColor);

    SetTextBasedOnState(mojom::FingerprintState::AVAILABLE);
  }

  void SetTextBasedOnAuthAttempt(bool success) {
    SetText(l10n_util::GetStringUTF16(
        success ? IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_AUTH_SUCCESS
                : IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_AUTH_FAILED));
    SetAccessibleName(l10n_util::GetStringUTF16(
        success ? IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_ACCESSIBLE_AUTH_SUCCESS
                : IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_ACCESSIBLE_AUTH_FAILED));
  }

  void SetTextBasedOnState(mojom::FingerprintState state) {
    auto get_displayed_id = [&]() {
      switch (state) {
        case mojom::FingerprintState::UNAVAILABLE:
        case mojom::FingerprintState::AVAILABLE:
          return IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_AVAILABLE;
        case mojom::FingerprintState::DISABLED_FROM_ATTEMPTS:
          return IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_DISABLED_FROM_ATTEMPTS;
        case mojom::FingerprintState::DISABLED_FROM_TIMEOUT:
          return IDS_ASH_LOGIN_FINGERPRINT_UNLOCK_DISABLED_FROM_TIMEOUT;
      }
      NOTREACHED();
    };

    auto get_accessible_id = [&]() {
      if (state == mojom::FingerprintState::DISABLED_FROM_ATTEMPTS)
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

}  // namespace

// Consists of fingerprint icon view and a label.
class LoginAuthUserView::FingerprintView : public views::View {
 public:
  FingerprintView() {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetBorder(views::CreateEmptyBorder(kFingerprintIconTopSpacingDp, 0, 0, 0));

    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::kVertical, gfx::Insets(),
        kSpacingBetweenFingerprintIconAndLabelDp));
    layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);

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

  void SetState(mojom::FingerprintState state) {
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
      SetIcon(mojom::FingerprintState::DISABLED_FROM_ATTEMPTS);
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
    SetVisible(state_ != mojom::FingerprintState::UNAVAILABLE &&
               state_ != mojom::FingerprintState::DISABLED_FROM_TIMEOUT);
    SetIcon(state_);
    label_->SetTextBasedOnState(state_);
  }

  void FireAlert() {
    label_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert,
                                     true /*send_native_event*/);
  }

  void SetIcon(mojom::FingerprintState state) {
    switch (state) {
      case mojom::FingerprintState::UNAVAILABLE:
      case mojom::FingerprintState::AVAILABLE:
      case mojom::FingerprintState::DISABLED_FROM_TIMEOUT:
        icon_->SetImage(gfx::CreateVectorIcon(
            kLockScreenFingerprintIcon, kFingerprintIconSizeDp, SK_ColorWHITE));
        break;
      case mojom::FingerprintState::DISABLED_FROM_ATTEMPTS:
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

  bool ShouldFireChromeVoxAlert(mojom::FingerprintState state) {
    return state == mojom::FingerprintState::DISABLED_FROM_ATTEMPTS ||
           state == mojom::FingerprintState::DISABLED_FROM_TIMEOUT;
  }

  FingerprintLabel* label_ = nullptr;
  AnimatedRoundedImageView* icon_ = nullptr;
  base::OneShotTimer reset_state_;
  mojom::FingerprintState state_ = mojom::FingerprintState::AVAILABLE;

  DISALLOW_COPY_AND_ASSIGN(FingerprintView);
};

// The message shown to user when the auth method is |AUTH_DISABLED|.
class LoginAuthUserView::DisabledAuthMessageView : public views::View {
 public:
  DisabledAuthMessageView() {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::kVertical,
        gfx::Insets(kDisabledAuthMessageVerticalBorderDp,
                    kDisabledAuthMessageHorizontalBorderDp),
        kDisabledAuthMessageChildrenSpacingDp));
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetPreferredSize(
        gfx::Size(kDisabledAuthMessageWidthDp, kDisabledAuthMessageHeightDp));
    SetFocusBehavior(FocusBehavior::ALWAYS);
    views::ImageView* alarm_clock_icon = new views::ImageView();
    alarm_clock_icon->SetPreferredSize(gfx::Size(
        kDisabledAuthMessageIconSizeDp, kDisabledAuthMessageIconSizeDp));
    alarm_clock_icon->SetImage(
        gfx::CreateVectorIcon(kLockScreenTimeLimitAlarmIcon, SK_ColorWHITE));
    AddChildView(alarm_clock_icon);

    auto decorate_label = [](views::Label* label) {
      label->SetSubpixelRenderingEnabled(false);
      label->SetAutoColorReadabilityEnabled(false);
      label->SetEnabledColor(SK_ColorWHITE);
      label->SetFocusBehavior(FocusBehavior::ALWAYS);
    };
    message_title_ = new views::Label(
        l10n_util::GetStringUTF16(IDS_ASH_LOGIN_TAKE_BREAK_MESSAGE),
        views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY);
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
    AddChildView(message_contents_);
  }

  ~DisabledAuthMessageView() override = default;

  // Set the time when auth will be reenabled. It will be included in the
  // message.
  void SetAuthReenabledTime(const base::Time& auth_reenabled_time) {
    const std::string time_of_day =
        TimeOfDay::FromTime(auth_reenabled_time).ToString();
    message_contents_->SetText(l10n_util::GetStringFUTF16(
        IDS_ASH_LOGIN_COME_BACK_MESSAGE, base::UTF8ToUTF16(time_of_day)));
    Layout();
  }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);

    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    SkColor color = Shell::Get()->wallpaper_controller()->GetProminentColor(
        color_utils::ColorProfile(color_utils::LumaRange::DARK,
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

LoginAuthUserView::Callbacks::Callbacks() = default;

LoginAuthUserView::Callbacks::Callbacks(const Callbacks& other) = default;

LoginAuthUserView::Callbacks::~Callbacks() = default;

LoginAuthUserView::LoginAuthUserView(const mojom::LoginUserInfoPtr& user,
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
  DCHECK_NE(user->basic_user_info->type,
            user_manager::USER_TYPE_PUBLIC_ACCOUNT);

  // Build child views.
  user_view_ = new LoginUserView(
      LoginDisplayStyle::kLarge, true /*show_dropdown*/, false /*show_domain*/,
      base::BindRepeating(&LoginAuthUserView::OnUserViewTap,
                          base::Unretained(this)),
      callbacks.on_remove_warning_shown, callbacks.on_remove);

  password_view_ = new LoginPasswordView();
  password_view_->SetPaintToLayer();  // Needed for opacity animation.
  password_view_->layer()->SetFillsBoundsOpaquely(false);
  password_view_->UpdateForUser(user);

  pin_view_ =
      new LoginPinView(base::BindRepeating(&LoginPasswordView::InsertNumber,
                                           base::Unretained(password_view_)),
                       base::BindRepeating(&LoginPasswordView::Backspace,
                                           base::Unretained(password_view_)));
  DCHECK(pin_view_->layer());

  padding_below_password_view_ = new NonAccessibleView();
  padding_below_password_view_->SetPreferredSize(gfx::Size(
      kNonEmptyWidthDp, kDistanceBetweenPasswordFieldAndPinKeyboardDp));

  // Initialization of |password_view_| is deferred because it needs the
  // |pin_view_| pointer.
  password_view_->Init(
      base::Bind(&LoginAuthUserView::OnAuthSubmit, base::Unretained(this)),
      base::Bind(&LoginPinView::OnPasswordTextChanged,
                 base::Unretained(pin_view_)),
      callbacks.on_easy_unlock_icon_hovered,
      callbacks.on_easy_unlock_icon_tapped);

  online_sign_in_message_ = new views::LabelButton(
      this, base::UTF8ToUTF16(user->basic_user_info->display_name));
  DecorateOnlineSignInMessage(online_sign_in_message_);

  disabled_auth_message_ = new DisabledAuthMessageView();

  fingerprint_view_ = new FingerprintView();

  // TODO(jdufault): Implement real UI.
  external_binary_auth_button_ = views::MdTextButton::Create(
      this, base::ASCIIToUTF16("Authenticate with external binary"));
  external_binary_enrollment_button_ = views::MdTextButton::Create(
      this, base::ASCIIToUTF16("Enroll with external binary"));

  SetPaintToLayer(ui::LayerType::LAYER_NOT_DRAWN);

  // Build layout.
  // Wrap the password view with a fill layout so that it always consumes space,
  // ie, when the password view is hidden the wrapped view will still consume
  // the same amount of space. This prevents the user view from shrinking.
  auto* wrapped_password_view = new NonAccessibleView();
  wrapped_password_view->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  wrapped_password_view->AddChildView(password_view_);
  auto* wrapped_online_sign_in_message_view =
      login_views_utils::WrapViewForPreferredSize(online_sign_in_message_);
  auto* wrapped_disabled_auth_message_view =
      login_views_utils::WrapViewForPreferredSize(disabled_auth_message_);
  auto* wrapped_user_view =
      login_views_utils::WrapViewForPreferredSize(user_view_);
  auto* wrapped_pin_view =
      login_views_utils::WrapViewForPreferredSize(pin_view_);
  auto* wrapped_fingerprint_view =
      login_views_utils::WrapViewForPreferredSize(fingerprint_view_);
  auto* wrapped_external_binary_view =
      login_views_utils::WrapViewForPreferredSize(external_binary_auth_button_);
  auto* wrapped_external_binary_enrollment_view =
      login_views_utils::WrapViewForPreferredSize(
          external_binary_enrollment_button_);
  auto* wrapped_padding_below_password_view =
      login_views_utils::WrapViewForPreferredSize(padding_below_password_view_);

  // Add views in tabbing order; they are rendered in a different order below.
  AddChildView(wrapped_password_view);
  AddChildView(wrapped_online_sign_in_message_view);
  AddChildView(wrapped_disabled_auth_message_view);
  AddChildView(wrapped_pin_view);
  AddChildView(wrapped_fingerprint_view);
  AddChildView(wrapped_external_binary_view);
  AddChildView(wrapped_external_binary_enrollment_view);
  AddChildView(wrapped_user_view);
  AddChildView(wrapped_padding_below_password_view);

  // Use views::GridLayout instead of views::BoxLayout because views::BoxLayout
  // lays out children according to the view->children order.
  views::GridLayout* grid_layout =
      SetLayoutManager(std::make_unique<views::GridLayout>(this));
  views::ColumnSet* column_set = grid_layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::CENTER, views::GridLayout::LEADING,
                        0 /*resize_percent*/, views::GridLayout::USE_PREF,
                        0 /*fixed_width*/, 0 /*min_width*/);
  auto add_view = [&](views::View* view) {
    grid_layout->StartRow(0 /*vertical_resize*/, 0 /*column_set_id*/);
    grid_layout->AddView(view);
  };
  auto add_padding = [&](int amount) {
    grid_layout->AddPaddingRow(0 /*vertical_resize*/, amount /*size*/);
  };

  // Add views in rendering order.
  add_padding(kDistanceFromTopOfBigUserViewToUserIconDp);
  add_view(wrapped_user_view);
  add_padding(kDistanceBetweenUserViewAndPasswordDp);
  add_view(wrapped_password_view);
  add_view(wrapped_online_sign_in_message_view);
  add_view(wrapped_disabled_auth_message_view);
  add_view(wrapped_padding_below_password_view);
  add_view(wrapped_pin_view);
  add_view(wrapped_fingerprint_view);
  add_view(wrapped_external_binary_view);
  add_view(wrapped_external_binary_enrollment_view);
  add_padding(kDistanceFromPinKeyboardToBigUserViewBottomDp);

  // Update authentication UI.
  SetAuthMethods(auth_methods_, false /*can_use_pin*/);
  user_view_->UpdateForUser(user, false /*animate*/);
}

LoginAuthUserView::~LoginAuthUserView() = default;

void LoginAuthUserView::SetAuthMethods(uint32_t auth_methods,
                                       bool can_use_pin) {
  can_use_pin_ = can_use_pin;
  bool had_password = HasAuthMethod(AUTH_PASSWORD);

  auth_methods_ = static_cast<AuthMethods>(auth_methods);
  bool has_password = HasAuthMethod(AUTH_PASSWORD);
  bool has_pin = HasAuthMethod(AUTH_PIN);
  bool has_tap = HasAuthMethod(AUTH_TAP);
  bool force_online_sign_in = HasAuthMethod(AUTH_ONLINE_SIGN_IN);
  bool has_fingerprint = HasAuthMethod(AUTH_FINGERPRINT);
  bool has_external_binary = HasAuthMethod(AUTH_EXTERNAL_BINARY);
  bool auth_disabled = HasAuthMethod(AUTH_DISABLED);
  bool hide_auth = auth_disabled || force_online_sign_in;

  // implication: if |has_pin| is true, then |can_use_pin| must also be true
  DCHECK(!has_pin || can_use_pin);
  // implication: if |can_use_pin| is false, then |has_pin| must also be false
  DCHECK(can_use_pin || !has_pin);

  online_sign_in_message_->SetVisible(force_online_sign_in);
  disabled_auth_message_->SetVisible(auth_disabled);
  if (auth_disabled)
    disabled_auth_message_->RequestFocus();

  password_view_->SetEnabled(has_password);
  password_view_->SetEnabledOnEmptyPassword(has_tap);
  password_view_->SetFocusEnabledForChildViews(has_password);
  password_view_->SetVisible(!hide_auth && has_password);
  password_view_->layer()->SetOpacity(has_password ? 1 : 0);

  if (!had_password && has_password)
    password_view_->RequestFocus();

  pin_view_->SetVisible(has_pin);
  fingerprint_view_->SetVisible(has_fingerprint);
  external_binary_auth_button_->SetVisible(has_external_binary);
  external_binary_enrollment_button_->SetVisible(has_external_binary);

  int padding_view_height = kDistanceBetweenPasswordFieldAndPinKeyboardDp;
  if (has_fingerprint && !has_pin) {
    padding_view_height = kDistanceBetweenPasswordFieldAndFingerprintViewDp;
  }
  padding_below_password_view_->SetPreferredSize(
      gfx::Size(kNonEmptyWidthDp, padding_view_height));

  // Note: if both |has_tap| and |has_pin| are true, prefer tap placeholder.
  if (has_tap) {
    password_view_->SetPlaceholderText(
        l10n_util::GetStringUTF16(IDS_ASH_LOGIN_POD_PASSWORD_TAP_PLACEHOLDER));
  } else if (has_pin) {
    password_view_->SetPlaceholderText(
        l10n_util::GetStringUTF16(IDS_ASH_LOGIN_POD_PASSWORD_PIN_PLACEHOLDER));
  } else {
    password_view_->SetPlaceholderText(
        l10n_util::GetStringUTF16(IDS_ASH_LOGIN_POD_PASSWORD_PLACEHOLDER));
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
    mojom::EasyUnlockIconId id,
    const base::string16& accessibility_label) {
  password_view_->SetEasyUnlockIcon(id, accessibility_label);
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
        base::TimeDelta::FromMilliseconds(
            login_constants::kChangeUserAnimationDurationMs),
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

void LoginAuthUserView::UpdateForUser(const mojom::LoginUserInfoPtr& user) {
  user_view_->UpdateForUser(user, true /*animate*/);
  password_view_->UpdateForUser(user);
  password_view_->Clear();
  online_sign_in_message_->SetText(
      base::UTF8ToUTF16(user->basic_user_info->display_name));
}

void LoginAuthUserView::SetFingerprintState(mojom::FingerprintState state) {
  fingerprint_view_->SetState(state);
}

void LoginAuthUserView::NotifyFingerprintAuthResult(bool success) {
  fingerprint_view_->NotifyFingerprintAuthResult(success);
}

void LoginAuthUserView::SetAuthReenabledTime(
    const base::Time& auth_reenabled_time) {
  disabled_auth_message_->SetAuthReenabledTime(auth_reenabled_time);
}

const mojom::LoginUserInfoPtr& LoginAuthUserView::current_user() const {
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
         sender == external_binary_auth_button_);
  if (sender == online_sign_in_message_) {
    OnOnlineSignInMessageTap();
  } else if (sender == external_binary_auth_button_) {
    password_view_->SetReadOnly(true);
    external_binary_auth_button_->SetEnabled(false);
    external_binary_enrollment_button_->SetEnabled(false);
    Shell::Get()->login_screen_controller()->AuthenticateUserWithExternalBinary(
        current_user()->basic_user_info->account_id,
        base::BindOnce(&LoginAuthUserView::OnAuthComplete,
                       weak_factory_.GetWeakPtr()));
  } else if (sender == external_binary_enrollment_button_) {
    password_view_->SetReadOnly(true);
    external_binary_auth_button_->SetEnabled(false);
    external_binary_enrollment_button_->SetEnabled(false);
    Shell::Get()->login_screen_controller()->EnrollUserWithExternalBinary(
        base::BindOnce(&LoginAuthUserView::OnEnrollmentComplete,
                       weak_factory_.GetWeakPtr()));
  }
}

void LoginAuthUserView::OnAuthSubmit(const base::string16& password) {
  // Pressing enter when the password field is empty and tap-to-unlock is
  // enabled should attempt unlock.
  if (HasAuthMethod(AUTH_TAP) && password.empty()) {
    Shell::Get()->login_screen_controller()->AuthenticateUserWithEasyUnlock(
        current_user()->basic_user_info->account_id);
    return;
  }

  password_view_->SetReadOnly(true);
  Shell::Get()->login_screen_controller()->AuthenticateUserWithPasswordOrPin(
      current_user()->basic_user_info->account_id, base::UTF16ToUTF8(password),
      can_use_pin_,
      base::BindOnce(&LoginAuthUserView::OnAuthComplete,
                     weak_factory_.GetWeakPtr()));
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

  on_auth_.Run(auth_success.value());
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
        current_user()->basic_user_info->account_id);
  } else if (HasAuthMethod(AUTH_ONLINE_SIGN_IN)) {
    // Tapping anywhere in the user view is the same with tapping the message.
    OnOnlineSignInMessageTap();
  } else {
    on_tap_.Run();
  }
}

void LoginAuthUserView::OnOnlineSignInMessageTap() {
  Shell::Get()->login_screen_controller()->ShowGaiaSignin(
      true /*can_close*/, current_user()->basic_user_info->account_id);
}

bool LoginAuthUserView::HasAuthMethod(AuthMethods auth_method) const {
  return (auth_methods_ & auth_method) != 0;
}

}  // namespace ash
