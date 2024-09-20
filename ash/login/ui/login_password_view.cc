// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_password_view.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/hover_notifier.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_arrow_navigation_delegate.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/color_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

// TODO(jdufault): On two user view the password prompt is visible to
// accessibility using special navigation keys even though it is invisible. We
// probably need to customize the text box quite a bit, we may want to do
// something similar to SearchBoxView.

namespace ash {
namespace {

// External padding on the submit button, used for the focus ring.
constexpr const int kBorderForFocusRingDp = 3;

// Spacing between the icons (caps lock, display password) and the
// borders of the password row. Note that if there is no icon, the padding will
// appear to be 8dp since the password textfield has a 2dp margin.
constexpr const int kInternalHorizontalPaddingPasswordRowDp = 6;

// Spacing between the password row and the submit button.
constexpr int kSpacingBetweenPasswordRowAndSubmitButtonDp =
    8 - kBorderForFocusRingDp;

// Size (width/height) of the submit button.
constexpr int kSubmitButtonContentSizeDp = 32;

// Size (width/height) of the submit button, border included.
constexpr int kSubmitButtonSizeDp =
    kSubmitButtonContentSizeDp + 2 * kBorderForFocusRingDp;

// Left padding of the password view allowing the view to have its center
// aligned with the one of the user pod.
constexpr int kLeftPaddingPasswordView =
    kSubmitButtonSizeDp + kSpacingBetweenPasswordRowAndSubmitButtonDp;

// Width of the password row, placed at the center of the password view
// (which also contains the submit button).
constexpr int kPasswordRowWidthDp = 204 + kBorderForFocusRingDp;

// Total width of the password view (left margin + password row + spacing +
// submit button).
constexpr int kPasswordTotalWidthDp =
    kLeftPaddingPasswordView + kPasswordRowWidthDp + kSubmitButtonSizeDp +
    kSpacingBetweenPasswordRowAndSubmitButtonDp;

// Delta between normal font and font of the typed text.
constexpr int kPasswordVisibleFontDeltaSize = 1;

// Delta between normal font and font of glyphs.
constexpr int kPasswordHiddenFontDeltaSize = 12;

// Line-height of the password hidden font, used to limit the height of the
// cursor.
constexpr int kPasswordHiddenLineHeight = 20;

// Spacing between glyphs.
constexpr int kPasswordGlyphSpacing = 6;

// Size (width/height) of the different icons belonging to the password row
// (the display password icon and the caps lock icon).
constexpr const int kIconSizeDp = 20;

// Horizontal spacing between the end of the password textfield and the display
// password button.  Note that the password textfield has a 2dp margin so the
// ending result will be 8dp.
constexpr const int kPasswordRowHorizontalSpacingDp = 6;

// The password textfield has an external margin because we want these specific
// visual results following in these different cases:
// icon-textfield-icon: 6dp - icon - 8dp - textfield - 8dp - icon - 6dp
// textfield-icon:      8dp - textfield - 8dp - icon - 6dp
// icon-textfield:      6dp - icon - 8dp - textfield - 8dp
// textfield:           8dp - textfield - 8dp
// This translates by having a 6dp spacing between children of the paassword
// row, having a 6dp padding for the password row and having a 2dp margin for
// the password textfield.
constexpr const int kPasswordTextfieldMarginDp = 2;

constexpr const int kJellyPasswordRowCornerRadiusDp = 8;

// Delay after which the password gets cleared if nothing has been typed. It is
// only running if the display password button is shown, as there is no
// potential security threat otherwise.
constexpr base::TimeDelta kClearPasswordAfterDelay = base::Seconds(30);

// Delay after which the password gets back to hidden state, for security.
constexpr base::TimeDelta kHidePasswordAfterDelay = base::Seconds(5);

}  // namespace

// The login password row contains the password textfield and different buttons
// and indicators (display password, caps lock enabled).
class LoginPasswordView::LoginPasswordRow : public views::View {
  METADATA_HEADER(LoginPasswordRow, views::View)

 public:
  explicit LoginPasswordRow() {
    const int corner_radius = kJellyPasswordRowCornerRadiusDp;
    const ui::ColorId background_color =
        cros_tokens::kCrosSysSystemBaseElevated;

    SetBackground(views::CreateThemedRoundedRectBackground(background_color,
                                                           corner_radius));
  }

