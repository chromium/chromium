// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_password_view.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/horizontal_image_sequence_animation_decoder.h"
#include "ash/login/ui/hover_notifier.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/public/cpp/login_constants.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

// TODO(jdufault): On two user view the password prompt is visible to
// accessibility using special navigation keys even though it is invisible. We
// probably need to customize the text box quite a bit, we may want to do
// something similar to SearchBoxView.

namespace ash {
namespace {

// How long the user must hover over the easy unlock icon before showing the
// tooltip.
const int kDelayBeforeShowingTooltipMs = 500;

// Margin above/below the password view.
constexpr const int kMarginAboveBelowPasswordIconsDp = 8;

// Spacing between the password textfield and the submit button.
constexpr int kSpacingBetweenPasswordTextFieldAndSubmitButtonDp = 8;

// Size (width/height) of the submit button.
constexpr int kSubmitButtonSizeDp = 32;

// Left padding of the password view allowing the view to have its center
// aligned with the one of the user pod.
constexpr int kLeftPaddingPasswordView =
    kSubmitButtonSizeDp + kSpacingBetweenPasswordTextFieldAndSubmitButtonDp;

// Width of the password textfield, placed at the center of the password view.
constexpr int kPasswordTextfieldWidthDp = 204;

// Total width of the password view (left margin + password textfield + spacing
// + submit button).
constexpr int kPasswordTotalWidthDp =
    kLeftPaddingPasswordView + kPasswordTextfieldWidthDp + kSubmitButtonSizeDp +
    kSpacingBetweenPasswordTextFieldAndSubmitButtonDp;

// Delta between normal font and font of the typed text.
constexpr int kPasswordFontDeltaSize = 5;

// Spacing between glyphs.
constexpr int kPasswordGlyphSpacing = 6;

// Size (width/height) of the display password button.
constexpr int kDisplayPasswordButtonSizeDp = 20;

// Size (width/height) of the caps lock hint icon.
constexpr int kCapsLockIconSizeDp = 20;

// Width and height of the easy unlock icon.
constexpr const int kEasyUnlockIconSizeDp = 20;

// Horizontal distance/margin between the easy unlock icon and the start of
// the password view.
constexpr const int kHorizontalDistanceBetweenEasyUnlockAndPasswordDp = 12;

// Non-empty height, useful for debugging/visualization.
constexpr const int kNonEmptyHeight = 1;

// Clears the password after some time if no action has been done and the
// display password feature is enabled, for security reasons.
constexpr base::TimeDelta kClearPasswordAfterDelay =
    base::TimeDelta::FromSeconds(30);

// Hides the password after a short delay for security reasons.
constexpr base::TimeDelta kHidePasswordAfterDelay =
    base::TimeDelta::FromSeconds(5);

constexpr const char kLoginPasswordViewName[] = "LoginPasswordView";

class NonAccessibleSeparator : public views::Separator {
 public:
  NonAccessibleSeparator() = default;
  ~NonAccessibleSeparator() override = default;

  // views::Separator:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    views::Separator::GetAccessibleNodeData(node_data);
    node_data->AddState(ax::mojom::State::kInvisible);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NonAccessibleSeparator);
};

// Set of resources for an easy unlock icon.
struct IconBundle {
  // Creates an IconBundle for a static image.
  IconBundle(int normal, int hover, int pressed)
      : normal(normal), hover(hover), pressed(pressed) {}
  // Creates an IconBundle instance for an animation.
  IconBundle(int resource, base::TimeDelta duration, int num_frames)
      : normal(resource),
        hover(resource),
        pressed(resource),
        duration(duration),
        num_frames(num_frames) {}

  // Icons for different button states.
  const int normal;
  const int hover;
  const int pressed;

