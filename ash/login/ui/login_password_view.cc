// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_password_view.h"

#include "ash/login/ui/horizontal_image_sequence_animation_decoder.h"
#include "ash/login/ui/hover_notifier.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_button.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/public/cpp/login_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/user/button_from_view.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
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

// Total width of the password view.
constexpr int kPasswordTotalWidthDp = 204;

// Size (width/height) of the submit button.
constexpr int kSubmitButtonSizeDp = 20;

// Size (width/height) of the caps lock hint icon.
constexpr int kCapsLockIconSizeDp = 20;

// Width and height of the easy unlock icon.
constexpr const int kEasyUnlockIconSizeDp = 20;

// Horizontal distance/margin between the easy unlock icon and the start of
// the password view.
constexpr const int kHorizontalDistanceBetweenEasyUnlockAndPasswordDp = 12;

// Non-empty height, useful for debugging/visualization.
constexpr const int kNonEmptyHeight = 1;

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

// A textfield that selects all text on focus.
class LoginTextfield : public views::Textfield {
 public:
  LoginTextfield() {}
  ~LoginTextfield() override {}

  // views::Textfield:
  void OnFocus() override {
    views::Textfield::OnFocus();
    SelectAll(false /*reverse*/);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginTextfield);
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

// Construct an IconBundle instance for a given mojom::EasyUnlockIconId value.
IconBundle GetEasyUnlockResources(mojom::EasyUnlockIconId id) {
  switch (id) {
    case mojom::EasyUnlockIconId::NONE:
      break;
    case mojom::EasyUnlockIconId::HARDLOCKED:
      return IconBundle(IDR_EASY_UNLOCK_HARDLOCKED,
                        IDR_EASY_UNLOCK_HARDLOCKED_HOVER,
                        IDR_EASY_UNLOCK_HARDLOCKED_PRESSED);
    case mojom::EasyUnlockIconId::LOCKED:
      return IconBundle(IDR_EASY_UNLOCK_LOCKED, IDR_EASY_UNLOCK_LOCKED_HOVER,
                        IDR_EASY_UNLOCK_LOCKED_PRESSED);
    case mojom::EasyUnlockIconId::LOCKED_TO_BE_ACTIVATED:
      return IconBundle(IDR_EASY_UNLOCK_LOCKED_TO_BE_ACTIVATED,
                        IDR_EASY_UNLOCK_LOCKED_TO_BE_ACTIVATED_HOVER,
                        IDR_EASY_UNLOCK_LOCKED_TO_BE_ACTIVATED_PRESSED);
    case mojom::EasyUnlockIconId::LOCKED_WITH_PROXIMITY_HINT:
      return IconBundle(IDR_EASY_UNLOCK_LOCKED_WITH_PROXIMITY_HINT,
                        IDR_EASY_UNLOCK_LOCKED_WITH_PROXIMITY_HINT_HOVER,
                        IDR_EASY_UNLOCK_LOCKED_WITH_PROXIMITY_HINT_PRESSED);
    case mojom::EasyUnlockIconId::UNLOCKED:
      return IconBundle(IDR_EASY_UNLOCK_UNLOCKED,
                        IDR_EASY_UNLOCK_UNLOCKED_HOVER,
                        IDR_EASY_UNLOCK_UNLOCKED_PRESSED);
    case mojom::EasyUnlockIconId::SPINNER:
      return IconBundle(IDR_EASY_UNLOCK_SPINNER,
                        base::TimeDelta::FromSeconds(2), 45 /*num_frames*/);
  }

  NOTREACHED();
  return IconBundle(IDR_EASY_UNLOCK_LOCKED, IDR_EASY_UNLOCK_LOCKED_HOVER,
                    IDR_EASY_UNLOCK_LOCKED_PRESSED);
}

}  // namespace

