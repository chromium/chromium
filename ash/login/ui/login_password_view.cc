// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_password_view.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/arrow_button_view.h"
#include "ash/login/ui/horizontal_image_sequence_animation_decoder.h"
#include "ash/login/ui/hover_notifier.h"
#include "ash/login/ui/lock_screen.h"
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
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
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
#include "ui/views/controls/focus_ring.h"
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

// External padding on the submit button, used for the focus ring.
constexpr const int kBorderForFocusRingDp = 3;

// Spacing between the icons (easy unlock, caps lock, display password) and the
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
// (the easy unlock icon, the display password icon and the caps lock icon).
constexpr const int kIconSizeDp = 20;

// Horizontal spacing between:
// -the easy unlock icon and the start of the password textfield
// -the end of the password textfield and the display password button.
// Note that the password textfield has a 2dp margin so the ending result will
// be 8dp.
constexpr const int kHorizontalSpacingBetweenIconsAndTextfieldDp = 6;

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

constexpr const int kPasswordRowCornerRadiusDp = 4;

// Delay after which the password gets cleared if nothing has been typed. It is
// only running if the display password button is shown, as there is no
// potential security threat otherwise.
constexpr base::TimeDelta kClearPasswordAfterDelay = base::Seconds(30);

// Delay after which the password gets back to hidden state, for security.
constexpr base::TimeDelta kHidePasswordAfterDelay = base::Seconds(5);

struct FrameParams {
  FrameParams(int duration_in_ms, float opacity_param)
      : duration(base::Milliseconds(duration_in_ms)), opacity(opacity_param) {}

  base::TimeDelta duration;
  float opacity;
};

// Duration i describes the transition from opacity i-1 to i.
// This means that we have a fade-in and fade-out of 0.5s each and we show the
// view at 100% opacity during 2s, except the first time where we show it 2.5s
// as there is no fade-in.
const FrameParams kAlternateFramesParams[] = {{500, 1.0f},
                                              {2000, 1.0f},
                                              {500, 0.0f}};

const FrameParams kSpinnerFramesParams[] = {{500, 1.0f}, {500, 0.5f}};

// An observer that swaps two views' visibilities at each animation repetition.
class AnimationWillRepeatObserver : public ui::LayerAnimationObserver {
 public:
  AnimationWillRepeatObserver() {}
  ~AnimationWillRepeatObserver() override = default;
  AnimationWillRepeatObserver(const AnimationWillRepeatObserver&) = delete;
  AnimationWillRepeatObserver& operator=(const AnimationWillRepeatObserver&) =
      delete;

  // ui::LayerAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override {}

  // ui::LayerAnimationObserver:
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {}

  // ui::LayerAnimationObserver:
  void OnLayerAnimationWillRepeat(
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
  raw_ptr<views::View, ExperimentalAsh> shown_now_;
  raw_ptr<views::View, ExperimentalAsh> shown_after_;
};

}  // namespace

// The login password row contains the password textfield and different buttons
// and indicators (easy unlock, display password, caps lock enabled).
class LoginPasswordView::LoginPasswordRow : public views::View {
 public:
  explicit LoginPasswordRow() = default;

  ~LoginPasswordRow() override = default;
  LoginPasswordRow(const LoginPasswordRow&) = delete;
  LoginPasswordRow& operator=(const LoginPasswordRow&) = delete;

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
};

