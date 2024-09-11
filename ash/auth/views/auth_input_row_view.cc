// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/auth/views/auth_input_row_view.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/auth/views/auth_textfield.h"
#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/hover_notifier.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/public/cpp/login_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/color_util.h"
#include "ash/style/icon_button.h"
#include "ash/style/style_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace {

// Spacing between the icons (caps lock, display password) and the
// borders of the input row. Note that if there is no icon, the padding will
// appear to be 8dp since the input textfield has a 2dp margin.
constexpr const int kIntenalHorizontalPaddingInputRowDp = 6;

// Spacing between the input row and the submit button.
constexpr int kSpacingBetweenInputRowAndSubmitButtonDp = 5;

// Size (width/height) of the submit button.
constexpr int kSubmitButtonContentSizeDp = 32;

// Size (width/height) of the submit button, border included.
constexpr int kSubmitButtonSizeDp = kSubmitButtonContentSizeDp + 6;

// Left padding of the input row  view allowing the view to have its center
// aligned with the one of the user pod.
constexpr int kLeftPaddingInputRowView =
    kSubmitButtonSizeDp + kSpacingBetweenInputRowAndSubmitButtonDp;

// Width of the input row, placed at the center of the input row view
// (which also contains the submit button).
constexpr int kInputRowWidthDp = 204;

// Total width of the input row view (left margin + input row + spacing +
// submit button).
constexpr int kInputRowTotalWidthDp = kLeftPaddingInputRowView +
                                      kInputRowWidthDp + kSubmitButtonSizeDp +
                                      kSpacingBetweenInputRowAndSubmitButtonDp;

// Height of the input row view
constexpr int kInputRowHeightDp = 32;

// Size (width/height) of the different icons belonging to the input row
// (the display text icon and the caps lock icon).
constexpr const int kIconSizeDp = 20;

// The input textfield has an external margin because we want these specific
// visual results following in these different cases:
// icon-textfield-icon: 6dp - icon - 8dp - textfield - 8dp - icon - 6dp
// textfield-icon:      8dp - textfield - 8dp - icon - 6dp
// icon-textfield:      6dp - icon - 8dp - textfield - 8dp
// textfield:           8dp - textfield - 8dp
// This translates by having a 6dp spacing between children of the input
// row, having a 6dp padding for the input row and having a 2dp margin for
// the input textfield.
constexpr const int kInputTextfieldMarginDp = 2;

// The corner radius of the input row.
constexpr const int kInputRowCornerRadiusDp = 8;

// The inset of the input row and it's focus ring.
constexpr const int kInputRowFocusRingInsetDp = 2;

// The focus ring corner radius.
constexpr const int kInputRowFocusRingRadiusDp =
    kInputRowCornerRadiusDp + kInputRowFocusRingInsetDp;

// Horizontal spacing between the end of the input textfield and the display
// text button. Note that the input textfield has a 2dp margin so the
// ending result will be 8dp.
constexpr const int kHorizontalSpacingBetweenIconsAndTextfieldDp = 6;

const ui::ImageModel kCapslockIconHighlighted =
    ui::ImageModel::FromVectorIcon(kLockScreenCapsLockIcon,
                                   cros_tokens::kCrosSysOnSurface);

const ui::ImageModel kCapslockIconBlurred =
    ui::ImageModel::FromVectorIcon(kLockScreenCapsLockIcon,
                                   cros_tokens::kCrosSysDisabled);

}  // namespace

AuthInputRowView::TestApi::TestApi(AuthInputRowView* view) : view_(view) {}

AuthInputRowView::TestApi::~TestApi() = default;

raw_ptr<AuthTextfield> AuthInputRowView::TestApi::GetTextfield() const {
  return view_->textfield_;
}

raw_ptr<views::ToggleImageButton>
AuthInputRowView::TestApi::GetDisplayTextButton() const {
  return view_->display_text_button_;
}

raw_ptr<IconButton> AuthInputRowView::TestApi::GetSubmitButton() const {
  return view_->submit_button_;
}