class LoginPasswordView::EasyUnlockIcon : public views::Button,
                                          public views::ButtonListener {
 public:
  EasyUnlockIcon(const gfx::Size& size, int corner_radius)
      : views::Button(this) {
    SetPreferredSize(size);
    SetLayoutManager(std::make_unique<views::FillLayout>());
    icon_ = new AnimatedRoundedImageView(size, corner_radius);
    AddChildView(icon_);
  }
  ~EasyUnlockIcon() override = default;

  void Init(const OnEasyUnlockIconHovered& on_hovered,
            const OnEasyUnlockIconTapped& on_tapped) {
    DCHECK(on_hovered);
    DCHECK(on_tapped);

    on_hovered_ = on_hovered;
    on_tapped_ = on_tapped;

    hover_notifier_ = std::make_unique<HoverNotifier>(
        this,
        base::Bind(&LoginPasswordView::EasyUnlockIcon::OnHoverStateChanged,
                   base::Unretained(this)));
  }

  void SetEasyUnlockIcon(mojom::EasyUnlockIconId icon_id,
                         const base::string16& accessibility_label) {
    bool changed_states = icon_id != icon_id_;
    icon_id_ = icon_id;
    UpdateImage(changed_states);
    SetAccessibleName(accessibility_label);
  }

  void set_immediately_hover_for_test() { immediately_hover_for_test_ = true; }

  // views::Button:
  void StateChanged(ButtonState old_state) override {
    // Stop showing tooltip, as we most likely exited hover state.
    invoke_hover_.Stop();

    switch (state()) {
      case ButtonState::STATE_NORMAL:
        UpdateImage(false /*changed_states*/);
        break;
      case ButtonState::STATE_HOVERED:
        UpdateImage(false /*changed_states*/);
        if (immediately_hover_for_test_) {
          on_hovered_.Run();
        } else {
          invoke_hover_.Start(
              FROM_HERE,
              base::TimeDelta::FromMilliseconds(kDelayBeforeShowingTooltipMs),
              on_hovered_);
        }
        break;
      case ButtonState::STATE_PRESSED:
        UpdateImage(false /*changed_states*/);
        break;
      case ButtonState::STATE_DISABLED:
        break;
      case ButtonState::STATE_COUNT:
        break;
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

    if (icon_id_ == mojom::EasyUnlockIconId::NONE)
      return;

    IconBundle resources = GetEasyUnlockResources(icon_id_);

    int active_resource = resources.normal;
    if (IsMouseHovered())
      active_resource = resources.hover;
    if (state() == ButtonState::STATE_PRESSED)
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
  mojom::EasyUnlockIconId icon_id_ = mojom::EasyUnlockIconId::NONE;

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

LoginPasswordView::TestApi::TestApi(LoginPasswordView* view) : view_(view) {}

LoginPasswordView::TestApi::~TestApi() = default;

views::Textfield* LoginPasswordView::TestApi::textfield() const {
  return view_->textfield_;
}

views::View* LoginPasswordView::TestApi::submit_button() const {
  return view_->submit_button_;
}

views::View* LoginPasswordView::TestApi::easy_unlock_icon() const {
  return view_->easy_unlock_icon_;
}

void LoginPasswordView::TestApi::set_immediately_hover_easy_unlock_icon() {
  view_->easy_unlock_icon_->set_immediately_hover_for_test();
}

LoginPasswordView::LoginPasswordView() {
  Shell::Get()->ime_controller()->AddObserver(this);

  auto* root_layout = SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
  root_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);

  password_row_ = new NonAccessibleView();

  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal,
      gfx::Insets(kMarginAboveBelowPasswordIconsDp, 0));
  layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  auto* layout_ptr = password_row_->SetLayoutManager(std::move(layout));
  AddChildView(password_row_);

  // Add easy unlock icon.
  easy_unlock_icon_ = new EasyUnlockIcon(
      gfx::Size(kEasyUnlockIconSizeDp, kEasyUnlockIconSizeDp),
      0 /*corner_radius*/);
  password_row_->AddChildView(easy_unlock_icon_);

  easy_unlock_right_margin_ = new NonAccessibleView();
  easy_unlock_right_margin_->SetPreferredSize(gfx::Size(
      kHorizontalDistanceBetweenEasyUnlockAndPasswordDp, kNonEmptyHeight));
  password_row_->AddChildView(easy_unlock_right_margin_);

  // Easy unlock starts invisible. There will be an event later to show it if
  // needed.
  easy_unlock_icon_->SetVisible(false);
  easy_unlock_right_margin_->SetVisible(false);

  // Password textfield. We control the textfield size by sizing the parent
  // view, as the textfield will expand to fill it.
  textfield_ = new LoginTextfield();
  textfield_->set_controller(this);
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  textfield_->SetTextColor(login_constants::kAuthMethodsTextColor);
  textfield_->SetFontList(views::Textfield::GetDefaultFontList().Derive(
      5, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL));
  textfield_->set_placeholder_font_list(views::Textfield::GetDefaultFontList());
  textfield_->set_placeholder_text_color(
      login_constants::kAuthMethodsTextColor);
  textfield_->SetGlyphSpacing(6);
  textfield_->SetBorder(nullptr);
  textfield_->SetBackgroundColor(SK_ColorTRANSPARENT);

  password_row_->AddChildView(textfield_);
  layout_ptr->SetFlexForView(textfield_, 1);

  // Caps lock hint icon.
  capslock_icon_ = new views::ImageView();
  capslock_icon_->SetImage(gfx::CreateVectorIcon(
      kLockScreenCapsLockIcon, kCapsLockIconSizeDp,
      SkColorSetA(login_constants::kButtonEnabledColor,
                  login_constants::kButtonDisabledAlpha)));
  password_row_->AddChildView(capslock_icon_);
  // Caps lock hint starts invisible. This constructor will call
  // OnCapsLockChanged with the actual caps lock state.
  capslock_icon_->SetVisible(false);

  // Submit button.
  submit_button_ = new LoginButton(this);
  submit_button_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(kLockScreenArrowIcon, kSubmitButtonSizeDp,
                            login_constants::kButtonEnabledColor));
  submit_button_->SetImage(
      views::Button::STATE_DISABLED,
      gfx::CreateVectorIcon(
          kLockScreenArrowIcon, kSubmitButtonSizeDp,
          SkColorSetA(login_constants::kButtonEnabledColor,
                      login_constants::kButtonDisabledAlpha)));
  submit_button_->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_ASH_LOGIN_POD_SUBMIT_BUTTON_ACCESSIBLE_NAME));
  password_row_->AddChildView(submit_button_);

  // Separator on bottom.
  separator_ = new NonAccessibleSeparator();
  AddChildView(separator_);

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
    mojom::EasyUnlockIconId id,
    const base::string16& accessibility_label) {
  // Update icon.
  easy_unlock_icon_->SetEasyUnlockIcon(id, accessibility_label);

  // Update icon visiblity.
  bool has_icon = id != mojom::EasyUnlockIconId::NONE;
  easy_unlock_icon_->SetVisible(has_icon);
  easy_unlock_right_margin_->SetVisible(has_icon);
  password_row_->Layout();
}