  ~LoginPasswordRow() override = default;
  LoginPasswordRow(const LoginPasswordRow&) = delete;
  LoginPasswordRow& operator=(const LoginPasswordRow&) = delete;
};

BEGIN_METADATA(LoginPasswordView, LoginPasswordRow)
END_METADATA

// A textfield that selects all text on focus and allows to switch between
// show/hide password modes.
class LoginPasswordView::LoginTextfield : public views::Textfield {
  METADATA_HEADER(LoginTextfield, views::Textfield)

 public:
  LoginTextfield(base::RepeatingClosure on_focus_closure,
                 base::RepeatingClosure on_blur_closure)
      : on_focus_closure_(std::move(on_focus_closure)),
        on_blur_closure_(std::move(on_blur_closure)) {
    SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
    UpdateFontListAndCursor();
    set_placeholder_font_list(font_list_visible_);
    SetObscuredGlyphSpacing(kPasswordGlyphSpacing);
    SetBorder(nullptr);
  }
  LoginTextfield(const LoginTextfield&) = delete;
  LoginTextfield& operator=(const LoginTextfield&) = delete;
  ~LoginTextfield() override = default;

  void OnThemeChanged() override {
    views::Textfield::OnThemeChanged();
    SetTextColor(GetColorProvider()->GetColor(kColorAshTextColorPrimary));
    SetBackgroundColor(SK_ColorTRANSPARENT);
    set_placeholder_text_color(
        GetColorProvider()->GetColor(kColorAshTextColorSecondary));
  }

  // views::Textfield:
  void OnBlur() override {
    if (on_blur_closure_) {
      on_blur_closure_.Run();
    }
    views::Textfield::OnBlur();
  }

  // views::Textfield:
  void OnFocus() override {
    if (on_focus_closure_) {
      on_focus_closure_.Run();
    }
    views::Textfield::OnFocus();
  }

  void AboutToRequestFocusFromTabTraversal(bool reverse) override {
    if (!GetText().empty()) {
      SelectAll(/*reversed=*/false);
    }
  }

  void UpdateFontListAndCursor() {
    SetFontList(GetTextInputType() == ui::TEXT_INPUT_TYPE_PASSWORD
                    ? font_list_hidden_
                    : font_list_visible_);
  }

  // This is useful when the display password button is not shown. In such a
  // case, the login text field needs to define its size.
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(kPasswordTotalWidthDp, kIconSizeDp);
  }

 private:
  const gfx::FontList font_list_visible_ =
      views::Textfield::GetDefaultFontList().Derive(
          kPasswordVisibleFontDeltaSize,
          gfx::Font::FontStyle::NORMAL,
          gfx::Font::Weight::NORMAL);
  // As the dots – displayed when the password is hidden – use a much bigger
  // font size, the cursor would be too big without limiting the line-height.
  // This way, when we switch from hidden to visible password, the cursor is
  // visually consistent.
  const gfx::FontList font_list_hidden_ =
      views::Textfield::GetDefaultFontList()
          .Derive(kPasswordHiddenFontDeltaSize,
                  gfx::Font::FontStyle::NORMAL,
                  gfx::Font::Weight::NORMAL)
          .DeriveWithHeightUpperBound(kPasswordHiddenLineHeight);

  // Closures that will be called when the element receives and loses focus.
  base::RepeatingClosure on_focus_closure_;
  base::RepeatingClosure on_blur_closure_;
  base::RepeatingClosure on_tab_focus_closure_;
};

BEGIN_METADATA(LoginPasswordView, LoginTextfield)
END_METADATA

class LoginPasswordView::DisplayPasswordButton
    : public views::ToggleImageButton {
  METADATA_HEADER(DisplayPasswordButton, views::ToggleImageButton)

 public:
  explicit DisplayPasswordButton(views::Button::PressedCallback callback)
      : ToggleImageButton(std::move(callback)) {
    SetTooltipText(l10n_util::GetStringUTF16(
        IDS_ASH_LOGIN_DISPLAY_PASSWORD_BUTTON_ACCESSIBLE_NAME_SHOW));
    SetToggledTooltipText(l10n_util::GetStringUTF16(
        IDS_ASH_LOGIN_DISPLAY_PASSWORD_BUTTON_ACCESSIBLE_NAME_HIDE));
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetInstallFocusRingOnFocus(true);
    views::FocusRing::Get(this)->SetColorId(ui::kColorAshFocusRing);

    const ui::ColorId enabled_icon_color_id = cros_tokens::kCrosSysOnSurface;
    const ui::ColorId disabled_icon_color_id = cros_tokens::kCrosSysDisabled;

    const ui::ImageModel invisible_icon = ui::ImageModel::FromVectorIcon(
        kLockScreenPasswordInvisibleIcon, enabled_icon_color_id, kIconSizeDp);
    const ui::ImageModel visible_icon = ui::ImageModel::FromVectorIcon(
        kLockScreenPasswordVisibleIcon, enabled_icon_color_id, kIconSizeDp);
    const ui::ImageModel visible_icon_disabled = ui::ImageModel::FromVectorIcon(
        kLockScreenPasswordVisibleIcon, disabled_icon_color_id, kIconSizeDp);
    SetImageModel(views::Button::STATE_NORMAL, visible_icon);
    SetImageModel(views::Button::STATE_DISABLED, visible_icon_disabled);
    SetToggledImageModel(views::Button::STATE_NORMAL, invisible_icon);

    SetEnabled(false);
  }

  DisplayPasswordButton(const DisplayPasswordButton&) = delete;
  DisplayPasswordButton& operator=(const DisplayPasswordButton&) = delete;
  ~DisplayPasswordButton() override = default;
};

