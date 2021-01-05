// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_password_view.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/horizontal_image_sequence_animation_decoder.h"
#include "ash/login/ui/hover_notifier.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/login_constants.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "base/bind.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
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

// External padding on the password row and submit button, used for the focus
// ring.
constexpr const int kBorderForFocusRingDp = 3;

// Spacing between the icons (easy unlock, caps lock, display password) and the
// borders of the password row.
constexpr const int kInternalHorizontalPaddingPasswordRowDp = 6;

// Spacing between the password row and the submit button.
constexpr int kSpacingBetweenPasswordRowAndSubmitButtonDp =
    8 - 2 * kBorderForFocusRingDp;

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
// (which also contains the submit buttonn).
constexpr int kPasswordRowWidthDp = 204 + 2 * kBorderForFocusRingDp;

// Total width of the password view (left margin + password row + spacing +
// submit button).
constexpr int kPasswordTotalWidthDp =
    kLeftPaddingPasswordView + kPasswordRowWidthDp + kSubmitButtonSizeDp +
    kSpacingBetweenPasswordRowAndSubmitButtonDp;

// Delta between normal font and font of the typed text.
constexpr int kPasswordVisibleFontDeltaSize = 1;

// Delta between normal font and font of glyphs.
constexpr int kPasswordHiddenFontDeltaSize = 12;

// Spacing between glyphs.
constexpr int kPasswordGlyphSpacing = 6;

// Size (width/height) of the different icons belonging to the password row
// (the easy unlock icon, the display password icon and the caps lock icon).
constexpr const int kIconSizeDp = 20;

// Horizontal spacing between:
// -the easy unlock icon and the start of the password textfield
// -the end of the password textfield and the display password button.
constexpr const int kHorizontalSpacingBetweenIconsAndTextfieldDp = 8;

constexpr const int kPasswordRowCornerRadiusDp = 4;

constexpr const int kPasswordRowFocusRingRadiusDp = 6;

// Delay after which the password gets cleared if nothing has been typed. It is
// only effective if the display password button is shown, as there is no
// potential security threat otherwise.
constexpr base::TimeDelta kClearPasswordAfterDelay =
    base::TimeDelta::FromSeconds(30);

// Delay after which the password gets back to hidden state, for security.
constexpr base::TimeDelta kHidePasswordAfterDelay =
    base::TimeDelta::FromSeconds(5);

constexpr const char kLoginPasswordViewName[] = "LoginPasswordView";

// Duration i describes the transition from opacity i-1 to i.
// This means that we have a fade-in and fade-out of 0.5s each and we show the
// view at 100% opacity during 2s, except the first time where we show it 2.5s
// as there is no fade-in.
constexpr const int kAlternateAnimationSequencesDurationsInMs[] = {500, 2000,
                                                                   500};
constexpr const float kAlternateAnimationSequencesOpacities[] = {1.0f, 1.0f,
                                                                 0.0f};

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

// An observer that swaps two views' visibilities at each animation cycle.
class AnimationCycleEndObserver : public ui::LayerAnimationObserver {
 public:
  explicit AnimationCycleEndObserver(ui::LayerAnimator* animator) {
    observation_.Observe(animator);
  }
  ~AnimationCycleEndObserver() override = default;
  AnimationCycleEndObserver(const AnimationCycleEndObserver&) = delete;
  AnimationCycleEndObserver& operator=(const AnimationCycleEndObserver&) =
      delete;

  // ui::LayerAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override {}

  // ui::LayerAnimationObserver:
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {}

  // ui::LayerAnimationObserver:
  void OnLayerAnimationCycleEnded(
      ui::LayerAnimationSequence* sequence) override {
    shown_now_->SetVisible(false);
    shown_after_->SetVisible(true);
    std::swap(shown_now_, shown_after_);
  }

