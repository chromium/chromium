// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/assistant/assistant_dialog_plate.h"

#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/base/assistant_button.h"
#include "ash/assistant/ui/dialog_plate/dialog_plate.h"
#include "ash/assistant/ui/dialog_plate/mic_view.h"
#include "ash/assistant/ui/logo_view/logo_view.h"
#include "ash/assistant/util/animation_util.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
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
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kIconSizeDip = 24;
constexpr int kButtonSizeDip = 32;
constexpr int kPaddingBottomDip = 8;
constexpr int kPaddingHorizontalDip = 16;
constexpr int kPaddingTopDip = 12;

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

// Textfield used for inputting text based Assistant queries.
class AssistantTextfield : public views::Textfield {
 public:
  AssistantTextfield() : views::Textfield() {
    SetID(AssistantViewID::kTextQueryField);
  }

  // views::Textfield overrides:
  const char* GetClassName() const override { return "AssistantTextfield"; }
};

}  // namespace

// AssistantDialogPlate --------------------------------------------------------

AssistantDialogPlate::AssistantDialogPlate(ash::AssistantViewDelegate* delegate)
    : delegate_(delegate),
      animation_observer_(std::make_unique<ui::CallbackLayerAnimationObserver>(
          /*start_animation_callback=*/base::BindRepeating(
              &AssistantDialogPlate::OnAnimationStarted,
              base::Unretained(this)),
          /*end_animation_callback=*/base::BindRepeating(
              &AssistantDialogPlate::OnAnimationEnded,
              base::Unretained(this)))),
      query_history_iterator_(
          delegate_->GetInteractionModel()->query_history().GetIterator()) {
  SetID(AssistantViewID::kDialogPlate);
  InitLayout();

  // The AssistantViewDelegate should outlive AssistantDialogPlate.
  delegate_->AddInteractionModelObserver(this);
  delegate_->AddUiModelObserver(this);
}

AssistantDialogPlate::~AssistantDialogPlate() {
  delegate_->RemoveUiModelObserver(this);
  delegate_->RemoveInteractionModelObserver(this);
}

const char* AssistantDialogPlate::GetClassName() const {
  return "AssistantDialogPlate";
}

gfx::Size AssistantDialogPlate::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

void AssistantDialogPlate::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  OnButtonPressed(static_cast<ash::AssistantButtonId>(sender->GetID()));
}