BEGIN_METADATA(LoginPasswordView, DisplayPasswordButton)
END_METADATA

LoginPasswordView::TestApi::TestApi(LoginPasswordView* view) : view_(view) {}

LoginPasswordView::TestApi::~TestApi() = default;

void LoginPasswordView::TestApi::SubmitPassword(const std::string& password) {
  view_->textfield_->SetText(base::ASCIIToUTF16(password));
  view_->UpdateUiState();
  view_->SubmitPassword();
}

views::Textfield* LoginPasswordView::TestApi::textfield() const {
  return view_->textfield_;
}

views::View* LoginPasswordView::TestApi::submit_button() const {
  return view_->submit_button_;
}

views::ToggleImageButton* LoginPasswordView::TestApi::display_password_button()
    const {
  return view_->display_password_button_;
}

LoginPasswordView::LoginPasswordView()
    : clear_password_timer_(FROM_HERE,
                            kClearPasswordAfterDelay,
                            base::BindRepeating(&LoginPasswordView::Reset,
                                                base::Unretained(this))),
      hide_password_timer_(FROM_HERE,
                           kHidePasswordAfterDelay,
                           base::BindRepeating(&LoginPasswordView::HidePassword,
                                               base::Unretained(this),
                                               /*chromevox_exception=*/true)) {
  Shell::Get()->ime_controller()->AddObserver(this);

  // Contains the password layout on the left and the submit button on the
  // right.
  auto* root_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::TLBR(0, kLeftPaddingPasswordView, 0, 0),
      kSpacingBetweenPasswordRowAndSubmitButtonDp));
  root_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);

  auto* password_row_container =
      AddChildView(std::make_unique<NonAccessibleView>());
  // The password row should have the same visible height than the submit
  // button. Since the login password view has the same height than the submit
  // button – border included – we need to remove its border.
  auto* password_row_container_layout =
      password_row_container->SetLayoutManager(
          std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kVertical,
              gfx::Insets::VH(kBorderForFocusRingDp, 0)));
  password_row_container_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);

  password_row_ = password_row_container->AddChildView(
      std::make_unique<LoginPasswordRow>());

  if (chromeos::features::IsJellyrollEnabled()) {
    password_row_->SetBorder(std::make_unique<views::HighlightBorder>(
        kJellyPasswordRowCornerRadiusDp,
        views::HighlightBorder::Type::kHighlightBorderOnShadow));

    password_row_->SetBackground(views::CreateThemedRoundedRectBackground(
        cros_tokens::kCrosSysSystemOnBase, kJellyPasswordRowCornerRadiusDp));
  }

  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::VH(0, kInternalHorizontalPaddingPasswordRowDp),
      kPasswordRowHorizontalSpacingDp);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  auto* layout_ptr = password_row_->SetLayoutManager(std::move(layout));

  // Make the password row fill the view.
  password_row_container_layout->SetFlexForView(password_row_, 1);

  capslock_icon_ =
      password_row_->AddChildView(std::make_unique<views::ImageView>());
  capslock_icon_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_CAPS_LOCK_ACCESSIBLE_NAME));
  capslock_icon_->SetVisible(false);

  auto* textfield_container =
      password_row_->AddChildView(std::make_unique<NonAccessibleView>());
  textfield_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::VH(0, kPasswordTextfieldMarginDp)));

  // Password textfield. We control the textfield size by sizing the parent
  // view, as the textfield will expand to fill it.
  textfield_ =
      textfield_container->AddChildView(std::make_unique<LoginTextfield>(
          // Highlight on focus. Remove highlight on blur.
          base::BindRepeating(&LoginPasswordView::SetCapsLockHighlighted,
                              base::Unretained(this), /*highlight=*/true),
          base::BindRepeating(&LoginPasswordView::SetCapsLockHighlighted,
                              base::Unretained(this), /*highlight=*/false)));
  textfield_->set_controller(this);

  layout_ptr->SetFlexForView(textfield_container, 1);

  display_password_button_ = password_row_->AddChildView(
      std::make_unique<DisplayPasswordButton>(base::BindRepeating(
          [](LoginPasswordView* view) {
            view->InvertPasswordDisplayingState();
          },
          this)));

  submit_button_ = AddChildView(std::make_unique<ArrowButtonView>(
      base::BindRepeating(&LoginPasswordView::SubmitPassword,
                          base::Unretained(this)),
      kSubmitButtonContentSizeDp));
  submit_button_->SetBackgroundColorId(kColorAshControlBackgroundColorInactive);
  submit_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_SUBMIT_BUTTON_ACCESSIBLE_NAME));
  submit_button_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_SUBMIT_BUTTON_ACCESSIBLE_NAME));
  submit_button_->SetEnabled(false);

  // Make sure the textfield always starts with focus.
  RequestFocus();

  // Initialize with the initial state of caps lock.
  OnCapsLockChanged(Shell::Get()->ime_controller()->IsCapsLockEnabled());

  // Make sure the UI start with the correct states.
  UpdateUiState();
}

