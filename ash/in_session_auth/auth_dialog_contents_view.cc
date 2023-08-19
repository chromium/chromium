// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/in_session_auth/auth_dialog_contents_view.h"

#include <memory>
#include <utility>

#include "ash/login/resources/grit/login_resources.h"
#include "ash/login/ui/horizontal_image_sequence_animation_decoder.h"
#include "ash/login/ui/login_password_view.h"
#include "ash/login/ui/login_pin_input_view.h"
#include "ash/login/ui/login_pin_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/webauthn_dialog_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace {

constexpr int kContainerPreferredWidth = 340;

constexpr int kBorderTopDp = 36;
constexpr int kBorderLeftDp = 24;
constexpr int kBorderBottomDp = 20;
constexpr int kBorderRightDp = 24;
constexpr int kCornerRadius = 12;

constexpr int kTitleFontSizeDeltaDp = 4;
constexpr int kOriginNameLineHeight = 18;

constexpr int kSpacingAfterAvatar = 18;
constexpr int kSpacingAfterTitle = 8;
constexpr int kSpacingAfterOriginName = 32;
constexpr int kSpacingAfterInputField = 16;

constexpr int kAvatarSizeDp = 36;
constexpr int kFingerprintIconSizeDp = 28;
constexpr int kSpacingBetweenPinPadAndFingerprintIcon = 24;
constexpr int kSpacingBetweenPasswordAndFingerprintIcon = 24;
constexpr int kSpacingBetweenFingerprintIconAndLabelDp = 15;
constexpr int kFingerprintViewWidthDp = 204;
constexpr int kFingerprintFailedAnimationNumFrames = 45;
constexpr base::TimeDelta kResetToDefaultIconDelay = base::Milliseconds(1300);
constexpr base::TimeDelta kResetToDefaultMessageDelay =
    base::Milliseconds(3000);
constexpr base::TimeDelta kFingerprintFailedAnimationDuration =
    base::Milliseconds(700);

constexpr int kSpacingBeforeButtons = 32;

}  // namespace

AuthDialogContentsView::TestApi::TestApi(AuthDialogContentsView* view)
    : view_(view) {}

AuthDialogContentsView::TestApi::~TestApi() = default;

void AuthDialogContentsView::TestApi::PasswordOrPinAuthComplete(
    bool authenticated_by_pin,
    bool success,
    bool can_use_pin) const {
  view_->OnPasswordOrPinAuthComplete(authenticated_by_pin, success,
                                     can_use_pin);
}

void AuthDialogContentsView::TestApi::FingerprintAuthComplete(
    bool success,
    FingerprintState fingerprint_state) const {
  view_->OnFingerprintAuthComplete(success, fingerprint_state);
}

raw_ptr<LoginPasswordView, ExperimentalAsh>
AuthDialogContentsView::TestApi::GetPasswordView() const {
  return view_->password_view_;
}

raw_ptr<LoginPasswordView, ExperimentalAsh>
AuthDialogContentsView::TestApi::GetPinTextInputView() const {
  return view_->pin_text_input_view_;
}