// A textfield that selects all text on focus and allows to switch between
// show/hide password modes.
class LoginPasswordView::LoginTextfield : public views::Textfield {
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
  gfx::Size CalculatePreferredSize() const override {
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

class LoginPasswordView::EasyUnlockIcon : public views::ImageButton {
 public:
  EasyUnlockIcon() : views::ImageButton(PressedCallback()) {
    SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  }

  EasyUnlockIcon(const EasyUnlockIcon&) = delete;
  EasyUnlockIcon& operator=(const EasyUnlockIcon&) = delete;

  ~EasyUnlockIcon() override = default;

  void Init(const OnEasyUnlockIconHovered& on_hovered) {
    DCHECK(on_hovered);

    on_hovered_ = on_hovered;

    hover_notifier_ = std::make_unique<HoverNotifier>(
        this, base::BindRepeating(
                  &LoginPasswordView::EasyUnlockIcon::OnHoverStateChanged,
                  base::Unretained(this)));
  }

  void SetEasyUnlockIcon(EasyUnlockIconState icon_state,
                         const std::u16string& accessibility_label) {
    icon_state_ = icon_state;
    UpdateImage(icon_state);
    SetAccessibleName(accessibility_label);
  }

  void set_immediately_hover_for_test() { immediately_hover_for_test_ = true; }

  // views::View:
  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    UpdateImage(icon_state_);
  }

  // views::Button:
  void StateChanged(ButtonState old_state) override {
    Button::StateChanged(old_state);

    // Stop showing tooltip, as we most likely exited hover state.
    invoke_hover_.Stop();

    if (GetState() == ButtonState::STATE_HOVERED) {
      if (immediately_hover_for_test_) {
        on_hovered_.Run();
      } else {
        invoke_hover_.Start(FROM_HERE,
                            base::Milliseconds(kDelayBeforeShowingTooltipMs),
                            on_hovered_);
      }
    }
  }

 private:
  void OnHoverStateChanged(bool has_hover) {
    SetState(has_hover ? ButtonState::STATE_HOVERED
                       : ButtonState::STATE_NORMAL);
  }

  void UpdateImage(EasyUnlockIconState icon_state) {
    // If the icon state changes from EasyUnlockIconState::SPINNER to something
    // else, we need to abort the current opacity animation and set back the
    // opacity to 100%. This can be done by destroying the layer that we do not
    // use anymore.
    if (layer()) {
      DestroyLayer();
    }

    const gfx::VectorIcon* icon = &kLockScreenEasyUnlockCloseIcon;
    const auto* color_provider = AshColorProvider::Get();
    SkColor color = color_provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kIconColorPrimary);

    switch (icon_state) {
      case EasyUnlockIconState::NONE:
        // The easy unlock icon will be set to invisible. Do nothing.
        break;
      case EasyUnlockIconState::LOCKED:
        // This is the default case in terms of icon and color.
        break;
      case EasyUnlockIconState::LOCKED_TO_BE_ACTIVATED:
        color =
            ColorUtil::GetDisabledColor(color_provider->GetContentLayerColor(
                AshColorProvider::ContentLayerType::kIconColorPrimary));
        break;
      case EasyUnlockIconState::LOCKED_WITH_PROXIMITY_HINT:
        color = color_provider->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kIconColorWarning);
        break;
      case EasyUnlockIconState::UNLOCKED:
        icon = &kLockScreenEasyUnlockOpenIcon;
        color = color_provider->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kIconColorPositive);
        break;
      case EasyUnlockIconState::SPINNER: {
        SetPaintToLayer();
        layer()->SetFillsBoundsOpaquely(false);
        std::unique_ptr<ui::LayerAnimationSequence> opacity_sequence =
            std::make_unique<ui::LayerAnimationSequence>();
        opacity_sequence->set_is_repeating(true);
        for (size_t i = 0; i < std::size(kSpinnerFramesParams); ++i) {
          opacity_sequence->AddElement(
              ui::LayerAnimationElement::CreateOpacityElement(
                  kSpinnerFramesParams[i].opacity,
                  kSpinnerFramesParams[i].duration));
        }
        layer()->GetAnimator()->ScheduleAnimation(opacity_sequence.release());
        break;
      }
      default:
        NOTREACHED();
    }

    const gfx::ImageSkia vector_icon =
        gfx::CreateVectorIcon(*icon, kIconSizeDp, color);

    SetImage(views::Button::STATE_NORMAL, vector_icon);
  }

  // Callbacks run when icon is hovered or tapped.
  OnEasyUnlockIconHovered on_hovered_;

  std::unique_ptr<HoverNotifier> hover_notifier_;

  // Timer used to control when we invoke |on_hover_|.
  base::OneShotTimer invoke_hover_;

  // If true, the tooltip/hover timer will be skipped and |on_hover_| will be
  // run immediately.
  bool immediately_hover_for_test_ = false;

  EasyUnlockIconState icon_state_ = EasyUnlockIconState::LOCKED;
};

