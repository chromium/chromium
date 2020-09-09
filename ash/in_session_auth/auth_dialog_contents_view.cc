// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/in_session_auth/auth_dialog_contents_view.h"

#include <memory>
#include <utility>

#include "ash/login/resources/grit/login_resources.h"
#include "ash/login/ui/horizontal_image_sequence_animation_decoder.h"
#include "ash/login/ui/login_password_view.h"
#include "ash/login/ui/login_pin_view.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/in_session_auth_dialog_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace {

enum class ButtonId {
  kCancel,
};

// TODO(b/164195709): Move these strings to a grd file.
const char kTitle[] = "Verify it's you";
const char kCancelButtonText[] = "Cancel";

const int kContainerPreferredWidth = 512;
const int kSpacingAfterTitle = 16;

const int kBorderTopDp = 24;
const int kBorderLeftDp = 24;
const int kBorderBottomDp = 20;
const int kBorderRightDp = 24;

const int kTitleFontSizeDeltaDp = 4;

constexpr int kFingerprintIconSizeDp = 28;
constexpr int kFingerprintIconTopSpacingDp = 20;
constexpr int kSpacingBetweenFingerprintIconAndLabelDp = 15;
constexpr int kFingerprintViewWidthDp = 204;
constexpr int kFingerprintFailedAnimationNumFrames = 45;
constexpr base::TimeDelta kResetToDefaultIconDelay =
    base::TimeDelta::FromMilliseconds(1300);
constexpr base::TimeDelta kResetToDefaultMessageDelay =
    base::TimeDelta::FromMilliseconds(3000);
constexpr base::TimeDelta kFingerprintFailedAnimationDuration =
    base::TimeDelta::FromMilliseconds(700);

// 38% opacity.
constexpr SkColor kDisabledFingerprintIconColor =
    SkColorSetA(SK_ColorDKGRAY, 97);

constexpr int kSpacingBeforeButtons = 32;

}  // namespace

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
      node_data->SetName(accessible_name_);
    }

    void SetAccessibleName(const base::string16& name) {
      accessible_name_ = name;
      NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged,
                               true /*send_native_event*/);
    }

   private:
    base::string16 accessible_name_;
  };

  FingerprintView() {
    SetBorder(views::CreateEmptyBorder(kFingerprintIconTopSpacingDp, 0, 0, 0));

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
    label_->SetEnabledColor(SK_ColorDKGRAY);
    label_->SetMultiLine(true);
    label_->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);

    DisplayCurrentState();
  }
  FingerprintView(const FingerprintView&) = delete;
  FingerprintView& operator=(const FingerprintView&) = delete;
  ~FingerprintView() override = default;

  void SetState(FingerprintState state) {
    if (state_ == state)
      return;

    state_ = state;
    DisplayCurrentState();
  }

  void SetCanUsePin(bool can_use_pin) {
    if (can_use_pin_ == can_use_pin)
      return;

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
    if (event->type() != ui::ET_GESTURE_TAP)
      return;
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
      base::string16 fingerprint_text =
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
    const SkColor color =
        (state == FingerprintState::AVAILABLE_DEFAULT ||
                 state == FingerprintState::AVAILABLE_WITH_TOUCH_SENSOR_WARNING
             ? SK_ColorDKGRAY
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
        if (can_use_pin_)
          return IDS_ASH_IN_SESSION_AUTH_FINGERPRINT_PIN_OR_PASSWORD_REQUIRED;
        return IDS_ASH_IN_SESSION_AUTH_FINGERPRINT_PASSWORD_REQUIRED;
      case FingerprintState::UNAVAILABLE:
        NOTREACHED();
        return 0;
    }
  }

  FingerprintLabel* label_ = nullptr;
  AnimatedRoundedImageView* icon_ = nullptr;
  FingerprintState state_ = FingerprintState::AVAILABLE_DEFAULT;
  bool can_use_pin_ = false;
  base::OneShotTimer reset_state_;
};

AuthDialogContentsView::AuthDialogContentsView(uint32_t auth_methods)
    : auth_methods_(auth_methods) {
  DCHECK(auth_methods_ & kAuthPassword);

  SetLayoutManager(std::make_unique<views::FillLayout>());
  container_ = AddChildView(std::make_unique<NonAccessibleView>());
  container_->SetBackground(views::CreateSolidBackground(SK_ColorWHITE));
  container_->SetBorder(views::CreateEmptyBorder(
      kBorderTopDp, kBorderLeftDp, kBorderBottomDp, kBorderRightDp));

  main_layout_ =
      container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  main_layout_->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);
  main_layout_->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  AddTitleView();
  AddVerticalSpacing(kSpacingAfterTitle);
  AddPasswordView();
  AddPinView();

  if (auth_methods_ & kAuthFingerprint) {
    fingerprint_view_ =
        container_->AddChildView(std::make_unique<FingerprintView>());
    fingerprint_view_->SetCanUsePin(auth_methods_ & kAuthPin);
  }

  AddVerticalSpacing(kSpacingBeforeButtons);
  AddActionButtonsView();

  // Deferred because it needs the pin_view_ pointer.
  InitPasswordView();
}

AuthDialogContentsView::~AuthDialogContentsView() = default;

void AuthDialogContentsView::AddedToWidget() {
  if (auth_methods_ & kAuthFingerprint) {
    // Inject a callback from the contents view so that we can show retry
    // prompt.
    InSessionAuthDialogController::Get()->AuthenticateUserWithFingerprint(
        base::BindOnce(&AuthDialogContentsView::OnFingerprintAuthComplete,
                       weak_factory_.GetWeakPtr()));
  }
}

