// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/dialog_plate/dialog_plate.h"

#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/base/assistant_button.h"
#include "ash/assistant/ui/dialog_plate/mic_view.h"
#include "ash/assistant/util/animation_util.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kIconSizeDip = 24;
constexpr int kButtonSizeDip = 32;
constexpr int kPreferredHeightDip = 48;

// Animation.
constexpr base::TimeDelta kAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(83);
constexpr base::TimeDelta kAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(100);
constexpr base::TimeDelta kAnimationFadeOutDuration =
    base::TimeDelta::FromMilliseconds(83);
constexpr base::TimeDelta kAnimationTransformInDuration =
    base::TimeDelta::FromMilliseconds(333);
constexpr int kAnimationTranslationDip = 30;

}  // namespace

// DialogPlate -----------------------------------------------------------------

DialogPlate::DialogPlate(AssistantViewDelegate* delegate)
    : delegate_(delegate),
      animation_observer_(std::make_unique<ui::CallbackLayerAnimationObserver>(
          /*start_animation_callback=*/base::BindRepeating(
              &DialogPlate::OnAnimationStarted,
              base::Unretained(this)),
          /*end_animation_callback=*/base::BindRepeating(
              &DialogPlate::OnAnimationEnded,
              base::Unretained(this)))),
      query_history_iterator_(
          delegate_->GetInteractionModel()->query_history().GetIterator()) {
  InitLayout();

  // The AssistantViewDelegate should outlive DialogPlate.
  delegate_->AddInteractionModelObserver(this);
  delegate_->AddUiModelObserver(this);
}

DialogPlate::~DialogPlate() {
  delegate_->RemoveUiModelObserver(this);
  delegate_->RemoveInteractionModelObserver(this);
}

const char* DialogPlate::GetClassName() const {
  return "DialogPlate";
}

gfx::Size DialogPlate::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

int DialogPlate::GetHeightForWidth(int width) const {
  return kPreferredHeightDip;
}

void DialogPlate::ButtonPressed(views::Button* sender, const ui::Event& event) {
  OnButtonPressed(static_cast<AssistantButtonId>(sender->GetID()));
}

bool DialogPlate::HandleKeyEvent(views::Textfield* textfield,
                                 const ui::KeyEvent& key_event) {
  if (key_event.type() != ui::EventType::ET_KEY_PRESSED)
    return false;

  switch (key_event.key_code()) {
    case ui::KeyboardCode::VKEY_RETURN: {
      // In tablet mode the virtual keyboard should not be sticky, so we hide it
      // when committing a query.
      if (delegate_->IsTabletMode())
        textfield_->GetFocusManager()->ClearFocus();

      const base::StringPiece16& trimmed_text = base::TrimWhitespace(
          textfield_->GetText(), base::TrimPositions::TRIM_ALL);

      // Only non-empty trimmed text is consider a valid contents commit.
      // Anything else will simply result in the DialogPlate being cleared.
      if (!trimmed_text.empty()) {
        delegate_->OnDialogPlateContentsCommitted(
            base::UTF16ToUTF8(trimmed_text));
      }

      textfield_->SetText(base::string16());

      return true;
    }
    case ui::KeyboardCode::VKEY_UP:
    case ui::KeyboardCode::VKEY_DOWN: {
      DCHECK(query_history_iterator_);
      auto opt_query = key_event.key_code() == ui::KeyboardCode::VKEY_UP
                           ? query_history_iterator_->Prev()
                           : query_history_iterator_->Next();
      textfield_->SetText(base::UTF8ToUTF16(opt_query.value_or("")));
      return true;
    }
    default:
      return false;
  }
}