  // Animation metadata. If these are set then |normal| == |hover| == |pressed|.
  const base::TimeDelta duration;
  const int num_frames = 0;
};

// Construct an IconBundle instance for a given EasyUnlockIconId value.
IconBundle GetEasyUnlockResources(EasyUnlockIconId id) {
  switch (id) {
    case EasyUnlockIconId::NONE:
      break;
    case EasyUnlockIconId::HARDLOCKED:
      return IconBundle(IDR_EASY_UNLOCK_HARDLOCKED,
                        IDR_EASY_UNLOCK_HARDLOCKED_HOVER,
                        IDR_EASY_UNLOCK_HARDLOCKED_PRESSED);
    case EasyUnlockIconId::LOCKED:
      return IconBundle(IDR_EASY_UNLOCK_LOCKED, IDR_EASY_UNLOCK_LOCKED_HOVER,
                        IDR_EASY_UNLOCK_LOCKED_PRESSED);
    case EasyUnlockIconId::LOCKED_TO_BE_ACTIVATED:
      return IconBundle(IDR_EASY_UNLOCK_LOCKED_TO_BE_ACTIVATED,
                        IDR_EASY_UNLOCK_LOCKED_TO_BE_ACTIVATED_HOVER,
                        IDR_EASY_UNLOCK_LOCKED_TO_BE_ACTIVATED_PRESSED);
    case EasyUnlockIconId::LOCKED_WITH_PROXIMITY_HINT:
      return IconBundle(IDR_EASY_UNLOCK_LOCKED_WITH_PROXIMITY_HINT,
                        IDR_EASY_UNLOCK_LOCKED_WITH_PROXIMITY_HINT_HOVER,
                        IDR_EASY_UNLOCK_LOCKED_WITH_PROXIMITY_HINT_PRESSED);
    case EasyUnlockIconId::UNLOCKED:
      return IconBundle(IDR_EASY_UNLOCK_UNLOCKED,
                        IDR_EASY_UNLOCK_UNLOCKED_HOVER,
                        IDR_EASY_UNLOCK_UNLOCKED_PRESSED);
    case EasyUnlockIconId::SPINNER:
      return IconBundle(IDR_EASY_UNLOCK_SPINNER,
                        base::TimeDelta::FromSeconds(2), /*num_frames=*/45);
  }

  NOTREACHED();
  return IconBundle(IDR_EASY_UNLOCK_LOCKED, IDR_EASY_UNLOCK_LOCKED_HOVER,
                    IDR_EASY_UNLOCK_LOCKED_PRESSED);
}

}  // namespace

// A textfield that selects all text on focus and allows to switch between
// show/hide password modes.
class LoginPasswordView::LoginTextfield : public views::Textfield {
 public:
  LoginTextfield(const LoginPalette& palette,
                 base::RepeatingClosure on_focus_closure,
                 base::RepeatingClosure on_blur_closure)
      : on_focus_closure_(std::move(on_focus_closure)),
        on_blur_closure_(std::move(on_blur_closure)) {
    SetTextColor(palette.password_text_color);
    SetFontList(views::Textfield::GetDefaultFontList().Derive(
        kPasswordFontDeltaSize, gfx::Font::FontStyle::NORMAL,
        gfx::Font::Weight::NORMAL));
    SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
    set_placeholder_font_list(views::Textfield::GetDefaultFontList());
    set_placeholder_text_color(palette.password_placeholder_text_color);
    SetObscuredGlyphSpacing(kPasswordGlyphSpacing);
    SetBorder(nullptr);
    SetBackgroundColor(palette.password_background_color);
  }
  LoginTextfield(const LoginTextfield&) = delete;
  LoginTextfield& operator=(const LoginTextfield&) = delete;
  ~LoginTextfield() override = default;

  // views::Textfield:
  void OnBlur() override {
    if (on_blur_closure_)
      on_blur_closure_.Run();
    views::Textfield::OnBlur();
  }

  void OnFocus() override {
    if (on_focus_closure_)
      on_focus_closure_.Run();
    views::Textfield::OnFocus();
  }

  void AboutToRequestFocusFromTabTraversal(bool reverse) override {
    SelectAll(/*reversed=*/false);
  }

  // Switches between normal input and password input when the user hits the
  // display password button.
  void InvertTextInputType() {
    if (GetTextInputType() == ui::TEXT_INPUT_TYPE_NULL)
      SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
    else
      SetTextInputType(ui::TEXT_INPUT_TYPE_NULL);
  }