void LoginPasswordView::UpdateForUser(const mojom::LoginUserInfoPtr& user) {
  textfield_->SetAccessibleName(l10n_util::GetStringFUTF16(
      IDS_ASH_LOGIN_POD_PASSWORD_FIELD_ACCESSIBLE_NAME,
      base::UTF8ToUTF16(user->basic_user_info->display_email)));
}

void LoginPasswordView::SetFocusEnabledForChildViews(bool enable) {
  auto behavior = enable ? FocusBehavior::ALWAYS : FocusBehavior::NEVER;
  textfield_->SetFocusBehavior(behavior);
}

void LoginPasswordView::Clear() {
  textfield_->SetText(base::string16());
  // |ContentsChanged| won't be called by |Textfield| if the text is changed
  // by |Textfield::SetText()|.
  ContentsChanged(textfield_, textfield_->text());
}

void LoginPasswordView::InsertNumber(int value) {
  textfield_->InsertOrReplaceText(base::IntToString16(value));
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
  textfield_->set_placeholder_text(placeholder_text);
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
      submit_button_->enabled()) {
    SubmitPassword();
    return true;
  }

  return false;
}

void LoginPasswordView::ButtonPressed(views::Button* sender,
                                      const ui::Event& event) {
  DCHECK_EQ(sender, submit_button_);
  SubmitPassword();
}

void LoginPasswordView::ContentsChanged(views::Textfield* sender,
                                        const base::string16& new_contents) {
  DCHECK_EQ(sender, textfield_);
  UpdateUiState();
  on_password_text_changed_.Run(new_contents.empty() /*is_empty*/);
}

// Implements swapping active user with arrow keys
bool LoginPasswordView::HandleKeyEvent(views::Textfield* sender,
                                       const ui::KeyEvent& key_event) {
  // Treat the password field as normal if it has text
  if (!textfield_->text().empty())
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
  bool is_enabled = !textfield_->read_only() &&
                    (enabled_on_empty_password_ || !textfield_->text().empty());
  submit_button_->SetEnabled(is_enabled);
  SkColor color = is_enabled
                      ? login_constants::kButtonEnabledColor
                      : SkColorSetA(login_constants::kButtonEnabledColor,
                                    login_constants::kButtonDisabledAlpha);
  separator_->SetColor(color);
  capslock_icon_->SetImage(gfx::CreateVectorIcon(kLockScreenCapsLockIcon,
                                                 kCapsLockIconSizeDp, color));
}

void LoginPasswordView::OnCapsLockChanged(bool enabled) {
  capslock_icon_->SetVisible(enabled);
  password_row_->Layout();
}

void LoginPasswordView::SubmitPassword() {
  DCHECK(submit_button_->enabled());
  if (textfield_->read_only())
    return;
  on_submit_.Run(textfield_->text());
}

}  // namespace ash