// Consists of fingerprint icon view and a label.
class AuthDialogContentsView::FingerprintView : public views::View {
 public:
  // Use a subclass that inherit views::Label so that GetAccessibleNodeData
  // override is respected.
  class FingerprintLabel : public views::Label {
   public:
    // views::View
    void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
      node_data->role = ax::mojom::Role::kStaticText;
      node_data->SetNameChecked(GetAccessibleName());
    }
  };

  FingerprintView() {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(),
        kSpacingBetweenFingerprintIconAndLabelDp));
    layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);

    icon_ = AddChildView(std::make_unique<AnimatedRoundedImageView>(
        gfx::Size(kFingerprintIconSizeDp, kFingerprintIconSizeDp),
        0 /*corner_radius*/));

    label_ = AddChildView(std::make_unique<FingerprintLabel>());
    label_->SetSubpixelRenderingEnabled(false);
    label_->SetAutoColorReadabilityEnabled(false);
    label_->SetEnabledColorId(kColorAshTextColorPrimary);
    label_->SetMultiLine(true);
    label_->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  }
  FingerprintView(const FingerprintView&) = delete;
  FingerprintView& operator=(const FingerprintView&) = delete;
  ~FingerprintView() override = default;

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    if (state_ != FingerprintState::DISABLED_FROM_ATTEMPTS) {
      SetIcon(state_);
    }
  }

  void AddedToWidget() override { DisplayCurrentState(); }

  void SetState(FingerprintState state) {
    if (state_ == state) {
      return;
    }

    state_ = state;
    DisplayCurrentState();
  }

  void SetCanUsePin(bool can_use_pin) {
    if (can_use_pin_ == can_use_pin) {
      return;
    }

    can_use_pin_ = can_use_pin;
    DisplayCurrentState();
  }

  // Notify the user of the fingerprint auth result. Should be called after
  // SetState. If fingerprint auth failed and retry is allowed, reset to
  // default state after animation.
  void NotifyFingerprintAuthResult(bool success) {
    reset_state_.Stop();
    if (state_ == FingerprintState::DISABLED_FROM_ATTEMPTS) {
      label_->SetText(l10n_util::GetStringUTF16(
          IDS_ASH_IN_SESSION_AUTH_FINGERPRINT_DISABLED_FROM_ATTEMPTS));
      label_->SetAccessibleName(l10n_util::GetStringUTF16(
          IDS_ASH_IN_SESSION_AUTH_FINGERPRINT_ACCESSIBLE_DISABLED_FROM_ATTEMPTS));
    } else if (success) {
      label_->SetText(l10n_util::GetStringUTF16(
          IDS_ASH_IN_SESSION_AUTH_FINGERPRINT_SUCCESS));
      label_->SetAccessibleName(l10n_util::GetStringUTF16(
          IDS_ASH_IN_SESSION_AUTH_FINGERPRINT_ACCESSIBLE_SUCCESS));
    } else {
      label_->SetText(l10n_util::GetStringUTF16(
          IDS_ASH_IN_SESSION_AUTH_FINGERPRINT_FAILED));
      label_->SetAccessibleName(l10n_util::GetStringUTF16(
          IDS_ASH_IN_SESSION_AUTH_FINGERPRINT_ACCESSIBLE_FAILED));
    }

    if (!success) {
      // This is just to display the "fingerprint auth failure" animation. It
      // does not necessarily mean |state_| is DISABLED_FROM_ATTEMPTS.
      SetIcon(FingerprintState::DISABLED_FROM_ATTEMPTS);
      // base::Unretained is safe because reset_state_ is owned by |this|.
      reset_state_.Start(FROM_HERE, kResetToDefaultIconDelay,
                         base::BindOnce(&FingerprintView::DisplayCurrentState,
                                        base::Unretained(this)));
      label_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert,
                                       true /*send_native_event*/);
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
    if (event->type() != ui::ET_GESTURE_TAP) {
      return;
    }
    if (state_ == FingerprintState::AVAILABLE_DEFAULT ||
        state_ == FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING) {
      SetState(FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING);
      reset_state_.Start(
          FROM_HERE, kResetToDefaultMessageDelay,
          base::BindOnce(&FingerprintView::SetState, base::Unretained(this),
                         FingerprintState::AVAILABLE_DEFAULT));
    }
  }

 private:
  void DisplayCurrentState() {
    SetVisible(state_ != FingerprintState::UNAVAILABLE);
    SetIcon(state_);
    if (state_ != FingerprintState::UNAVAILABLE) {
      std::u16string fingerprint_text =
          l10n_util::GetStringUTF16(GetTextIdFromState());
      label_->SetText(fingerprint_text);
      label_->SetAccessibleName(
          state_ == FingerprintState::DISABLED_FROM_ATTEMPTS
              ? l10n_util::GetStringUTF16(
                    IDS_ASH_IN_SESSION_AUTH_FINGERPRINT_ACCESSIBLE_DISABLED_FROM_ATTEMPTS)
              : fingerprint_text);
    }
  }

  void SetIcon(FingerprintState state) {
    const ui::ColorId color_id =
        (state == FingerprintState::AVAILABLE_DEFAULT ||
                 state == FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING
             ? kColorAshTextColorPrimary
             : kColorAshButtonIconDisabledColor);
    switch (state) {
      case FingerprintState::UNAVAILABLE:
      case FingerprintState::AVAILABLE_DEFAULT:
      case FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING:
      case FingerprintState::DISABLED_FROM_TIMEOUT:
        icon_->SetImage(
            ui::ImageModel::FromVectorIcon(kLockScreenFingerprintIcon, color_id,
                                           kFingerprintIconSizeDp)
                .Rasterize(GetColorProvider()));
        break;
      case FingerprintState::DISABLED_FROM_ATTEMPTS:
        icon_->SetAnimationDecoder(
            std::make_unique<HorizontalImageSequenceAnimationDecoder>(
                *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                    IDR_LOGIN_FINGERPRINT_UNLOCK_SPINNER),
                kFingerprintFailedAnimationDuration,
                kFingerprintFailedAnimationNumFrames),
            AnimatedRoundedImageView::Playback::kSingle);
        break;
    }
  }

  int GetTextIdFromState() const {
    switch (state_) {
      case FingerprintState::AVAILABLE_DEFAULT:
        return IDS_ASH_IN_SESSION_AUTH_FINGERPRINT_AVAILABLE;
      case FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING:
        return IDS_ASH_IN_SESSION_AUTH_FINGERPRINT_TOUCH_SENSOR;
      case FingerprintState::DISABLED_FROM_ATTEMPTS:
        return IDS_ASH_IN_SESSION_AUTH_FINGERPRINT_DISABLED_FROM_ATTEMPTS;
      case FingerprintState::DISABLED_FROM_TIMEOUT:
        if (can_use_pin_) {
          return IDS_ASH_IN_SESSION_AUTH_FINGERPRINT_PIN_OR_PASSWORD_REQUIRED;
        }
        return IDS_ASH_IN_SESSION_AUTH_FINGERPRINT_PASSWORD_REQUIRED;
      case FingerprintState::UNAVAILABLE:
        NOTREACHED();
        return 0;
    }
  }

  raw_ptr<FingerprintLabel, ExperimentalAsh> label_ = nullptr;
  raw_ptr<AnimatedRoundedImageView, ExperimentalAsh> icon_ = nullptr;
  FingerprintState state_ = FingerprintState::AVAILABLE_DEFAULT;
  bool can_use_pin_ = false;
  base::OneShotTimer reset_state_;
};