LoginPasswordView::~LoginPasswordView() {
  Shell::Get()->ime_controller()->RemoveObserver(this);
}

void LoginPasswordView::Init(
    const OnPasswordSubmit& on_submit,
    const OnPasswordTextChanged& on_password_text_changed) {
  DCHECK(on_submit);
  DCHECK(on_password_text_changed);
  on_submit_ = on_submit;
  on_password_text_changed_ = on_password_text_changed;
}

void LoginPasswordView::SetFocusEnabledForTextfield(bool enable) {
  auto behavior = enable ? FocusBehavior::ALWAYS : FocusBehavior::NEVER;
  textfield_->SetFocusBehavior(behavior);
}

void LoginPasswordView::SetDisplayPasswordButtonVisible(bool visible) {
  display_password_button_->SetVisible(visible);
  // Only start the timer if the display password button is enabled.
  if (visible) {
    clear_password_timer_.Reset();
  }
}

void LoginPasswordView::Reset() {
  HidePassword(/*chromevox_exception=*/false);
  textfield_->SetText(std::u16string());
  textfield_->ClearEditHistory();
  // |ContentsChanged| won't be called by |Textfield| if the text is changed
  // by |Textfield::SetText()|.
  ContentsChanged(textfield_, textfield_->GetText());
}

void LoginPasswordView::InsertNumber(int value) {
  if (textfield_->GetReadOnly()) {
    return;
  }

  if (!textfield_->HasFocus()) {
    // RequestFocus on textfield to activate cursor.
    RequestFocus();
  }
  textfield_->InsertOrReplaceText(base::NumberToString16(value));
}

void LoginPasswordView::Backspace() {
  // Instead of just adjusting textfield_ text directly, fire a backspace key
  // event as this handles the various edge cases (ie, selected text).

  // views::Textfield::OnKeyPressed is private, so we call it via views::View.
  auto* view = static_cast<views::View*>(textfield_);
  view->OnKeyPressed(ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_BACK,
                                  ui::DomCode::BACKSPACE, ui::EF_NONE));
  view->OnKeyPressed(ui::KeyEvent(ui::EventType::kKeyReleased, ui::VKEY_BACK,
                                  ui::DomCode::BACKSPACE, ui::EF_NONE));
}

void LoginPasswordView::SetPlaceholderText(
    const std::u16string& placeholder_text) {
  textfield_->SetPlaceholderText(placeholder_text);
}

void LoginPasswordView::SetReadOnly(bool read_only) {
  if (!read_only &&
      Shell::Get()->login_screen_controller()->IsAuthenticating()) {
    // TODO(b/276246832): We shouldn't enable the LoginPasswordView during
    // Authentication.
    LOG(WARNING) << "LoginPasswordView::SetReadOnly called with false during "
                    "Authentication.";
  }
  textfield_->SetReadOnly(read_only);
  textfield_->SetCursorEnabled(!read_only);
  UpdateUiState();
}

bool LoginPasswordView::IsReadOnly() const {
  return textfield_->GetReadOnly();
}