void DialogPlate::OnInputModalityChanged(InputModality input_modality) {
  using assistant::util::CreateLayerAnimationSequence;
  using assistant::util::CreateOpacityElement;
  using assistant::util::CreateTransformElement;
  using assistant::util::StartLayerAnimationSequencesTogether;

  keyboard_layout_container_->SetVisible(true);
  voice_layout_container_->SetVisible(true);

  switch (input_modality) {
    case InputModality::kKeyboard: {
      // Animate voice layout container opacity to 0%.
      voice_layout_container_->layer()->GetAnimator()->StartAnimation(
          CreateLayerAnimationSequence(
              CreateOpacityElement(0.f, kAnimationFadeOutDuration,
                                   gfx::Tween::Type::FAST_OUT_LINEAR_IN)));

      // Apply a pre-transformation on the keyboard layout container so that it
      // can be animated into place.
      gfx::Transform transform;
      transform.Translate(-kAnimationTranslationDip, 0);
      keyboard_layout_container_->layer()->SetTransform(transform);

      // Animate keyboard layout container.
      StartLayerAnimationSequencesTogether(
          keyboard_layout_container_->layer()->GetAnimator(),
          {// Animate transformation.
           CreateLayerAnimationSequence(CreateTransformElement(
               gfx::Transform(), kAnimationTransformInDuration,
               gfx::Tween::Type::FAST_OUT_SLOW_IN_2)),
           // Animate opacity to 100% with delay.
           CreateLayerAnimationSequence(
               ui::LayerAnimationElement::CreatePauseElement(
                   ui::LayerAnimationElement::AnimatableProperty::OPACITY,
                   kAnimationFadeInDelay),
               CreateOpacityElement(1.f, kAnimationFadeInDuration,
                                    gfx::Tween::Type::FAST_OUT_LINEAR_IN))},
          // Observe this animation.
          animation_observer_.get());

      // Activate the animation observer to receive start/end events.
      animation_observer_->SetActive();
      break;
    }
    case InputModality::kVoice: {
      // Animate keyboard layout container opacity to 0%.
      keyboard_layout_container_->layer()->GetAnimator()->StartAnimation(
          CreateLayerAnimationSequence(
              CreateOpacityElement(0.f, kAnimationFadeOutDuration,
                                   gfx::Tween::Type::FAST_OUT_LINEAR_IN)));

      // Apply a pre-transformation on the voice layout container so that it can
      // be animated into place.
      gfx::Transform transform;
      transform.Translate(kAnimationTranslationDip, 0);
      voice_layout_container_->layer()->SetTransform(transform);

      // Animate voice layout container.
      StartLayerAnimationSequencesTogether(
          voice_layout_container_->layer()->GetAnimator(),
          {// Animate transformation.
           CreateLayerAnimationSequence(CreateTransformElement(
               gfx::Transform(), kAnimationTransformInDuration,
               gfx::Tween::Type::FAST_OUT_SLOW_IN_2)),
           // Animate opacity to 100% with delay.
           CreateLayerAnimationSequence(
               ui::LayerAnimationElement::CreatePauseElement(
                   ui::LayerAnimationElement::AnimatableProperty::OPACITY,
                   kAnimationFadeInDelay),
               CreateOpacityElement(1.f, kAnimationFadeInDuration,
                                    gfx::Tween::Type::FAST_OUT_LINEAR_IN))},
          // Observe this animation.
          animation_observer_.get());

      // Activate the animation observer to receive start/end events.
      animation_observer_->SetActive();
      break;
    }
    case InputModality::kStylus:
      // No action necessary.
      break;
  }
}

void DialogPlate::OnCommittedQueryChanged(
    const AssistantQuery& committed_query) {
  DCHECK(query_history_iterator_);
  query_history_iterator_->ResetToLast();
}

void DialogPlate::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    base::Optional<AssistantEntryPoint> entry_point,
    base::Optional<AssistantExitPoint> exit_point) {
  // When the Assistant UI is no longer visible we need to clear the dialog
  // plate so that text does not persist across Assistant launches.
  if (old_visibility == AssistantVisibility::kVisible)
    textfield_->SetText(base::string16());
}