class AuthDialogContentsView::TitleLabel : public views::Label {
 public:
  TitleLabel() {
    SetSubpixelRenderingEnabled(false);
    SetAutoColorReadabilityEnabled(false);
    SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);

    const gfx::FontList& base_font_list = views::Label::GetDefaultFontList();

    SetFontList(base_font_list.Derive(kTitleFontSizeDeltaDp,
                                      gfx::Font::FontStyle::NORMAL,
                                      gfx::Font::Weight::MEDIUM));
    SetMaximumWidthSingleLine(kContainerPreferredWidth);
    SetElideBehavior(gfx::ElideBehavior::ELIDE_TAIL);

    SetPreferredSize(gfx::Size(kContainerPreferredWidth,
                               GetHeightForWidth(kContainerPreferredWidth)));
    SetHorizontalAlignment(gfx::ALIGN_CENTER);
  }

  bool IsShowingError() const { return is_showing_error_; }

  void ShowTitle() {
    std::u16string title =
        l10n_util::GetStringUTF16(IDS_ASH_IN_SESSION_AUTH_TITLE);
    SetText(title);
    SetEnabledColorId(kColorAshTextColorPrimary);
    is_showing_error_ = false;
    SetAccessibleName(title);
  }

  void ShowError(const std::u16string& error_text) {
    SetText(error_text);
    SetEnabledColorId(kColorAshTextColorAlert);
    is_showing_error_ = true;
    SetAccessibleName(error_text);
    NotifyAccessibilityEvent(ax::mojom::Event::kAlert,
                             true /*send_native_event*/);
  }

  // views::View
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = ax::mojom::Role::kStaticText;
    node_data->SetNameChecked(GetAccessibleName());
  }

 private:
  bool is_showing_error_ = false;
};