  // ui::LayerAnimationObserver:
  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}

  void SetDisplayOrder(views::View* shown_now, views::View* shown_after) {
    shown_now_ = shown_now;
    shown_after_ = shown_after;
  }

 private:
  base::ScopedObservation<ui::LayerAnimator, ui::LayerAnimationObserver>
      observation_{this};
  views::View* shown_now_;
  views::View* shown_after_;
};

}  // namespace

// The login password row contains the password textfield and different buttons
// and indicators (easy unlock, display password, caps lock enabled).
class LoginPasswordView::LoginPasswordRow : public views::View {
 public:
  LoginPasswordRow() : focus_ring_(views::FocusRing::Install(this)) {
    focus_ring_->SetColor(ShelfConfig::Get()->shelf_focus_border_color());
    views::InstallRoundRectHighlightPathGenerator(
        this, gfx::Insets(), kPasswordRowFocusRingRadiusDp);

    focus_ring_->SetHasFocusPredicate([](View* view) {
      return static_cast<LoginPasswordRow*>(view)->is_highlighted_;
    });

    SetBorder(views::CreateEmptyBorder(gfx::Insets(kBorderForFocusRingDp)));
  }
  ~LoginPasswordRow() override = default;
  LoginPasswordRow(const LoginPasswordRow&) = delete;
  LoginPasswordRow& operator=(const LoginPasswordRow&) = delete;

  void SetHighlight(bool enabled) {
    is_highlighted_ = enabled;
    focus_ring_->SchedulePaint();
  }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);

    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(AshColorProvider::Get()->GetControlsLayerColor(
        AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive));
    canvas->DrawRoundRect(GetContentsBounds(), kPasswordRowCornerRadiusDp,
                          flags);
  }

 private:
  bool is_highlighted_ = false;
  views::FocusRing* focus_ring_;
};

// A textfield that selects all text on focus and allows to switch between
// show/hide password modes.
class LoginPasswordView::LoginTextfield : public views::Textfield {
 public:
  LoginTextfield(const LoginPalette& palette,
                 base::RepeatingClosure on_focus_closure,
                 base::RepeatingClosure on_blur_closure,
                 base::RepeatingClosure on_tab_focus_closure)
      : on_focus_closure_(std::move(on_focus_closure)),
        on_blur_closure_(std::move(on_blur_closure)),
        on_tab_focus_closure_(std::move(on_tab_focus_closure)) {
    SetTextColor(palette.password_text_color);
    SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
    UpdateFontListAndCursor();
    set_placeholder_font_list(font_list_visible_);
    set_placeholder_text_color(palette.password_placeholder_text_color);
    SetObscuredGlyphSpacing(kPasswordGlyphSpacing);
    SetBorder(nullptr);
    SetBackgroundColor(palette.password_background_color);
    SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
    set_placeholder_text_draw_flags(gfx::Canvas::TEXT_ALIGN_CENTER);
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

  // views::Textfield:
  void OnFocus() override {
    if (on_focus_closure_)
      on_focus_closure_.Run();
    views::Textfield::OnFocus();
  }

  void AboutToRequestFocusFromTabTraversal(bool reverse) override {
    if (on_tab_focus_closure_)
      on_tab_focus_closure_.Run();

    if (!GetText().empty())
      SelectAll(/*reversed=*/false);
  }

  void UpdateFontListAndCursor() {
    SetCursorEnabled(GetText().empty());
    // We do not want the cursor to be too big, therefore the hidden font is
    // set only when there is no cursor.
    if (GetTextInputType() == ui::TEXT_INPUT_TYPE_PASSWORD &&
        !GetCursorEnabled()) {
      SetFontList(font_list_hidden_);
    } else {
      SetFontList(font_list_visible_);
    }
  }

  // Switches between normal input and password input when the user hits the
  // display password button.
  void InvertTextInputType() {
    if (GetTextInputType() == ui::TEXT_INPUT_TYPE_NULL)
      SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
    else
      SetTextInputType(ui::TEXT_INPUT_TYPE_NULL);

    UpdateFontListAndCursor();
  }

  // This is useful when the display password button is not shown. In such a
  // case, the login text field needs to define its size.
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(kPasswordTotalWidthDp, kIconSizeDp);
  }