void AuthDialogContentsView::AddTitleView() {
  title_ = container_->AddChildView(std::make_unique<views::Label>());
  title_->SetEnabledColor(SK_ColorBLACK);
  title_->SetSubpixelRenderingEnabled(false);
  title_->SetAutoColorReadabilityEnabled(false);
  title_->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);

  const gfx::FontList& base_font_list = views::Label::GetDefaultFontList();

  title_->SetFontList(base_font_list.Derive(kTitleFontSizeDeltaDp,
                                            gfx::Font::FontStyle::NORMAL,
                                            gfx::Font::Weight::NORMAL));
  title_->SetText(base::UTF8ToUTF16(kTitle));
  title_->SetMaximumWidth(kContainerPreferredWidth);
  title_->SetElideBehavior(gfx::ElideBehavior::ELIDE_TAIL);

  title_->SetPreferredSize(
      gfx::Size(kContainerPreferredWidth, title_->height()));
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
}

void AuthDialogContentsView::AddPasswordView() {
  password_view_ = container_->AddChildView(
      std::make_unique<LoginPasswordView>(CreateInSessionAuthPalette()));

  password_view_->SetPaintToLayer();
  password_view_->layer()->SetFillsBoundsOpaquely(false);
  password_view_->SetDisplayPasswordButtonVisible(true);
  password_view_->SetEnabled(true);
  password_view_->SetEnabledOnEmptyPassword(false);
  password_view_->SetFocusEnabledForChildViews(true);
  password_view_->SetVisible(true);

  password_view_->SetPlaceholderText(
      (auth_methods_ & kAuthPin)
          ? l10n_util::GetStringUTF16(
                IDS_ASH_LOGIN_POD_PASSWORD_PIN_PLACEHOLDER)
          : l10n_util::GetStringUTF16(IDS_ASH_LOGIN_POD_PASSWORD_PLACEHOLDER));
}

void AuthDialogContentsView::AddPinView() {
  pin_view_ = container_->AddChildView(std::make_unique<LoginPinView>(
      LoginPinView::Style::kAlphanumeric, CreateInSessionAuthPalette(),
      base::BindRepeating(&LoginPasswordView::InsertNumber,
                          base::Unretained(password_view_)),
      base::BindRepeating(&LoginPasswordView::Backspace,
                          base::Unretained(password_view_)),
      base::BindRepeating(&LoginPasswordView::SubmitPassword,
                          base::Unretained(password_view_))));
  pin_view_->SetVisible(auth_methods_ & kAuthPin);
}

void AuthDialogContentsView::InitPasswordView() {
  password_view_->Init(
      base::BindRepeating(&AuthDialogContentsView::OnAuthSubmit,
                          base::Unretained(this)),
      base::BindRepeating(&LoginPinView::OnPasswordTextChanged,
                          base::Unretained(pin_view_)),
      base::DoNothing(), base::DoNothing());
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
      views::BoxLayout::MainAxisAlignment::kEnd);

  cancel_button_ =
      AddButton(kCancelButtonText, static_cast<int>(ButtonId::kCancel),
                action_view_container_);

  action_view_container_->SetPreferredSize(
      gfx::Size(kContainerPreferredWidth, cancel_button_->height()));
}

void AuthDialogContentsView::ButtonPressed(views::Button* sender,
                                           const ui::Event& event) {
  if (sender == cancel_button_) {
    // Cancel() deletes |this|.
    InSessionAuthDialogController::Get()->Cancel();
  }
}

views::LabelButton* AuthDialogContentsView::AddButton(const std::string& text,
                                                      int id,
                                                      views::View* container) {
  // Creates a button with |text|.
  auto button =
      std::make_unique<views::MdTextButton>(this, base::ASCIIToUTF16(text));
  button->SetID(id);

  views::LabelButton* view = button.get();
  container->AddChildView(
      login_views_utils::WrapViewForPreferredSize(std::move(button)));
  return view;
}

void AuthDialogContentsView::OnAuthSubmit(const base::string16& password) {
  InSessionAuthDialogController::Get()->AuthenticateUserWithPasswordOrPin(
      base::UTF16ToUTF8(password),
      base::BindOnce(&AuthDialogContentsView::OnPasswordOrPinAuthComplete,
                     weak_factory_.GetWeakPtr()));
}

// TODO(b/156258540): Clear password/PIN if auth failed and retry is allowed.
void AuthDialogContentsView::OnPasswordOrPinAuthComplete(
    base::Optional<bool> success) {}

void AuthDialogContentsView::OnFingerprintAuthComplete(
    bool success,
    FingerprintState fingerprint_state) {
  fingerprint_view_->SetState(fingerprint_state);
  // Prepare for the next fingerprint scan.
  if (!success && fingerprint_state == FingerprintState::AVAILABLE_DEFAULT) {
    InSessionAuthDialogController::Get()->AuthenticateUserWithFingerprint(
        base::BindOnce(&AuthDialogContentsView::OnFingerprintAuthComplete,
                       weak_factory_.GetWeakPtr()));
  }
  fingerprint_view_->NotifyFingerprintAuthResult(success);
}

void AuthDialogContentsView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  views::View::GetAccessibleNodeData(node_data);
  node_data->role = ax::mojom::Role::kDialog;
  node_data->SetName(base::UTF8ToUTF16(kTitle));
}

}  // namespace ash