AuthDialogContentsView::AuthDialogContentsView(
    uint32_t auth_methods,
    const std::string& origin_name,
    const AuthMethodsMetadata& auth_metadata,
    const UserAvatar& avatar)
    : auth_methods_(auth_methods),
      origin_name_(origin_name),
      auth_metadata_(auth_metadata) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::FLOAT, views::BubbleBorder::STANDARD_SHADOW,
      ui::kColorPrimaryBackground);
  border->SetCornerRadius(kCornerRadius);
  SetBackground(std::make_unique<views::BubbleBackground>(border.get()));
  SetBorder(std::move(border));

  container_ = AddChildView(std::make_unique<NonAccessibleView>());
  container_->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      kBorderTopDp, kBorderLeftDp, kBorderBottomDp, kBorderRightDp)));

  main_layout_ =
      container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  main_layout_->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);
  main_layout_->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  AddAvatarView(avatar);
  AddVerticalSpacing(kSpacingAfterAvatar);
  AddTitleView();
  AddVerticalSpacing(kSpacingAfterTitle);
  AddOriginNameView();
  AddVerticalSpacing(kSpacingAfterOriginName);
  if (auth_methods_ & kAuthPin) {
    if (LoginPinInputView::IsAutosubmitSupported(
            auth_metadata_.autosubmit_pin_length)) {
      pin_autosubmit_on_ = true;
      AddPinDigitInputView();
    } else {
      pin_autosubmit_on_ = false;
      AddPinTextInputView();
    }
    AddVerticalSpacing(kSpacingAfterInputField);
    // PIN pad is always visible regardless of PIN autosubmit status.
    AddPinPadView();
  } else if (auth_methods & kAuthPassword) {
    AddPasswordView();
  }

  if (auth_methods_ & kAuthFingerprint) {
    if (pin_pad_view_) {
      AddVerticalSpacing(kSpacingBetweenPinPadAndFingerprintIcon);
    } else if (password_view_) {
      AddVerticalSpacing(kSpacingBetweenPasswordAndFingerprintIcon);
    }

    fingerprint_view_ =
        container_->AddChildView(std::make_unique<FingerprintView>());
  }

  AddVerticalSpacing(kSpacingBeforeButtons);
  AddActionButtonsView();
}

AuthDialogContentsView::~AuthDialogContentsView() = default;

void AuthDialogContentsView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  views::View::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kDialog;
  node_data->SetName(
      l10n_util::GetStringFUTF16(IDS_ASH_IN_SESSION_AUTH_ACCESSIBLE_TITLE,
                                 base::UTF8ToUTF16(origin_name_)));
}

void AuthDialogContentsView::RequestFocus() {
  if (auth_methods_ == kAuthFingerprint) {
    // There's no PIN input field, so let the focus be on the cancel button
    // (instead of the help button) because it is more often used.
    cancel_button_->RequestFocus();
    return;
  }

  // For other cases, the base method correctly sets focus to the input field.
  views::View::RequestFocus();
}