bool AssistantDialogPlate::HandleKeyEvent(views::Textfield* textfield,
                                          const ui::KeyEvent& key_event) {
  if (key_event.type() != ui::EventType::ET_KEY_PRESSED)
    return false;

  switch (key_event.key_code()) {
    case ui::KeyboardCode::VKEY_RETURN: {
      // In tablet mode the virtual keyboard should not be sticky, so we hide it
      // when committing a query.
      if (delegate_->IsTabletMode())
        keyboard::KeyboardUIController::Get()->HideKeyboardImplicitlyBySystem();

      const base::StringPiece16& trimmed_text = base::TrimWhitespace(
          textfield_->GetText(), base::TrimPositions::TRIM_ALL);

      // Only non-empty trimmed text is consider a valid contents commit.
      // Anything else will simply result in the AssistantDialogPlate being
      // cleared.
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

void AssistantDialogPlate::OnInputModalityChanged(
    ash::InputModality input_modality) {
  using ash::assistant::util::CreateLayerAnimationSequence;
  using ash::assistant::util::CreateOpacityElement;
  using ash::assistant::util::CreateTransformElement;
  using ash::assistant::util::StartLayerAnimationSequencesTogether;

  keyboard_layout_container_->SetVisible(true);
  voice_layout_container_->SetVisible(true);

  switch (input_modality) {
    case ash::InputModality::kKeyboard: {
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
    case ash::InputModality::kVoice: {
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
    case ash::InputModality::kStylus:
      // No action necessary.
      break;
  }
}

void AssistantDialogPlate::OnCommittedQueryChanged(
    const ash::AssistantQuery& committed_query) {
  DCHECK(query_history_iterator_);
  query_history_iterator_->ResetToLast();
}

void AssistantDialogPlate::OnUiVisibilityChanged(
    ash::AssistantVisibility new_visibility,
    ash::AssistantVisibility old_visibility,
    base::Optional<ash::AssistantEntryPoint> entry_point,
    base::Optional<ash::AssistantExitPoint> exit_point) {
  // When the Assistant UI is no longer visible we need to clear the dialog
  // plate so that text does not persist across Assistant launches.
  if (old_visibility == ash::AssistantVisibility::kVisible)
    textfield_->SetText(base::string16());
}

void AssistantDialogPlate::RequestFocus() {
  SetFocus(delegate_->GetInteractionModel()->input_modality());
}

views::View* AssistantDialogPlate::FindFirstFocusableView() {
  ash::InputModality input_modality =
      delegate_->GetInteractionModel()->input_modality();

  // The first focusable view depends entirely on current input modality.
  switch (input_modality) {
    case ash::InputModality::kKeyboard:
      return textfield_;
    case ash::InputModality::kVoice:
      return animated_voice_input_toggle_;
    case ash::InputModality::kStylus:
      // Default views::FocusSearch behavior is acceptable.
      return nullptr;
  }
}

void AssistantDialogPlate::InitLayout() {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(kPaddingTopDip, kPaddingHorizontalDip, kPaddingBottomDip,
                      kPaddingHorizontalDip)));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Molecule icon.
  molecule_icon_ = ash::LogoView::Create();
  molecule_icon_->SetID(AssistantViewID::kModuleIcon);
  molecule_icon_->SetPreferredSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  molecule_icon_->SetState(ash::LogoView::State::kMoleculeWavy,
                           /*animate=*/false);
  AddChildView(molecule_icon_);

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

  // Artificially trigger event to set initial state.
  OnInputModalityChanged(delegate_->GetInteractionModel()->input_modality());
}

void AssistantDialogPlate::InitKeyboardLayoutContainer() {
  keyboard_layout_container_ = new views::View();
  keyboard_layout_container_->SetPaintToLayer();
  keyboard_layout_container_->layer()->SetFillsBoundsOpaquely(false);
  keyboard_layout_container_->layer()->SetOpacity(0.f);

  constexpr int kLeftPaddingDip = 16;
  views::BoxLayout* layout_manager =
      keyboard_layout_container_->SetLayoutManager(
          std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kHorizontal,
              gfx::Insets(0, kLeftPaddingDip, 0, 0)));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  gfx::FontList font_list =
      ash::assistant::ui::GetDefaultFontList().DeriveWithSizeDelta(2);

  // Textfield.
  textfield_ = new AssistantTextfield();
  textfield_->SetBackgroundColor(SK_ColorTRANSPARENT);
  textfield_->SetBorder(views::NullBorder());
  textfield_->set_controller(this);
  textfield_->SetFontList(font_list);
  textfield_->set_placeholder_font_list(font_list);

  auto textfield_hint =
      l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_DIALOG_PLATE_HINT);
  textfield_->SetPlaceholderText(textfield_hint);
  textfield_->SetAccessibleName(textfield_hint);
  textfield_->set_placeholder_text_color(ash::kTextColorSecondary);
  textfield_->SetTextColor(ash::kTextColorPrimary);
  keyboard_layout_container_->AddChildView(textfield_);

  layout_manager->SetFlexForView(textfield_, 1);

  // Voice input toggle.
  voice_input_toggle_ = ash::AssistantButton::Create(
      this, ash::kMicIcon, kButtonSizeDip, kIconSizeDip,
      IDS_ASH_ASSISTANT_DIALOG_PLATE_MIC_ACCNAME,
      ash::AssistantButtonId::kVoiceInputToggle,
      IDS_ASH_ASSISTANT_DIALOG_PLATE_MIC_TOOLTIP);
  voice_input_toggle_->SetID(AssistantViewID::kVoiceInputToggle);
  keyboard_layout_container_->AddChildView(voice_input_toggle_);

  input_modality_layout_container_->AddChildView(keyboard_layout_container_);
}

void AssistantDialogPlate::InitVoiceLayoutContainer() {
  voice_layout_container_ = new views::View();
  voice_layout_container_->SetPaintToLayer();
  voice_layout_container_->layer()->SetFillsBoundsOpaquely(false);
  voice_layout_container_->layer()->SetOpacity(0.f);

  views::BoxLayout* layout_manager = voice_layout_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Offset.
  // To make the |animated_voice_input_toggle_| horizontally centered in the
  // dialog plate we need to offset by the difference in width between the
  // |molecule_icon_| and the |keyboard_input_toggle_|.
  constexpr int difference =
      /*keyboard_input_toggle_width=*/kButtonSizeDip -
      /*molecule_icon_width=*/kIconSizeDip;
  views::View* offset = new views::View();
  offset->SetPreferredSize(gfx::Size(difference, 1));
  voice_layout_container_->AddChildView(offset);

  // Spacer.
  views::View* spacer = new views::View();
  voice_layout_container_->AddChildView(spacer);
  layout_manager->SetFlexForView(spacer, 1);

  // Animated voice input toggle.
  animated_voice_input_toggle_ = new ash::MicView(
      this, delegate_, ash::AssistantButtonId::kVoiceInputToggle);
  animated_voice_input_toggle_->SetID(AssistantViewID::kMicView);
  animated_voice_input_toggle_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_DIALOG_PLATE_MIC_ACCNAME));
  voice_layout_container_->AddChildView(animated_voice_input_toggle_);

  // Spacer.
  spacer = new views::View();
  voice_layout_container_->AddChildView(spacer);
  layout_manager->SetFlexForView(spacer, 1);

  // Keyboard input toggle.
  keyboard_input_toggle_ = ash::AssistantButton::Create(
      this, ash::kKeyboardIcon, kButtonSizeDip, kIconSizeDip,
      IDS_ASH_ASSISTANT_DIALOG_PLATE_KEYBOARD_ACCNAME,
      ash::AssistantButtonId::kKeyboardInputToggle,
      IDS_ASH_ASSISTANT_DIALOG_PLATE_KEYBOARD_TOOLTIP);
  keyboard_input_toggle_->SetID(AssistantViewID::kKeyboardInputToggle);
  voice_layout_container_->AddChildView(keyboard_input_toggle_);

  input_modality_layout_container_->AddChildView(voice_layout_container_);
}