raw_ptr<views::ImageView> AuthInputRowView::TestApi::GetCapsLockIcon() const {
  return view_->capslock_icon_;
}

raw_ptr<AuthInputRowView> AuthInputRowView::TestApi::GetView() const {
  return view_;
}

AuthInputRowView::AuthInputRowView(AuthType auth_type) : auth_type_(auth_type) {
  input_methods_observer_.Observe(Shell::Get()->ime_controller());

  ConfigureRootLayout();
  CreateAndConfigureInputRow();
  CreateAndConfigureCapslockIcon();
  CreateAndConfigureTextfieldContainer();
  CreateFocusRingForInputRow();
  CreateAndConfigureDisplayTextButton();
  CreateAndConfigureSubmitButton();
  SetDisplayTextButtonVisible(true);
}

AuthInputRowView::~AuthInputRowView() = default;

void AuthInputRowView::ConfigureRootLayout() {
  // Contains the input row layout on the left and the submit button on the
  // right.
  SetLayoutManager(std::make_unique<views::BoxLayout>(
                       views::BoxLayout::Orientation::kHorizontal,
                       gfx::Insets::TLBR(0, kLeftPaddingInputRowView, 0, 0),
                       kSpacingBetweenInputRowAndSubmitButtonDp))
      ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);
}

void AuthInputRowView::CreateAndConfigureInputRow() {
  auto* input_row_container =
      AddChildView(std::make_unique<NonAccessibleView>());
  // The input row should have the same visible height than the submit
  // button. Since the login password view has the same height than the submit
  // button – border included – we need to remove its border.
  auto* input_row_container_layout =
      input_row_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  input_row_container_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);

  input_row_ =
      input_row_container->AddChildView(std::make_unique<views::View>());
  input_row_->SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemOnBase, kInputRowCornerRadiusDp));

  input_row_->SetBorder(std::make_unique<views::HighlightBorder>(
      kInputRowCornerRadiusDp,
      views::HighlightBorder::Type::kHighlightBorderNoShadow));

  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::VH(0, kIntenalHorizontalPaddingInputRowDp),
      kHorizontalSpacingBetweenIconsAndTextfieldDp);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  input_row_layout_ = input_row_->SetLayoutManager(std::move(layout));

  // Make the input row fill the view.
  input_row_container_layout->SetFlexForView(input_row_, 1);
}

void AuthInputRowView::CreateAndConfigureCapslockIcon() {
  capslock_icon_ =
      input_row_->AddChildView(std::make_unique<views::ImageView>());
  capslock_icon_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_CAPS_LOCK_ACCESSIBLE_NAME));
  capslock_icon_->SetVisible(false);
}

void AuthInputRowView::CreateAndConfigureTextfieldContainer() {
  auto* textfield_container =
      input_row_->AddChildView(std::make_unique<NonAccessibleView>());
  textfield_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::VH(0, kInputTextfieldMarginDp)));

  // Input textfield. We control the textfield size by sizing the parent
  // view, as the textfield will expand to fill it.
  textfield_ =
      textfield_container->AddChildView(std::make_unique<AuthTextfield>(
          auth_type_ == AuthType::kPassword ? AuthTextfield::AuthType::kPassword
                                            : AuthTextfield::AuthType::kPin));
  textfield_->AddObserver(this);

  input_row_layout_->SetFlexForView(textfield_container, 1);
}

void AuthInputRowView::CreateFocusRingForInputRow() {
  CHECK_NE(textfield_, nullptr);

  StyleUtil::SetUpFocusRingForView(input_row_);
  views::FocusRing::Get(input_row_)
      ->SetPathGenerator(
          std::make_unique<views::RoundRectHighlightPathGenerator>(
              -gfx::Insets::VH(kInputRowFocusRingInsetDp,
                               kInputRowFocusRingInsetDp),
              kInputRowFocusRingRadiusDp));
  views::FocusRing::Get(input_row_)
      ->SetHasFocusPredicate(base::BindRepeating(
          [](const AuthTextfield* textfield, const views::View* view) {
            return textfield->IsActive();
          },
          textfield_));
}