void AuthDialogContentsView::AddedToWidget() {
  if (auth_methods_ & kAuthFingerprint) {
    fingerprint_view_->SetCanUsePin(auth_methods_ & kAuthPin);
    // Inject a callback from the contents view so that we can show retry
    // prompt.
    WebAuthNDialogController::Get()->AuthenticateUserWithFingerprint(
        base::BindOnce(&AuthDialogContentsView::OnFingerprintAuthComplete,
                       weak_factory_.GetWeakPtr()));
  }
}

void AuthDialogContentsView::AddAvatarView(const UserAvatar& avatar) {
  avatar_view_ =
      container_->AddChildView(std::make_unique<AnimatedRoundedImageView>(
          gfx::Size(kAvatarSizeDp, kAvatarSizeDp),
          kAvatarSizeDp / 2 /*corner_radius*/));
  avatar_view_->SetImage(avatar.image);
}

void AuthDialogContentsView::AddTitleView() {
  title_ = container_->AddChildView(std::make_unique<TitleLabel>());
  title_->ShowTitle();
}

void AuthDialogContentsView::AddOriginNameView() {
  origin_name_view_ =
      container_->AddChildView(std::make_unique<views::Label>());
  origin_name_view_->SetEnabledColorId(kColorAshTextColorSecondary);
  origin_name_view_->SetSubpixelRenderingEnabled(false);
  origin_name_view_->SetAutoColorReadabilityEnabled(false);
  origin_name_view_->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);

  origin_name_view_->SetText(
      l10n_util::GetStringFUTF16(IDS_ASH_IN_SESSION_AUTH_ORIGIN_NAME_PROMPT,
                                 base::UTF8ToUTF16(origin_name_)));
  origin_name_view_->SetMultiLine(true);
  origin_name_view_->SetMaximumWidth(kContainerPreferredWidth);
  origin_name_view_->SetLineHeight(kOriginNameLineHeight);

  origin_name_view_->SetPreferredSize(gfx::Size(
      kContainerPreferredWidth,
      origin_name_view_->GetHeightForWidth(kContainerPreferredWidth)));
  origin_name_view_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
}

void AuthDialogContentsView::AddPinTextInputView() {
  pin_text_input_view_ =
      container_->AddChildView(std::make_unique<LoginPasswordView>());

  pin_text_input_view_->SetPaintToLayer();
  pin_text_input_view_->layer()->SetFillsBoundsOpaquely(false);
  pin_text_input_view_->SetDisplayPasswordButtonVisible(true);
  pin_text_input_view_->SetEnabledOnEmptyPassword(false);
  pin_text_input_view_->SetFocusEnabledForTextfield(true);

  pin_text_input_view_->SetPlaceholderText(
      l10n_util::GetStringUTF16(IDS_ASH_IN_SESSION_AUTH_PIN_PLACEHOLDER));
}

void AuthDialogContentsView::AddPasswordView() {
  password_view_ =
      container_->AddChildView(std::make_unique<LoginPasswordView>());

  password_view_->SetPaintToLayer();
  password_view_->layer()->SetFillsBoundsOpaquely(false);
  password_view_->SetDisplayPasswordButtonVisible(true);
  password_view_->SetEnabledOnEmptyPassword(false);
  password_view_->SetFocusEnabledForTextfield(true);

  password_view_->SetPlaceholderText(
      l10n_util::GetStringUTF16(IDS_ASH_IN_SESSION_AUTH_PASSWORD_PLACEHOLDER));
  password_view_->Init(
      base::BindRepeating(&AuthDialogContentsView::OnAuthSubmit,
                          base::Unretained(this),
                          /*authenticated_by_pin=*/false),
      base::BindRepeating(&AuthDialogContentsView::OnInputTextChanged,
                          base::Unretained(this)));
}