  // This is useful when the display password button is not shown. In such a
  // case, the login text field needs to define its size.
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(kPasswordTotalWidthDp, kDisplayPasswordButtonSizeDp);
  }

 private:
  // Closures that will be called when the element receives and loses focus.
  base::RepeatingClosure on_focus_closure_;
  base::RepeatingClosure on_blur_closure_;
};

class LoginPasswordView::EasyUnlockIcon : public views::Button,
                                          public views::ButtonListener {
 public:
  EasyUnlockIcon(const gfx::Size& size, int corner_radius)
      : views::Button(this) {
    SetPreferredSize(size);
    SetLayoutManager(std::make_unique<views::FillLayout>());
    icon_ = AddChildView(
        std::make_unique<AnimatedRoundedImageView>(size, corner_radius));
  }
  ~EasyUnlockIcon() override = default;

  void Init(const OnEasyUnlockIconHovered& on_hovered,
            const OnEasyUnlockIconTapped& on_tapped) {
    DCHECK(on_hovered);
    DCHECK(on_tapped);

    on_hovered_ = on_hovered;
    on_tapped_ = on_tapped;

    hover_notifier_ = std::make_unique<HoverNotifier>(
        this, base::BindRepeating(
                  &LoginPasswordView::EasyUnlockIcon::OnHoverStateChanged,
                  base::Unretained(this)));
  }

  void SetEasyUnlockIcon(EasyUnlockIconId icon_id,
                         const base::string16& accessibility_label) {
    bool changed_states = icon_id != icon_id_;
    icon_id_ = icon_id;
    UpdateImage(changed_states);
    SetAccessibleName(accessibility_label);
  }

  void set_immediately_hover_for_test() { immediately_hover_for_test_ = true; }

  // views::Button:
  void StateChanged(ButtonState old_state) override {
    Button::StateChanged(old_state);

    // Stop showing tooltip, as we most likely exited hover state.
    invoke_hover_.Stop();

    if (GetState() == ButtonState::STATE_DISABLED)
      return;

    UpdateImage(false /*changed_states*/);

    if (GetState() == ButtonState::STATE_HOVERED) {
      if (immediately_hover_for_test_) {
        on_hovered_.Run();
      } else {
        invoke_hover_.Start(
            FROM_HERE,
            base::TimeDelta::FromMilliseconds(kDelayBeforeShowingTooltipMs),
            on_hovered_);
      }
    }
  }

  // views::ButtonListener:
  void ButtonPressed(Button* sender, const ui::Event& event) override {
    on_tapped_.Run();
  }

 private:
  void OnHoverStateChanged(bool has_hover) {
    SetState(has_hover ? ButtonState::STATE_HOVERED
                       : ButtonState::STATE_NORMAL);
  }

  void UpdateImage(bool changed_states) {
    // Ignore any calls happening while the view is not attached;
    // IsMouseHovered() will CHECK(false) in that scenario. GetWidget() may be
    // null during construction and GetWidget()->GetRootView() may be null
    // during destruction. Both scenarios only happen in tests.
    if (!GetWidget() || !GetWidget()->GetRootView())
      return;

    if (icon_id_ == EasyUnlockIconId::NONE)
      return;

    IconBundle resources = GetEasyUnlockResources(icon_id_);

    int active_resource = resources.normal;
    if (IsMouseHovered())
      active_resource = resources.hover;
    if (GetState() == ButtonState::STATE_PRESSED)
      active_resource = resources.pressed;

    // Image to show. It may or may not be an animation, depending on
    // |resources.duration|.
    gfx::ImageSkia* image =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            active_resource);

    if (!resources.duration.is_zero()) {
      // Only change the animation if the state itself has changed, otherwise
      // the active animation frame is reset and there is a lot of unecessary
      // decoding/image resizing work. This optimization only is valid only if
      // all three resource assets are the same.
      DCHECK_EQ(resources.normal, resources.hover);
      DCHECK_EQ(resources.normal, resources.pressed);
      if (changed_states) {
        icon_->SetAnimationDecoder(
            std::make_unique<HorizontalImageSequenceAnimationDecoder>(
                *image, resources.duration, resources.num_frames),
            AnimatedRoundedImageView::Playback::kRepeat);
      }
    } else {
      icon_->SetImage(*image);
    }
  }

  // Icon we are currently displaying.
  EasyUnlockIconId icon_id_ = EasyUnlockIconId::NONE;

  // View which renders the icon.
  AnimatedRoundedImageView* icon_;

  // Callbacks run when icon is hovered or tapped.
  OnEasyUnlockIconHovered on_hovered_;
  OnEasyUnlockIconTapped on_tapped_;

  std::unique_ptr<HoverNotifier> hover_notifier_;

  // Timer used to control when we invoke |on_hover_|.
  base::OneShotTimer invoke_hover_;

  // If true, the tooltip/hover timer will be skipped and |on_hover_| will be
  // run immediately.
  bool immediately_hover_for_test_ = false;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockIcon);
};