 private:
  const gfx::FontList font_list_visible_ =
      views::Textfield::GetDefaultFontList().Derive(
          kPasswordVisibleFontDeltaSize,
          gfx::Font::FontStyle::NORMAL,
          gfx::Font::Weight::NORMAL);
  const gfx::FontList font_list_hidden_ =
      views::Textfield::GetDefaultFontList().Derive(
          kPasswordHiddenFontDeltaSize,
          gfx::Font::FontStyle::NORMAL,
          gfx::Font::Weight::NORMAL);

  // Closures that will be called when the element receives and loses focus.
  base::RepeatingClosure on_focus_closure_;
  base::RepeatingClosure on_blur_closure_;
  base::RepeatingClosure on_tab_focus_closure_;
};

class LoginPasswordView::EasyUnlockIcon : public views::Button {
 public:
  EasyUnlockIcon(const gfx::Size& size, int corner_radius)
      : views::Button(PressedCallback()) {
    SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
    SetPreferredSize(size);
    SetLayoutManager(std::make_unique<views::FillLayout>());
    icon_ = AddChildView(
        std::make_unique<AnimatedRoundedImageView>(size, corner_radius));
  }
  ~EasyUnlockIcon() override = default;

  void Init(const OnEasyUnlockIconHovered& on_hovered,
            views::Button::PressedCallback on_tapped) {
    DCHECK(on_hovered);

    on_hovered_ = on_hovered;
    SetCallback(std::move(on_tapped));

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
                        views::Button::PressedCallback callback)
      : ToggleImageButton(std::move(callback)) {
    const gfx::ImageSkia invisible_icon =
        gfx::CreateVectorIcon(kLockScreenPasswordInvisibleIcon, kIconSizeDp,
                              palette.button_enabled_color);
    const gfx::ImageSkia visible_icon =
        gfx::CreateVectorIcon(kLockScreenPasswordVisibleIcon, kIconSizeDp,
                              palette.button_enabled_color);
    const gfx::ImageSkia visible_icon_disabled = gfx::CreateVectorIcon(
        kLockScreenPasswordVisibleIcon, kIconSizeDp,
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

// A container view that either shows the easy unlock icon or the caps lock
// icon. When both should be displayed, it starts an animation transitioning
// from one to the other alternatively.
class LoginPasswordView::AlternateIconsView : public views::View {
 public:
  AlternateIconsView() {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);

    observer_ =
        std::make_unique<AnimationCycleEndObserver>(layer()->GetAnimator());
  }
  AlternateIconsView(const AlternateIconsView&) = delete;
  AlternateIconsView& operator=(const AlternateIconsView&) = delete;
  ~AlternateIconsView() override = default;

  void ScheduleAnimation(views::View* shown_now, views::View* shown_after) {
    DCHECK(layer());

    shown_now->SetVisible(true);
    shown_after->SetVisible(false);

    // Do not alternate between icons if ChromeVox is enabled.
    if (Shell::Get()->accessibility_controller()->spoken_feedback().enabled())
      return;

    observer_->SetDisplayOrder(shown_now, shown_after);

    std::unique_ptr<ui::LayerAnimationSequence> opacity_sequence =
        std::make_unique<ui::LayerAnimationSequence>();
    opacity_sequence->set_is_cyclic(true);
    for (size_t i = 0; i < base::size(kAlternateAnimationSequencesOpacities);
         ++i) {
      opacity_sequence->AddElement(
          ui::LayerAnimationElement::CreateOpacityElement(
              kAlternateAnimationSequencesOpacities[i],
              base::TimeDelta::FromMilliseconds(
                  kAlternateAnimationSequencesDurationsInMs[i])));
    }

    layer()->GetAnimator()->ScheduleAnimation(opacity_sequence.release());
  }

  void AbortAnimationIfAny() { layer()->GetAnimator()->AbortAllAnimations(); }

 private:
  std::unique_ptr<AnimationCycleEndObserver> observer_;
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

views::View* LoginPasswordView::TestApi::capslock_icon() const {
  return view_->capslock_icon_;
}

void LoginPasswordView::TestApi::set_immediately_hover_easy_unlock_icon() {
  view_->easy_unlock_icon_->set_immediately_hover_for_test();
}

LoginPasswordView::LoginPasswordView(const LoginPalette& palette)
    : clear_password_timer_(std::make_unique<base::RetainingOneShotTimer>()),
      hide_password_timer_(std::make_unique<base::RetainingOneShotTimer>()),
      palette_(palette),
      capslock_icon_(new views::ImageView()),
      easy_unlock_icon_(new EasyUnlockIcon(gfx::Size(kIconSizeDp, kIconSizeDp),
                                           /*corner_radius=*/0)) {
  Shell::Get()->ime_controller()->AddObserver(this);

  // Contains the password layout on the left and the submit button on the
  // right.
  auto* root_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(0, kLeftPaddingPasswordView, 0, 0),
      kSpacingBetweenPasswordRowAndSubmitButtonDp));
  root_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);