void AuthDialogContentsView::AddPinPadView() {
  DCHECK(auth_methods_ & kAuthPin);
  if (pin_autosubmit_on_) {
    pin_pad_view_ = container_->AddChildView(std::make_unique<LoginPinView>(
        LoginPinView::Style::kAlphanumeric,
        base::BindRepeating(&AuthDialogContentsView::OnInsertDigitFromPinPad,
                            base::Unretained(this)),
        base::BindRepeating(&AuthDialogContentsView::OnBackspaceFromPinPad,
                            base::Unretained(this))));
    pin_digit_input_view_->Init(
        base::BindRepeating(&AuthDialogContentsView::OnAuthSubmit,
                            base::Unretained(this),
                            /*authenticated_by_pin=*/true),
        base::BindRepeating(&AuthDialogContentsView::OnInputTextChanged,
                            base::Unretained(this)));
  } else {
    pin_pad_view_ = container_->AddChildView(std::make_unique<LoginPinView>(
        LoginPinView::Style::kAlphanumeric,
        base::BindRepeating(&AuthDialogContentsView::OnInsertDigitFromPinPad,
                            base::Unretained(this)),
        base::BindRepeating(&AuthDialogContentsView::OnBackspaceFromPinPad,
                            base::Unretained(this)),
        base::BindRepeating(&LoginPasswordView::SubmitPassword,
                            base::Unretained(pin_text_input_view_))));
    pin_text_input_view_->Init(
        base::BindRepeating(&AuthDialogContentsView::OnAuthSubmit,
                            base::Unretained(this),
                            /*authenticated_by_pin=*/true),
        base::BindRepeating(&AuthDialogContentsView::OnInputTextChanged,
                            base::Unretained(this)));
  }
  pin_pad_view_->SetVisible(true);
}

void AuthDialogContentsView::AddPinDigitInputView() {
  pin_digit_input_view_ =
      container_->AddChildView(std::make_unique<LoginPinInputView>());
  pin_digit_input_view_->UpdateLength(auth_metadata_.autosubmit_pin_length);
  pin_digit_input_view_->SetVisible(true);
}

void AuthDialogContentsView::AddVerticalSpacing(int height) {
  auto* spacing =
      container_->AddChildView(std::make_unique<NonAccessibleView>());
  spacing->SetPreferredSize(gfx::Size(kContainerPreferredWidth, height));
}

void AuthDialogContentsView::AddActionButtonsView() {
  action_view_container_ =
      container_->AddChildView(std::make_unique<NonAccessibleView>());
  auto* buttons_layout = action_view_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  buttons_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);

  help_button_ =
      action_view_container_->AddChildView(std::make_unique<views::LabelButton>(
          base::BindRepeating(&AuthDialogContentsView::OnNeedHelpButtonPressed,
                              base::Unretained(this)),
          l10n_util::GetStringUTF16(IDS_ASH_IN_SESSION_AUTH_HELP),
          views::style::CONTEXT_BUTTON));
  help_button_->SetEnabledTextColorIds(kColorAshTextColorPrimary);

  auto* spacing = action_view_container_->AddChildView(
      std::make_unique<NonAccessibleView>());
  buttons_layout->SetFlexForView(spacing, 1);

  cancel_button_ = action_view_container_->AddChildView(
      std::make_unique<views::MdTextButton>(
          base::BindRepeating(&AuthDialogContentsView::OnCancelButtonPressed,
                              base::Unretained(this)),
          l10n_util::GetStringUTF16(IDS_ASH_IN_SESSION_AUTH_CANCEL)));

  action_view_container_->SetPreferredSize(
      gfx::Size(kContainerPreferredWidth, cancel_button_->height()));
}

void AuthDialogContentsView::OnInsertDigitFromPinPad(int digit) {
  // Ignore anything if reached max attempts.
  if (pin_locked_out_) {
    return;
  }

  if (title_->IsShowingError()) {
    title_->ShowTitle();
  }

  if (pin_autosubmit_on_) {
    pin_digit_input_view_->InsertDigit(digit);
  } else {
    pin_text_input_view_->InsertNumber(digit);
  }
}