void DialogPlate::RequestFocus() {
  SetFocus(delegate_->GetInteractionModel()->input_modality());
}

views::View* DialogPlate::FindFirstFocusableView() {
  InputModality input_modality =
      delegate_->GetInteractionModel()->input_modality();

  // The first focusable view depends entirely on current input modality.
  switch (input_modality) {
    case InputModality::kKeyboard:
      return textfield_;
    case InputModality::kVoice:
      return animated_voice_input_toggle_;
    case InputModality::kStylus:
      // Default views::FocusSearch behavior is acceptable.
      return nullptr;
  }
}

void DialogPlate::InitLayout() {
  constexpr int kRightPaddingDip = 8;

  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(0, 0, 0, kRightPaddingDip)));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Input modality layout container.
  input_modality_layout_container_ = new views::View();
  input_modality_layout_container_->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  input_modality_layout_container_->SetPaintToLayer();
  input_modality_layout_container_->layer()->SetFillsBoundsOpaquely(false);
  input_modality_layout_container_->layer()->SetMasksToBounds(true);
  AddChildView(input_modality_layout_container_);

  layout_manager->SetFlexForView(input_modality_layout_container_, 1);

  InitKeyboardLayoutContainer();
  InitVoiceLayoutContainer();

  // Settings.
  settings_button_ = AssistantButton::Create(
      this, kSettingsIcon, kButtonSizeDip, kIconSizeDip,
      IDS_ASH_ASSISTANT_DIALOG_PLATE_SETTINGS_ACCNAME_TOOLTIP,
      AssistantButtonId::kSettings,
      IDS_ASH_ASSISTANT_DIALOG_PLATE_SETTINGS_ACCNAME_TOOLTIP);
  AddChildView(settings_button_);

  // Artificially trigger event to set initial state.
  OnInputModalityChanged(delegate_->GetInteractionModel()->input_modality());
}

void DialogPlate::InitKeyboardLayoutContainer() {
  keyboard_layout_container_ = new views::View();
  keyboard_layout_container_->SetPaintToLayer();
  keyboard_layout_container_->layer()->SetFillsBoundsOpaquely(false);
  keyboard_layout_container_->layer()->SetOpacity(0.f);

  constexpr int kHorizontalPaddingDip = 16;

  views::BoxLayout* layout_manager =
      keyboard_layout_container_->SetLayoutManager(
          std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kHorizontal,
              gfx::Insets(0, kHorizontalPaddingDip)));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  gfx::FontList font_list =
      assistant::ui::GetDefaultFontList().DeriveWithSizeDelta(2);

  // Textfield.
  textfield_ = new views::Textfield();
  textfield_->SetBackgroundColor(SK_ColorTRANSPARENT);
  textfield_->SetBorder(views::NullBorder());
  textfield_->set_controller(this);
  textfield_->SetFontList(font_list);
  textfield_->set_placeholder_font_list(font_list);

  auto textfield_hint =
      l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_DIALOG_PLATE_HINT);
  textfield_->SetPlaceholderText(textfield_hint);
  textfield_->SetAccessibleName(textfield_hint);
  textfield_->set_placeholder_text_color(kTextColorSecondary);
  textfield_->SetTextColor(kTextColorPrimary);
  keyboard_layout_container_->AddChildView(textfield_);

  layout_manager->SetFlexForView(textfield_, 1);

  // Voice input toggle.
  voice_input_toggle_ =
      AssistantButton::Create(this, kMicIcon, kButtonSizeDip, kIconSizeDip,
                              IDS_ASH_ASSISTANT_DIALOG_PLATE_MIC_ACCNAME,
                              AssistantButtonId::kVoiceInputToggle,
                              IDS_ASH_ASSISTANT_DIALOG_PLATE_MIC_TOOLTIP);
  keyboard_layout_container_->AddChildView(voice_input_toggle_);

  input_modality_layout_container_->AddChildView(keyboard_layout_container_);
}