class LoginPasswordView::DisplayPasswordButton
    : public views::ToggleImageButton {
 public:
  DisplayPasswordButton(const LoginPalette& palette,
                        views::ButtonListener* listener)
      : ToggleImageButton(listener) {
    const gfx::ImageSkia invisible_icon = gfx::CreateVectorIcon(
        kLockScreenPasswordInvisibleIcon, kDisplayPasswordButtonSizeDp,
        palette.button_enabled_color);
    const gfx::ImageSkia visible_icon = gfx::CreateVectorIcon(
        kLockScreenPasswordVisibleIcon, kDisplayPasswordButtonSizeDp,
        palette.button_enabled_color);
    const gfx::ImageSkia visible_icon_disabled = gfx::CreateVectorIcon(
        kLockScreenPasswordVisibleIcon, kDisplayPasswordButtonSizeDp,
        SkColorSetA(palette.button_enabled_color,
                    login_constants::kButtonDisabledAlpha));
    SetImage(views::Button::STATE_NORMAL, visible_icon);
    SetImage(views::Button::STATE_DISABLED, visible_icon_disabled);
    SetToggledImage(views::Button::STATE_NORMAL, &invisible_icon);

    SetTooltipText(l10n_util::GetStringUTF16(
        IDS_ASH_LOGIN_DISPLAY_PASSWORD_BUTTON_ACCESSIBLE_NAME_SHOW));
    SetToggledTooltipText(l10n_util::GetStringUTF16(
        IDS_ASH_LOGIN_DISPLAY_PASSWORD_BUTTON_ACCESSIBLE_NAME_HIDE));
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetInstallFocusRingOnFocus(true);
    focus_ring()->SetColor(ShelfConfig::Get()->shelf_focus_border_color());

    SetEnabled(false);
  }

  DisplayPasswordButton(const DisplayPasswordButton&) = delete;
  DisplayPasswordButton& operator=(const DisplayPasswordButton&) = delete;
  ~DisplayPasswordButton() override = default;

  // This should be done automatically per ToggleImageButton.
  void InvertToggled() {
    toggled_ = !toggled_;
    SetToggled(toggled_);
  }

 private:
  bool toggled_ = false;
};

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

views::View* LoginPasswordView::TestApi::easy_unlock_icon() const {
  return view_->easy_unlock_icon_;
}

void LoginPasswordView::TestApi::set_immediately_hover_easy_unlock_icon() {
  view_->easy_unlock_icon_->set_immediately_hover_for_test();
}

void LoginPasswordView::TestApi::SetTimers(
    std::unique_ptr<base::RetainingOneShotTimer> clear_timer,
    std::unique_ptr<base::RetainingOneShotTimer> hide_timer) {
  view_->clear_password_timer_ = std::move(clear_timer);
  view_->hide_password_timer_ = std::move(hide_timer);
  // Starts the clearing timer.
  view_->SetDisplayPasswordButtonVisible(true);
}