  password_row_ = AddChildView(std::make_unique<LoginPasswordRow>());
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(0, kInternalHorizontalPaddingPasswordRowDp),
      kHorizontalSpacingBetweenIconsAndTextfieldDp);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  auto* layout_ptr = password_row_->SetLayoutManager(std::move(layout));

  easy_unlock_icon_->SetVisible(false);
  capslock_icon_->SetVisible(false);
  left_icon_ =
      password_row_->AddChildView(std::make_unique<AlternateIconsView>());
  left_icon_->SetLayoutManager(std::make_unique<views::FillLayout>());
  left_icon_->AddChildView(easy_unlock_icon_);
  left_icon_->AddChildView(capslock_icon_);

  // Password textfield. We control the textfield size by sizing the parent
  // view, as the textfield will expand to fill it.
  auto textfield = std::make_unique<LoginTextfield>(
      palette_,
      // Highlight on focus. Remove highlight on blur.
      base::BindRepeating(&LoginPasswordView::SetCapsLockHighlighted,
                          base::Unretained(this), /*highlight=*/true),
      base::BindRepeating(&LoginPasswordView::RemoveHighlightFromCapsLockAndRow,
                          base::Unretained(this)),
      base::BindRepeating(&LoginPasswordView::SetPasswordRowHighlighted,
                          base::Unretained(this), /*highlight=*/true));
  textfield_ = password_row_->AddChildView(std::move(textfield));
  textfield_->set_controller(this);

  layout_ptr->SetFlexForView(textfield_, 1);

  display_password_button_ =
      password_row_->AddChildView(std::make_unique<DisplayPasswordButton>(
          palette_, base::BindRepeating(
                        [](LoginPasswordView* view) {
                          view->InvertPasswordDisplayingState();
                        },
                        this)));

  submit_button_ = AddChildView(std::make_unique<ArrowButtonView>(
      base::BindRepeating(&LoginPasswordView::SubmitPassword,
                          base::Unretained(this)),
      kSubmitButtonContentSizeDp));
  const AshColorProvider* color_provider = AshColorProvider::Get();
  SkColor color = color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
  submit_button_->SetBackgroundColor(color);
  submit_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_SUBMIT_BUTTON_ACCESSIBLE_NAME));
  submit_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_SUBMIT_BUTTON_ACCESSIBLE_NAME));
  submit_button_->SetEnabled(false);

  // Initialize the capslock icon without a highlight.
  SetCapsLockHighlighted(/*highlight=*/false);

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
    const OnPasswordTextChanged& on_password_text_changed,
    const OnEasyUnlockIconHovered& on_easy_unlock_icon_hovered,
    views::Button::PressedCallback on_easy_unlock_icon_tapped) {
  DCHECK(on_submit);
  DCHECK(on_password_text_changed);
  on_submit_ = on_submit;
  on_password_text_changed_ = on_password_text_changed;
  easy_unlock_icon_->Init(on_easy_unlock_icon_hovered,
                          std::move(on_easy_unlock_icon_tapped));
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
  // We do not want to schedule a new animation when the user switches from an
  // account to another.
  if (should_show_easy_unlock_ == has_icon)
    return;
  should_show_easy_unlock_ = has_icon;
  HandleLeftIconsVisibilities(false /*handling_capslock*/);
}