void AssistantDialogPlate::OnButtonPressed(ash::AssistantButtonId id) {
  delegate_->OnDialogPlateButtonPressed(id);
  textfield_->SetText(base::string16());
}

void AssistantDialogPlate::OnAnimationStarted(
    const ui::CallbackLayerAnimationObserver& observer) {
  keyboard_layout_container_->set_can_process_events_within_subtree(false);
  voice_layout_container_->set_can_process_events_within_subtree(false);
}

bool AssistantDialogPlate::OnAnimationEnded(
    const ui::CallbackLayerAnimationObserver& observer) {
  ash::InputModality input_modality =
      delegate_->GetInteractionModel()->input_modality();

  switch (input_modality) {
    case ash::InputModality::kKeyboard:
      keyboard_layout_container_->set_can_process_events_within_subtree(true);
      voice_layout_container_->SetVisible(false);
      break;
    case ash::InputModality::kVoice:
      voice_layout_container_->set_can_process_events_within_subtree(true);
      keyboard_layout_container_->SetVisible(false);
      break;
    case ash::InputModality::kStylus:
      // No action necessary.
      break;
  }

  SetFocus(input_modality);

  // We return false so that the animation observer will not destroy itself.
  return false;
}

void AssistantDialogPlate::SetFocus(ash::InputModality input_modality) {
  switch (input_modality) {
    case ash::InputModality::kKeyboard:
      textfield_->RequestFocus();
      break;
    case ash::InputModality::kVoice:
      animated_voice_input_toggle_->RequestFocus();
      break;
    case ash::InputModality::kStylus:
      break;
  }
}

}  // namespace ash