void DialogPlate::InitVoiceLayoutContainer() {
  voice_layout_container_ = new views::View();
  voice_layout_container_->SetPaintToLayer();
  voice_layout_container_->layer()->SetFillsBoundsOpaquely(false);
  voice_layout_container_->layer()->SetOpacity(0.f);

  constexpr int kLeftPaddingDip = 8;
  views::BoxLayout* layout_manager = voice_layout_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(0, kLeftPaddingDip, 0, 0)));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Keyboard input toggle.
  keyboard_input_toggle_ =
      AssistantButton::Create(this, kKeyboardIcon, kButtonSizeDip, kIconSizeDip,
                              IDS_ASH_ASSISTANT_DIALOG_PLATE_KEYBOARD_ACCNAME,
                              AssistantButtonId::kKeyboardInputToggle,
                              IDS_ASH_ASSISTANT_DIALOG_PLATE_KEYBOARD_TOOLTIP);
  voice_layout_container_->AddChildView(keyboard_input_toggle_);

  // Spacer.
  views::View* spacer = new views::View();
  voice_layout_container_->AddChildView(spacer);

  layout_manager->SetFlexForView(spacer, 1);

  // Animated voice input toggle.
  animated_voice_input_toggle_ =
      new MicView(this, delegate_, AssistantButtonId::kVoiceInputToggle);
  animated_voice_input_toggle_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_DIALOG_PLATE_MIC_ACCNAME));
  voice_layout_container_->AddChildView(animated_voice_input_toggle_);

  // Spacer.
  spacer = new views::View();
  voice_layout_container_->AddChildView(spacer);

  layout_manager->SetFlexForView(spacer, 1);

  input_modality_layout_container_->AddChildView(voice_layout_container_);
}

void DialogPlate::OnButtonPressed(AssistantButtonId id) {
  delegate_->OnDialogPlateButtonPressed(id);
  textfield_->SetText(base::string16());
}

void DialogPlate::OnAnimationStarted(
    const ui::CallbackLayerAnimationObserver& observer) {
  keyboard_layout_container_->set_can_process_events_within_subtree(false);
  voice_layout_container_->set_can_process_events_within_subtree(false);
}

bool DialogPlate::OnAnimationEnded(
    const ui::CallbackLayerAnimationObserver& observer) {
  InputModality input_modality =
      delegate_->GetInteractionModel()->input_modality();

  switch (input_modality) {
    case InputModality::kKeyboard:
      keyboard_layout_container_->set_can_process_events_within_subtree(true);
      voice_layout_container_->SetVisible(false);
      break;
    case InputModality::kVoice:
      voice_layout_container_->set_can_process_events_within_subtree(true);
      keyboard_layout_container_->SetVisible(false);
      break;
    case InputModality::kStylus:
      // No action necessary.
      break;
  }

  // Only set focus if Assistant UI is visible. Otherwise we may accidentally
  // steal focus from another window. (See crbug/969983).
  if (delegate_->GetUiModel()->visibility() == AssistantVisibility::kVisible)
    SetFocus(input_modality);

  // We return false so that the animation observer will not destroy itself.
  return false;
}

void DialogPlate::SetFocus(InputModality input_modality) {
  switch (input_modality) {
    case InputModality::kKeyboard:
      textfield_->RequestFocus();
      break;
    case InputModality::kVoice:
    case InputModality::kStylus:
      // When not using |kKeyboard| input modality we need to explicitly clear
      // focus if the focused view is |textfield_| or |voice_input_toggle_| to
      // prevent it from being read by ChromeVox. Clearing focus also allows
      // AssistantContainerView's focus traversal to be reset.
      views::FocusManager* focus_manager = GetFocusManager();
      if (focus_manager &&
          (focus_manager->GetFocusedView() == textfield_ ||
           focus_manager->GetFocusedView() == voice_input_toggle_)) {
        focus_manager->ClearFocus();
      }
      break;
  }
}

}  // namespace ash