class LoginPasswordView::DisplayPasswordButton
    : public views::ToggleImageButton {
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

    const ui::ImageModel invisible_icon =
        ui::ImageModel::FromVectorIcon(kLockScreenPasswordInvisibleIcon,
                                       kColorAshIconColorPrimary, kIconSizeDp);
    const ui::ImageModel visible_icon = ui::ImageModel::FromVectorIcon(
        kLockScreenPasswordVisibleIcon, kColorAshIconColorPrimary, kIconSizeDp);
    const ui::ImageModel visible_icon_disabled = ui::ImageModel::FromVectorIcon(
        kLockScreenPasswordVisibleIcon, kColorAshIconPrimaryDisabledColor,
        kIconSizeDp);
    SetImageModel(views::Button::STATE_NORMAL, visible_icon);
    SetImageModel(views::Button::STATE_DISABLED, visible_icon_disabled);
    SetToggledImageModel(views::Button::STATE_NORMAL, invisible_icon);

    SetEnabled(false);
  }

  DisplayPasswordButton(const DisplayPasswordButton&) = delete;
  DisplayPasswordButton& operator=(const DisplayPasswordButton&) = delete;
  ~DisplayPasswordButton() override = default;
};

// A container view that either shows the easy unlock icon or the caps lock
// icon. When both should be displayed, it starts an animation transitioning
// from one to the other alternatively.
class LoginPasswordView::AlternateIconsView : public views::View {
 public:
  AlternateIconsView() = default;
  AlternateIconsView(const AlternateIconsView&) = delete;
  AlternateIconsView& operator=(const AlternateIconsView&) = delete;
  ~AlternateIconsView() override = default;

  void ScheduleAnimation(views::View* shown_now, views::View* shown_after) {
    if (!layer()) {
      SetPaintToLayer();
      layer()->SetFillsBoundsOpaquely(false);
    }

    shown_now->SetVisible(true);
    shown_after->SetVisible(false);

    // Do not alternate between icons if ChromeVox is enabled.
    if (Shell::Get()->accessibility_controller()->spoken_feedback().enabled()) {
      return;
    }

    std::unique_ptr<ui::LayerAnimationSequence> opacity_sequence =
        std::make_unique<ui::LayerAnimationSequence>();
    observer_ = std::make_unique<AnimationWillRepeatObserver>();
    observer_->SetDisplayOrder(shown_now, shown_after);
    // The observer will be removed automatically from the sequence whenever
    // the layer animator or the observer is destroyed.
    opacity_sequence->AddObserver(observer_.get());
    opacity_sequence->set_is_repeating(true);
    for (size_t i = 0; i < std::size(kAlternateFramesParams); ++i) {
      opacity_sequence->AddElement(
          ui::LayerAnimationElement::CreateOpacityElement(
              kAlternateFramesParams[i].opacity,
              kAlternateFramesParams[i].duration));
    }

    layer()->GetAnimator()->ScheduleAnimation(opacity_sequence.release());
  }

  void AbortAnimationIfAny() {
    if (layer()) {
      layer()->GetAnimator()->AbortAllAnimations();
      layer()->SetOpacity(1.0f);
    }
  }

 private:
  std::unique_ptr<AnimationWillRepeatObserver> observer_;
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

void LoginPasswordView::TestApi::set_immediately_hover_easy_unlock_icon() {
  view_->easy_unlock_icon_->set_immediately_hover_for_test();
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
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::VH(0, kInternalHorizontalPaddingPasswordRowDp),
      kHorizontalSpacingBetweenIconsAndTextfieldDp);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  auto* layout_ptr = password_row_->SetLayoutManager(std::move(layout));

  // Make the password row fill the view.
  password_row_container_layout->SetFlexForView(password_row_, 1);

  left_icon_ =
      password_row_->AddChildView(std::make_unique<AlternateIconsView>());
  left_icon_->SetLayoutManager(std::make_unique<views::FillLayout>());
  left_icon_->SetVisible(false);