LoginPasswordView::LoginPasswordView(const LoginPalette& palette)
    : is_display_password_feature_enabled_(
          chromeos::features::IsLoginDisplayPasswordButtonEnabled()),
      clear_password_timer_(std::make_unique<base::RetainingOneShotTimer>()),
      hide_password_timer_(std::make_unique<base::RetainingOneShotTimer>()),
      palette_(palette) {
  Shell::Get()->ime_controller()->AddObserver(this);

  // Contains the password layout on the left and the submit button on the
  // right.
  auto* root_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(0, kLeftPaddingPasswordView, 0, 0),
      kSpacingBetweenPasswordTextFieldAndSubmitButtonDp));
  root_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);

  // Contains the password row along with the separator.
  auto* password = AddChildView(std::make_unique<views::View>());
  std::unique_ptr<views::BoxLayout> password_layout =
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical);
  password_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  password->SetLayoutManager(std::move(password_layout));

  password_row_ = password->AddChildView(std::make_unique<NonAccessibleView>());
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(kMarginAboveBelowPasswordIconsDp, 0));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  auto* layout_ptr = password_row_->SetLayoutManager(std::move(layout));

  // Add easy unlock icon.
  easy_unlock_icon_ =
      password_row_->AddChildView(std::make_unique<EasyUnlockIcon>(
          gfx::Size(kEasyUnlockIconSizeDp, kEasyUnlockIconSizeDp),
          /*corner_radius=*/0));

  easy_unlock_right_margin_ =
      password_row_->AddChildView(std::make_unique<NonAccessibleView>());
  easy_unlock_right_margin_->SetPreferredSize(gfx::Size(
      kHorizontalDistanceBetweenEasyUnlockAndPasswordDp, kNonEmptyHeight));

  // Easy unlock starts invisible. There will be an event later to show it if
  // needed.
  easy_unlock_icon_->SetVisible(false);
  easy_unlock_right_margin_->SetVisible(false);

  // Password textfield. We control the textfield size by sizing the parent
  // view, as the textfield will expand to fill it.
  auto textfield = std::make_unique<LoginTextfield>(
      palette_,
      // Highlight on focus. Remove highlight on blur.
      base::BindRepeating(
          &LoginPasswordView::SetSeparatorAndCapsLockHighlighted,
          base::Unretained(this), /*highlight=*/true),
      base::BindRepeating(
          &LoginPasswordView::SetSeparatorAndCapsLockHighlighted,
          base::Unretained(this), /*highlight=*/false));
  textfield_ = password_row_->AddChildView(std::move(textfield));
  textfield_->set_controller(this);

  layout_ptr->SetFlexForView(textfield_, 1);

  // Caps lock hint icon.
  capslock_icon_ =
      password_row_->AddChildView(std::make_unique<views::ImageView>());
  // Caps lock hint starts invisible. This constructor will call
  // OnCapsLockChanged with the actual caps lock state.
  capslock_icon_->SetVisible(false);

  if (is_display_password_feature_enabled_) {
    display_password_button_ = password_row_->AddChildView(
        std::make_unique<DisplayPasswordButton>(palette_, this));
  }

  // Separator on bottom.
  separator_ =
      password->AddChildView(std::make_unique<NonAccessibleSeparator>());

  submit_button_ = AddChildView(std::make_unique<ArrowButtonView>(
      /*listener=*/this, kSubmitButtonSizeDp));
  const AshColorProvider* color_provider = AshColorProvider::Get();
  SkColor color = color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
  submit_button_->SetBackgroundColor(color);
  submit_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_SUBMIT_BUTTON_ACCESSIBLE_NAME));
  submit_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_SUBMIT_BUTTON_ACCESSIBLE_NAME));
  submit_button_->SetEnabled(false);

  // Initialize the capslock icon and the separator without a highlight.
  SetSeparatorAndCapsLockHighlighted(/*highlight=*/false);

  // Make sure the textfield always starts with focus.
  textfield_->RequestFocus();

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
    const OnPasswordTextChanged& on_password_text_changed,
    const OnEasyUnlockIconHovered& on_easy_unlock_icon_hovered,
    const OnEasyUnlockIconTapped& on_easy_unlock_icon_tapped) {
  DCHECK(on_submit);
  DCHECK(on_password_text_changed);
  on_submit_ = on_submit;
  on_password_text_changed_ = on_password_text_changed;
  easy_unlock_icon_->Init(on_easy_unlock_icon_hovered,
                          on_easy_unlock_icon_tapped);
}