void AuthInputRowView::CreateAndConfigureSubmitButton() {
  IconButton::Builder builder;
  builder.SetType(IconButton::Type::kMedium)
      .SetAccessibleNameId(IDS_ASH_LOGIN_SUBMIT_BUTTON_ACCESSIBLE_NAME)
      .SetCallback(base::BindRepeating(&AuthInputRowView::OnSubmit,
                                       weak_ptr_factory_.GetWeakPtr()))
      .SetTogglable(true)
      .SetEnabled(false)
      .SetBorder(true)
      .SetBackgroundColor(cros_tokens::kCrosSysDisabledContainer)
      .SetVectorIcon(&kLockScreenArrowIcon);

  submit_button_ = AddChildView(builder.Build());

  submit_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_SUBMIT_BUTTON_ACCESSIBLE_NAME));

  submit_button_->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
}

void AuthInputRowView::CreateAndConfigureDisplayTextButton() {
  display_text_button_ =
      input_row_->AddChildView(std::make_unique<views::ToggleImageButton>(
          base::BindRepeating(&AuthInputRowView::ToggleTextDisplayingState,
                              base::Unretained(this))));

  switch (auth_type_) {
    case AuthType::kPassword:
      display_text_button_->SetTooltipText(l10n_util::GetStringUTF16(
          IDS_ASH_LOGIN_DISPLAY_PASSWORD_BUTTON_ACCESSIBLE_NAME_SHOW));
      display_text_button_->SetToggledTooltipText(l10n_util::GetStringUTF16(
          IDS_ASH_LOGIN_DISPLAY_PASSWORD_BUTTON_ACCESSIBLE_NAME_HIDE));
      break;
    case AuthType::kPin:
      display_text_button_->SetTooltipText(l10n_util::GetStringUTF16(
          IDS_ASH_AUTH_DISPLAY_PIN_BUTTON_ACCESSIBLE_NAME_SHOW));
      display_text_button_->SetToggledTooltipText(l10n_util::GetStringUTF16(
          IDS_ASH_AUTH_DISPLAY_PIN_BUTTON_ACCESSIBLE_NAME_HIDE));
      break;
  }

  display_text_button_->SetFocusBehavior(FocusBehavior::ALWAYS);
  display_text_button_->SetInstallFocusRingOnFocus(true);
  views::FocusRing::Get(display_text_button_)
      ->SetColorId(ui::kColorAshFocusRing);

  const ui::ImageModel invisible_icon = ui::ImageModel::FromVectorIcon(
      kLockScreenPasswordInvisibleIcon, cros_tokens::kCrosSysOnSurface,
      kIconSizeDp);
  const ui::ImageModel visible_icon = ui::ImageModel::FromVectorIcon(
      kLockScreenPasswordVisibleIcon, cros_tokens::kCrosSysOnSurface,
      kIconSizeDp);
  const ui::ImageModel visible_icon_disabled = ui::ImageModel::FromVectorIcon(
      kLockScreenPasswordVisibleIcon, cros_tokens::kCrosSysDisabled,
      kIconSizeDp);
  display_text_button_->SetImageModel(views::Button::STATE_NORMAL,
                                      visible_icon);
  display_text_button_->SetImageModel(views::Button::STATE_DISABLED,
                                      visible_icon_disabled);
  display_text_button_->SetToggledImageModel(views::Button::STATE_NORMAL,
                                             invisible_icon);

  display_text_button_->SetEnabled(false);
}

void AuthInputRowView::OnTextfieldBlur() {
  views::FocusRing::Get(input_row_)->SchedulePaint();
  SetCapsLockHighlighted(false);
  for (auto& observer : observers_) {
    observer.OnTextfieldBlur();
  }
}

void AuthInputRowView::OnTextfieldFocus() {
  views::FocusRing::Get(input_row_)->SchedulePaint();
  SetCapsLockHighlighted(true);
  for (auto& observer : observers_) {
    observer.OnTextfieldFocus();
  }
}