void LoginPasswordView::SetAccessibleName(const base::string16& name) {
  textfield_->SetAccessibleName(name);
}

void LoginPasswordView::SetFocusEnabledForTextfield(bool enable) {
  auto behavior = enable ? FocusBehavior::ALWAYS : FocusBehavior::NEVER;
  textfield_->SetFocusBehavior(behavior);
}

void LoginPasswordView::SetDisplayPasswordButtonVisible(bool visible) {
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

  // A user could hit the display button, then quickly switch account and
  // type; we want the password to be hidden in such a case.
  HidePassword(false /*chromevox_exception*/);
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
    RequestFocus();
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
  textfield_->UpdateFontListAndCursor();
  on_password_text_changed_.Run(new_contents.empty() /*is_empty*/);

  // If the password is currently revealed.
  if (textfield_->GetTextInputType() == ui::TEXT_INPUT_TYPE_NULL)
    hide_password_timer_->Reset();

  // The display password button could be hidden by user policy.
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
    RequestFocus();
  submit_button_->SetEnabled(enable_buttons);
  display_password_button_->SetEnabled(enable_buttons);
}

void LoginPasswordView::OnCapsLockChanged(bool enabled) {
  should_show_capslock_ = enabled;
  HandleLeftIconsVisibilities(true /*handling_capslock*/);
}

bool LoginPasswordView::IsPasswordSubmittable() {
  return !textfield_->GetReadOnly() &&
         (enabled_on_empty_password_ || !textfield_->GetText().empty());
}

void LoginPasswordView::HandleLeftIconsVisibilities(bool handling_capslock) {
  views::View* handled_view = easy_unlock_icon_;
  views::View* other_view = capslock_icon_;
  bool handled_should_show = should_show_easy_unlock_;
  bool other_should_show = should_show_capslock_;
  if (handling_capslock) {
    std::swap(handled_view, other_view);
    std::swap(handled_should_show, other_should_show);
  }

  if (handled_should_show) {
    handled_view->SetVisible(true);
    if (other_should_show) {
      other_view->SetVisible(false);
      left_icon_->ScheduleAnimation(handled_view, other_view);
    }
  } else {
    left_icon_->AbortAnimationIfAny();
    other_view->SetVisible(should_show_easy_unlock_);
    handled_view->SetVisible(false);
  }
  password_row_->Layout();
}

void LoginPasswordView::SubmitPassword() {
  DCHECK(IsPasswordSubmittable());
  if (textfield_->GetReadOnly())
    return;
  SetReadOnly(true);
  on_submit_.Run(textfield_->GetText());
}

void LoginPasswordView::SetCapsLockHighlighted(bool highlight) {
  SkColor color = palette_.button_enabled_color;
  if (!highlight)
    color = SkColorSetA(color, login_constants::kButtonDisabledAlpha);
  capslock_icon_->SetImage(
      gfx::CreateVectorIcon(kLockScreenCapsLockIcon, kIconSizeDp, color));
}

void LoginPasswordView::SetPasswordRowHighlighted(bool highlight) {
  password_row_->SetHighlight(highlight);
}

void LoginPasswordView::RemoveHighlightFromCapsLockAndRow() {
  SetCapsLockHighlighted(false);
  SetPasswordRowHighlighted(false);
}

}  // namespace ash