void LoginPasswordView::SetEnabledOnEmptyPassword(bool enabled) {
  enabled_on_empty_password_ = enabled;
  UpdateUiState();
}

void LoginPasswordView::SetEasyUnlockIcon(
    EasyUnlockIconId id,
    const base::string16& accessibility_label) {
  // Update icon.
  easy_unlock_icon_->SetEasyUnlockIcon(id, accessibility_label);

  // Update icon visiblity.
  bool has_icon = id != EasyUnlockIconId::NONE;
  easy_unlock_icon_->SetVisible(has_icon);
  easy_unlock_right_margin_->SetVisible(has_icon);
  password_row_->Layout();
}

void LoginPasswordView::SetAccessibleName(const base::string16& name) {
  textfield_->SetAccessibleName(name);
}

void LoginPasswordView::SetFocusEnabledForChildViews(bool enable) {
  auto behavior = enable ? FocusBehavior::ALWAYS : FocusBehavior::NEVER;
  textfield_->SetFocusBehavior(behavior);
}

void LoginPasswordView::SetDisplayPasswordButtonVisible(bool visible) {
  if (!is_display_password_feature_enabled_)
    return;
  display_password_button_->SetVisible(visible);
  // Only start the timer if the display password button is enabled.
  if (visible) {
    clear_password_timer_->Start(
        FROM_HERE, kClearPasswordAfterDelay,
        base::BindRepeating(&LoginPasswordView::Clear, base::Unretained(this)));
  }
}

void LoginPasswordView::Reset() {
  Clear();

  if (is_display_password_feature_enabled_) {
    // A user could hit the display button, then quickly switch account and
    // type; we want the password to be hidden in such a case.
    HidePassword(false /*chromevox_exception*/);
  }
}

void LoginPasswordView::Clear() {
  textfield_->SetText(base::string16());
  // For security reasons, we also want to clear the edit history if the Clear
  // function is invoked by the clear password timer.
  textfield_->ClearEditHistory();
  // |ContentsChanged| won't be called by |Textfield| if the text is changed
  // by |Textfield::SetText()|.
  ContentsChanged(textfield_, textfield_->GetText());
}

void LoginPasswordView::InsertNumber(int value) {
  if (textfield_->GetReadOnly())
    return;

  if (!textfield_->HasFocus()) {
    // RequestFocus on textfield to activate cursor.
    textfield_->RequestFocus();
  }
  textfield_->InsertOrReplaceText(base::NumberToString16(value));
}

void LoginPasswordView::Backspace() {
  // Instead of just adjusting textfield_ text directly, fire a backspace key
  // event as this handles the various edge cases (ie, selected text).

  // views::Textfield::OnKeyPressed is private, so we call it via views::View.
  auto* view = static_cast<views::View*>(textfield_);
  view->OnKeyPressed(ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_BACK,
                                  ui::DomCode::BACKSPACE, ui::EF_NONE));
  view->OnKeyPressed(ui::KeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_BACK,
                                  ui::DomCode::BACKSPACE, ui::EF_NONE));
}

void LoginPasswordView::SetPlaceholderText(
    const base::string16& placeholder_text) {
  textfield_->SetPlaceholderText(placeholder_text);
  SchedulePaint();
}

void LoginPasswordView::SetReadOnly(bool read_only) {
  textfield_->SetReadOnly(read_only);
  textfield_->SetCursorEnabled(!read_only);
  UpdateUiState();
}

const char* LoginPasswordView::GetClassName() const {
  return kLoginPasswordViewName;
}