  easy_unlock_icon_ =
      left_icon_->AddChildView(std::make_unique<EasyUnlockIcon>());
  easy_unlock_icon_->SetVisible(false);

  capslock_icon_ =
      left_icon_->AddChildView(std::make_unique<views::ImageView>());
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
  submit_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_LOGIN_SUBMIT_BUTTON_ACCESSIBLE_NAME));
  submit_button_->SetEnabled(false);

  // Initialize the capslock icon without a highlight.
  is_capslock_higlight_ = false;

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
    const OnEasyUnlockIconHovered& on_easy_unlock_icon_hovered) {
  DCHECK(on_submit);
  DCHECK(on_password_text_changed);
  on_submit_ = on_submit;
  on_password_text_changed_ = on_password_text_changed;
  easy_unlock_icon_->Init(on_easy_unlock_icon_hovered);
}

void LoginPasswordView::SetEnabledOnEmptyPassword(bool enabled) {
  enabled_on_empty_password_ = enabled;
  UpdateUiState();
}

void LoginPasswordView::SetEasyUnlockIcon(
    EasyUnlockIconState icon_state,
    const std::u16string& accessibility_label) {
  // Do not update EasyUnlockIconState if the Smart Lock revamp is enabled since
  // it will be removed post launch.
  if (base::FeatureList::IsEnabled(ash::features::kSmartLockUIRevamp)) {
    return;
  }

  // Update icon.
  easy_unlock_icon_->SetEasyUnlockIcon(icon_state, accessibility_label);

  // Update icon visibility.
  bool has_icon = icon_state != EasyUnlockIconState::NONE;
  // We do not want to schedule a new animation when the user switches from an
  // account to another.
  if (should_show_easy_unlock_ == has_icon) {
    return;
  }
  should_show_easy_unlock_ = has_icon;
  HandleLeftIconsVisibilities(false /*handling_capslock*/);
}

void LoginPasswordView::OnAccessibleNameChanged(
    const std::u16string& new_name) {
  textfield_->SetAccessibleName(new_name);
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
  view->OnKeyPressed(ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_BACK,
                                  ui::DomCode::BACKSPACE, ui::EF_NONE));
  view->OnKeyPressed(ui::KeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_BACK,
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

  if (key_event.type() != ui::ET_KEY_PRESSED) {
    return false;
  }

  if (key_event.is_repeat()) {
    return false;
  }

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
  if (!enable_buttons && submit_button_->HasFocus()) {
    RequestFocus();
  }
  submit_button_->SetEnabled(enable_buttons);
  display_password_button_->SetEnabled(enable_buttons);
}

void LoginPasswordView::OnCapsLockChanged(bool enabled) {
  should_show_capslock_ = enabled;
  HandleLeftIconsVisibilities(true /*handling_capslock*/);
}

void LoginPasswordView::OnImplicitAnimationsCompleted() {
  Reset();
  SetVisible(false);
  StopObservingImplicitAnimations();
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

  left_icon_->SetVisible(handled_should_show || other_should_show);

  if (handled_should_show) {
    // If the view that is currently handled should be shown, we immediately
    // show it; if the other view should be shown as well, we make it invisible
    // for the moment and start a repeating animation that will show these two
    // views alternatively.
    handled_view->SetVisible(true);
    if (other_should_show) {
      other_view->SetVisible(false);
      left_icon_->ScheduleAnimation(handled_view, other_view);
    }
  } else {
    // If the view that is currently handled should now be invisible, we
    // immediately hide it and we abort the repeating animation if it was
    // running. We also make the other view visible if needed, as its current
    // state may depend on how long the animation has been running.
    left_icon_->AbortAnimationIfAny();
    other_view->SetVisible(other_should_show);
    handled_view->SetVisible(false);
  }
  password_row_->Layout();
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
  is_capslock_higlight_ = highlight;
  capslock_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      kLockScreenCapsLockIcon, highlight ? kColorAshIconColorPrimary
                                         : kColorAshIconPrimaryDisabledColor));
}

BEGIN_METADATA(LoginPasswordView, views::View)
END_METADATA

}  // namespace ash