void AuthDialogContentsView::OnBackspaceFromPinPad() {
  // Ignore anything if reached max attempts.
  if (pin_locked_out_) {
    return;
  }

  if (title_->IsShowingError()) {
    title_->ShowTitle();
  }

  if (pin_autosubmit_on_) {
    pin_digit_input_view_->Backspace();
  } else {
    pin_text_input_view_->Backspace();
  }
}

void AuthDialogContentsView::OnInputTextChanged(bool is_empty) {
  // If the user is interacting with the input field, restore the title (clear
  // error message).
  //
  // If |is_empty| is true, this call may come from resetting
  // |pin_text_input_view_| or |pin_digit_input_view_|, when the error message
  // hasn't been shown and read yet. In this case we don't restore the title.
  if (title_->IsShowingError() && !is_empty) {
    title_->ShowTitle();
  }

  if (pin_pad_view_) {
    pin_pad_view_->OnPasswordTextChanged(is_empty);
  }
}

void AuthDialogContentsView::OnAuthSubmit(bool authenticated_by_pin,
                                          const std::u16string& password) {
  if (authenticated_by_pin) {
    if (pin_autosubmit_on_) {
      pin_digit_input_view_->SetReadOnly(true);
    } else {
      pin_text_input_view_->SetReadOnly(true);
    }
  } else {
    password_view_->SetReadOnly(true);
  }
  WebAuthNDialogController::Get()->AuthenticateUserWithPasswordOrPin(
      base::UTF16ToUTF8(password), authenticated_by_pin,
      base::BindOnce(&AuthDialogContentsView::OnPasswordOrPinAuthComplete,
                     weak_factory_.GetWeakPtr(), authenticated_by_pin));
}

void AuthDialogContentsView::OnPasswordOrPinAuthComplete(
    bool authenticated_by_pin,
    bool success,
    bool can_use_pin) {
  // On success, do nothing, and the dialog will dismiss.
  if (success) {
    return;
  }

  std::u16string error_text;
  if (authenticated_by_pin) {
    pin_locked_out_ = !can_use_pin;
    error_text =
        pin_locked_out_
            ? l10n_util::GetStringUTF16(
                  IDS_ASH_IN_SESSION_AUTH_PIN_TOO_MANY_ATTEMPTS)
            : l10n_util::GetStringUTF16(IDS_ASH_IN_SESSION_AUTH_PIN_INCORRECT);
  } else {
    error_text =
        l10n_util::GetStringUTF16(IDS_ASH_IN_SESSION_AUTH_PASSWORD_INCORRECT);
  }
  title_->ShowError(error_text);

  if (!authenticated_by_pin) {
    password_view_->Reset();
    password_view_->SetReadOnly(false);
  } else if (can_use_pin) {
    if (pin_autosubmit_on_) {
      pin_digit_input_view_->Reset();
      pin_digit_input_view_->SetReadOnly(false);
    } else {
      pin_text_input_view_->Reset();
      pin_text_input_view_->SetReadOnly(false);
    }
  }
}

void AuthDialogContentsView::OnFingerprintAuthComplete(
    bool success,
    FingerprintState fingerprint_state) {
  fingerprint_view_->SetState(fingerprint_state);
  // Prepare for the next fingerprint scan.
  if (!success && fingerprint_state == FingerprintState::AVAILABLE_DEFAULT) {
    WebAuthNDialogController::Get()->AuthenticateUserWithFingerprint(
        base::BindOnce(&AuthDialogContentsView::OnFingerprintAuthComplete,
                       weak_factory_.GetWeakPtr()));
  }
  fingerprint_view_->NotifyFingerprintAuthResult(success);
}

void AuthDialogContentsView::OnCancelButtonPressed(const ui::Event& event) {
  WebAuthNDialogController::Get()->Cancel();
}

void AuthDialogContentsView::OnNeedHelpButtonPressed(const ui::Event& event) {
  WebAuthNDialogController::Get()->OpenInSessionAuthHelpPage();
}

}  // namespace ash