gfx::Size LoginPasswordView::CalculatePreferredSize() const {
  gfx::Size size = views::View::CalculatePreferredSize();
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
  display_password_button_->InvertToggled();
  textfield_->InvertTextInputType();
  hide_password_timer_->Start(
      FROM_HERE, kHidePasswordAfterDelay,
      base::BindRepeating(&LoginPasswordView::HidePassword,
                          base::Unretained(this),
                          true /*chromevox_exception*/));
}

void LoginPasswordView::ButtonPressed(views::Button* sender,
                                      const ui::Event& event) {
  if (sender == submit_button_) {
    SubmitPassword();
  } else if (is_display_password_feature_enabled_) {
    DCHECK_EQ(sender, display_password_button_);
    InvertPasswordDisplayingState();
  }
}

void LoginPasswordView::HidePassword(bool chromevox_exception) {
  if (chromevox_exception &&
      Shell::Get()->accessibility_controller()->spoken_feedback().enabled()) {
    return;
  }
  if (textfield_->GetTextInputType() == ui::TEXT_INPUT_TYPE_NULL)
    InvertPasswordDisplayingState();
}

void LoginPasswordView::ContentsChanged(views::Textfield* sender,
                                        const base::string16& new_contents) {
  DCHECK_EQ(sender, textfield_);
  UpdateUiState();
  on_password_text_changed_.Run(new_contents.empty() /*is_empty*/);

  if (!is_display_password_feature_enabled_)
    return;

  // If the password is currently revealed.
  if (textfield_->GetTextInputType() == ui::TEXT_INPUT_TYPE_NULL)
    hide_password_timer_->Reset();
  // The feature could be enabled on the device but disabled for this user by policy.
  if (display_password_button_->GetVisible())
    clear_password_timer_->Reset();
}

// Implements swapping active user with arrow keys
bool LoginPasswordView::HandleKeyEvent(views::Textfield* sender,
                                       const ui::KeyEvent& key_event) {
  // Treat the password field as normal if it has text
  if (!textfield_->GetText().empty())
    return false;

  if (key_event.type() != ui::ET_KEY_PRESSED)
    return false;

  if (key_event.is_repeat())
    return false;

  switch (key_event.key_code()) {
    case ui::VKEY_LEFT:
      LockScreen::Get()->FocusPreviousUser();
      break;
    case ui::VKEY_RIGHT:
      LockScreen::Get()->FocusNextUser();
      break;
    default:
      return false;
  }

  return true;
}

void LoginPasswordView::UpdateUiState() {
  bool enable_buttons = IsPasswordSubmittable();
  // Disabling the submit button will make it lose focus. The previous focusable
  // view will be the password textfield, which is more expected than the user
  // drop down button.
  if (!enable_buttons && submit_button_->HasFocus())
    textfield_->RequestFocus();
  submit_button_->SetEnabled(enable_buttons);

  if (!is_display_password_feature_enabled_)
    return;
  display_password_button_->SetEnabled(enable_buttons);
}

void LoginPasswordView::OnCapsLockChanged(bool enabled) {
  capslock_icon_->SetVisible(enabled);
  password_row_->Layout();
}

bool LoginPasswordView::IsPasswordSubmittable() {
  return !textfield_->GetReadOnly() &&
         (enabled_on_empty_password_ || !textfield_->GetText().empty());
}

void LoginPasswordView::SubmitPassword() {
  DCHECK(IsPasswordSubmittable());
  if (textfield_->GetReadOnly())
    return;
  SetReadOnly(true);
  on_submit_.Run(textfield_->GetText());
}

void LoginPasswordView::SetSeparatorAndCapsLockHighlighted(bool highlight) {
  SkColor color = palette_.button_enabled_color;
  if (!highlight)
    color = SkColorSetA(color, login_constants::kButtonDisabledAlpha);
  separator_->SetColor(color);
  capslock_icon_->SetImage(gfx::CreateVectorIcon(kLockScreenCapsLockIcon,
                                                 kCapsLockIconSizeDp, color));
}

}  // namespace ash