gfx::Size LoginPasswordView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size size = views::View::CalculatePreferredSize(
      views::SizeBounds(kPasswordTotalWidthDp, {}));
  size.set_width(kPasswordTotalWidthDp);
  return size;
}

void LoginPasswordView::RequestFocus() {
  textfield_->RequestFocus();
}

bool LoginPasswordView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::KeyboardCode::VKEY_RETURN &&
      IsPasswordSubmittable()) {
    SubmitPassword();
    return true;
  }

  return false;
}

void LoginPasswordView::InvertPasswordDisplayingState() {
  if (textfield_->GetTextInputType() == ui::TEXT_INPUT_TYPE_PASSWORD) {
    textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_NULL);
    display_password_button_->SetToggled(true);
    textfield_->UpdateFontListAndCursor();
    hide_password_timer_.Reset();
  } else {
    HidePassword(/*chromevox_exception=*/false);
  }
}

void LoginPasswordView::HidePassword(bool chromevox_exception) {
  if (chromevox_exception &&
      Shell::Get()->accessibility_controller()->spoken_feedback().enabled()) {
    return;
  }
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  display_password_button_->SetToggled(false);
  textfield_->UpdateFontListAndCursor();
}

void LoginPasswordView::ContentsChanged(views::Textfield* sender,
                                        const std::u16string& new_contents) {
  DCHECK_EQ(sender, textfield_);
  UpdateUiState();
  textfield_->UpdateFontListAndCursor();
  on_password_text_changed_.Run(new_contents.empty() /*is_empty*/);

  // If the password is currently revealed.
  if (textfield_->GetTextInputType() == ui::TEXT_INPUT_TYPE_NULL) {
    hide_password_timer_.Reset();
  }

  // The display password button could be hidden by user policy.
  if (display_password_button_->GetVisible()) {
    clear_password_timer_.Reset();
  }
}

// Implements swapping active user with arrow keys
bool LoginPasswordView::HandleKeyEvent(views::Textfield* sender,
                                       const ui::KeyEvent& key_event) {
  // Treat the password field as normal if it has text
  if (!textfield_->GetText().empty()) {
    return false;
  }

  if (key_event.type() != ui::EventType::kKeyPressed) {
    return false;
  }

  if (key_event.is_repeat()) {
    return false;
  }

  if (arrow_navigation_delegate_) {
    return arrow_navigation_delegate_->HandleKeyEvent(key_event);
  }

  return false;
}

void LoginPasswordView::UpdateUiState() {
  bool enable_buttons = IsPasswordSubmittable();
  // Disabling the submit button will make it lose focus. The previous focusable
  // view will be the password textfield, which is more expected than the user
  // drop down button.
  if (!enable_buttons && submit_button_->HasFocus()) {
    RequestFocus();
  }
  submit_button_->SetEnabled(enable_buttons);
  display_password_button_->SetEnabled(enable_buttons);
}

void LoginPasswordView::OnCapsLockChanged(bool enabled) {
  capslock_icon_->SetVisible(enabled);
}

void LoginPasswordView::OnImplicitAnimationsCompleted() {
  Reset();
  SetVisible(false);
  StopObservingImplicitAnimations();
}

bool LoginPasswordView::IsPasswordSubmittable() {
  return !textfield_->GetReadOnly() && !textfield_->GetText().empty();
}

void LoginPasswordView::SubmitPassword() {
  DCHECK(IsPasswordSubmittable());
  if (textfield_->GetReadOnly()) {
    return;
  }
  SetReadOnly(true);
  on_submit_.Run(textfield_->GetText());
}

void LoginPasswordView::SetCapsLockHighlighted(bool highlight) {
  const gfx::VectorIcon& capslock_icon =
      Shell::Get()->keyboard_capability()->IsModifierSplitEnabled()
          ? kModifierSplitLockScreenCapsLockIcon
          : kLockScreenCapsLockIcon;
  const ui::ColorId enabled_icon_color_id = cros_tokens::kCrosSysOnSurface;
  const ui::ColorId disabled_icon_color_id = cros_tokens::kCrosSysDisabled;
  capslock_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      capslock_icon,
      highlight ? enabled_icon_color_id : disabled_icon_color_id));
}

void LoginPasswordView::SetLoginArrowNavigationDelegate(
    LoginArrowNavigationDelegate* delegate) {
  arrow_navigation_delegate_ = delegate;
}

void LoginPasswordView::SetAccessibleNameOnTextfield(
    const std::u16string& new_name) {
  textfield_->GetViewAccessibility().SetName(new_name);
}

BEGIN_METADATA(LoginPasswordView)
END_METADATA

}  // namespace ash