void AuthInputRowView::OnContentsChanged(const std::u16string& new_contents) {
  bool enable_buttons = !textfield_->GetReadOnly() && !new_contents.empty();
  if (new_contents.empty() && textfield_->IsTextVisible()) {
    ToggleTextDisplayingState();
  }

  if (!enable_buttons) {
    // If the submit or eye icon had the focus we should pass the focus to the
    // textfield_.
    if (submit_button_->HasFocus() || display_text_button_->HasFocus()) {
      RequestFocus();
    }
  }
  submit_button_->SetEnabled(enable_buttons);
  display_text_button_->SetEnabled(enable_buttons);
  for (auto& observer : observers_) {
    observer.OnContentsChanged(new_contents);
  }
}

void AuthInputRowView::OnTextVisibleChanged(bool visible) {
  display_text_button_->SetToggled(visible);
  for (auto& observer : observers_) {
    observer.OnTextVisibleChanged(visible);
  }
}

void AuthInputRowView::OnSubmit() {
  Submit();
}

void AuthInputRowView::OnEscape() {
  Escape();
}

void AuthInputRowView::SetDisplayTextButtonVisible(bool visible) {
  display_text_button_->SetVisible(visible);
  // Only start the timer if the display password button is enabled.
  if (visible) {
    textfield_->ApplyTimerLogic();
  } else {
    textfield_->ResetTimerLogic();
  }
}

gfx::Size AuthInputRowView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size({kInputRowTotalWidthDp, kInputRowHeightDp});
}

void AuthInputRowView::RequestFocus() {
  textfield_->RequestFocus();
}

bool AuthInputRowView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::KeyboardCode::VKEY_ESCAPE) {
    Escape();
    return true;
  }

  return false;
}

void AuthInputRowView::ToggleTextDisplayingState() {
  textfield_->SetTextVisible(!textfield_->IsTextVisible());
}

void AuthInputRowView::OnCapsLockChanged(bool enabled) {
  capslock_icon_->SetVisible(enabled);
  for (auto& observer : observers_) {
    observer.OnCapsLockStateChanged(enabled);
  }
}

void AuthInputRowView::OnImplicitAnimationsCompleted() {
  textfield_->Reset();
  SetVisible(false);
  StopObservingImplicitAnimations();
}

bool AuthInputRowView::IsInputSubmittable() const {
  return !textfield_->GetReadOnly() && !textfield_->GetText().empty();
}

void AuthInputRowView::Submit() {
  DCHECK(IsInputSubmittable());
  for (auto& observer : observers_) {
    observer.OnSubmit(textfield_->GetText());
  }
}

void AuthInputRowView::Escape() {
  for (auto& observer : observers_) {
    observer.OnEscape();
  }
}

void AuthInputRowView::SetCapsLockHighlighted(bool highlight) {
  capslock_icon_->SetImage(highlight ? kCapslockIconHighlighted
                                     : kCapslockIconBlurred);
}

void AuthInputRowView::SetAccessibleNameOnTextfield(
    const std::u16string& new_name) {
  textfield_->SetAccessibleName(new_name);
}

void AuthInputRowView::SetInputEnabled(bool enabled) {
  SetEnabled(enabled);
  textfield_->SetEnabled(enabled);
  textfield_->SetBorder(nullptr);
  submit_button_->SetEnabled(enabled);
  display_text_button_->SetEnabled(enabled);
}

void AuthInputRowView::InsertDigit(int digit) {
  CHECK_EQ(auth_type_, AuthType::kPin);
  textfield_->InsertDigit(digit);
}

void AuthInputRowView::Backspace() {
  CHECK_EQ(auth_type_, AuthType::kPin);
  textfield_->Backspace();
}

void AuthInputRowView::ResetState() {
  // This automatically calls the OnContentsChanged function, and that will set
  // the buttons properly to the reset state.
  textfield_->Reset();
}

void AuthInputRowView::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AuthInputRowView::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

BEGIN_METADATA(AuthInputRowView)
END_METADATA

}  // namespace ash
