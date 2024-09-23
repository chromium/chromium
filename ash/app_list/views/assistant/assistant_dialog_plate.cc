// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/assistant/assistant_dialog_plate.h"

#include <string_view>
#include <utility>

#include "ash/ash_element_identifiers.h"
#include "ash/assistant/model/assistant_interaction_model.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/base/assistant_button.h"
#include "ash/assistant/ui/dialog_plate/mic_view.h"
#include "ash/assistant/util/animation_util.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

// Appearance.
constexpr int kIconSizeDip = 24;
constexpr int kButtonSizeDip = 32;
constexpr int kPaddingBottomDip = 8;
constexpr int kPaddingHorizontalDip = 16;
constexpr int kPaddingTopDip = 12;

// Animation.
constexpr base::TimeDelta kAnimationFadeInDelay = base::Milliseconds(83);
constexpr base::TimeDelta kAnimationFadeInDuration = base::Milliseconds(100);
constexpr base::TimeDelta kAnimationFadeOutDuration = base::Milliseconds(83);
constexpr base::TimeDelta kAnimationTransformInDuration =
    base::Milliseconds(333);
constexpr int kAnimationTranslationDip = 30;

using keyboard::KeyboardUIController;

// Textfield used for inputting text based Assistant queries.
class AssistantTextfield : public views::Textfield {
  METADATA_HEADER(AssistantTextfield, views::Textfield)

 public:
  AssistantTextfield() { SetID(AssistantViewID::kTextQueryField); }
};

BEGIN_METADATA(AssistantTextfield)
END_METADATA

void ShowKeyboardIfEnabled() {
  auto* keyboard_controller = KeyboardUIController::Get();

  if (keyboard_controller->IsEnabled())
    keyboard_controller->ShowKeyboard(/*lock=*/false);
}

void HideKeyboardIfEnabled() {
  auto* keyboard_controller = KeyboardUIController::Get();

  if (keyboard_controller->IsEnabled())
    keyboard_controller->HideKeyboardImplicitlyByUser();
}

}  // namespace

// AssistantDialogPlate --------------------------------------------------------

AssistantDialogPlate::AssistantDialogPlate(AssistantViewDelegate* delegate)
    : delegate_(delegate),
      animation_observer_(std::make_unique<ui::CallbackLayerAnimationObserver>(
          /*start_animation_callback=*/base::BindRepeating(
              &AssistantDialogPlate::OnAnimationStarted,
              base::Unretained(this)),
          /*end_animation_callback=*/base::BindRepeating(
              &AssistantDialogPlate::OnAnimationEnded,
              base::Unretained(this)))),
      query_history_iterator_(AssistantInteractionController::Get()
                                  ->GetModel()
                                  ->query_history()
                                  .GetIterator()) {
  SetID(AssistantViewID::kDialogPlate);
  SetProperty(views::kElementIdentifierKey, kAssistantDialogPlateElementId);
  InitLayout();

  assistant_controller_observation_.Observe(AssistantController::Get());
  AssistantInteractionController::Get()->GetModel()->AddObserver(this);
  AssistantUiController::Get()->GetModel()->AddObserver(this);
}

AssistantDialogPlate::~AssistantDialogPlate() {
  if (AssistantUiController::Get())
    AssistantUiController::Get()->GetModel()->RemoveObserver(this);

  if (AssistantInteractionController::Get())
    AssistantInteractionController::Get()->GetModel()->RemoveObserver(this);
}

gfx::Size AssistantDialogPlate::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(
      INT_MAX, GetLayoutManager()->GetPreferredHeightForWidth(this, INT_MAX));
}

void AssistantDialogPlate::OnButtonPressed(AssistantButtonId button_id) {
  delegate_->OnDialogPlateButtonPressed(button_id);
  textfield_->SetText(std::u16string());
}

bool AssistantDialogPlate::HandleKeyEvent(views::Textfield* textfield,
                                          const ui::KeyEvent& key_event) {
  if (key_event.type() != ui::EventType::kKeyPressed) {
    return false;
  }

  switch (key_event.key_code()) {
    case ui::KeyboardCode::VKEY_RETURN: {
      // In tablet mode the virtual keyboard should not be sticky, so we hide it
      // when committing a query.
      if (delegate_->IsTabletMode())
        HideKeyboardIfEnabled();

      std::u16string_view trimmed_text = base::TrimWhitespace(
          textfield_->GetText(), base::TrimPositions::TRIM_ALL);

      // Only non-empty trimmed text is consider a valid contents commit.
      // Anything else will simply result in the AssistantDialogPlate being
      // cleared.
      if (!trimmed_text.empty()) {
        delegate_->OnDialogPlateContentsCommitted(
            base::UTF16ToUTF8(trimmed_text));
      }

      textfield_->SetText(std::u16string());

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

void AssistantDialogPlate::OnAssistantControllerDestroying() {
  AssistantUiController::Get()->GetModel()->RemoveObserver(this);
  AssistantInteractionController::Get()->GetModel()->RemoveObserver(this);
  DCHECK(assistant_controller_observation_.IsObservingSource(
      AssistantController::Get()));
  assistant_controller_observation_.Reset();
}

void AssistantDialogPlate::OnInputModalityChanged(
    InputModality input_modality) {
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
  }
}

void AssistantDialogPlate::OnCommittedQueryChanged(
    const AssistantQuery& committed_query) {
  // Whenever a query is submitted we return the focus to the dialog plate.
  RequestFocus();

  DCHECK(query_history_iterator_);
  query_history_iterator_->ResetToLast();
}

void AssistantDialogPlate::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    std::optional<AssistantEntryPoint> entry_point,
    std::optional<AssistantExitPoint> exit_point) {
  switch (new_visibility) {
    case AssistantVisibility::kVisible:
      UpdateModalityVisibility();
      UpdateKeyboardVisibility();
      break;
    case AssistantVisibility::kClosed:
      // When the Assistant UI is no longer visible we need to clear the dialog
      // plate so that text does not persist across Assistant launches.
      textfield_->SetText(std::u16string());
      HideKeyboardIfEnabled();
      break;
    case AssistantVisibility::kClosing:
      // No action.
      break;
  }
}

void AssistantDialogPlate::RequestFocus() {
  views::View* view = FindFirstFocusableView();
  if (view)
    view->RequestFocus();
}

void AssistantDialogPlate::OnThemeChanged() {
  views::View::OnThemeChanged();

  textfield_->SetTextColor(
      GetColorProvider()->GetColor(cros_tokens::kColorPrimary));
  textfield_->set_placeholder_text_color(
      GetColorProvider()->GetColor(cros_tokens::kColorSecondary));
}

views::View* AssistantDialogPlate::FindFirstFocusableView() {
  // The first focusable view depends entirely on current input modality.
  switch (input_modality()) {
    case InputModality::kKeyboard:
      return textfield_;
    case InputModality::kVoice:
      return animated_voice_input_toggle_;
  }
}

void AssistantDialogPlate::InitLayout() {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets::TLBR(kPaddingTopDip, kPaddingHorizontalDip,
                            kPaddingBottomDip, kPaddingHorizontalDip)));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Molecule icon.
  molecule_icon_ = AddChildView(std::make_unique<views::ImageView>());
  molecule_icon_->SetID(AssistantViewID::kModuleIcon);
  molecule_icon_->SetPreferredSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  molecule_icon_->SetImage(gfx::CreateVectorIcon(
      chromeos::kAssistantIcon, kIconSizeDip, gfx::kPlaceholderColor));

  // Input modality layout container.
  input_modality_layout_container_ =
      AddChildView(std::make_unique<views::View>());
  input_modality_layout_container_->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  input_modality_layout_container_->SetPaintToLayer();
  input_modality_layout_container_->layer()->SetFillsBoundsOpaquely(false);
  input_modality_layout_container_->layer()->SetMasksToBounds(true);

  layout_manager->SetFlexForView(input_modality_layout_container_, 1);

  InitKeyboardLayoutContainer();
  InitVoiceLayoutContainer();

  // Set initial state.
  UpdateModalityVisibility();
}

void AssistantDialogPlate::InitKeyboardLayoutContainer() {
  auto keyboard_layout_container = std::make_unique<views::View>();
  keyboard_layout_container->SetPaintToLayer();
  keyboard_layout_container->layer()->SetFillsBoundsOpaquely(false);
  keyboard_layout_container->layer()->SetOpacity(0.f);

  constexpr int kLeftPaddingDip = 16;
  views::BoxLayout* layout_manager =
      keyboard_layout_container->SetLayoutManager(
          std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kHorizontal,
              gfx::Insets::TLBR(0, kLeftPaddingDip, 0, 0)));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  gfx::FontList font_list =
      assistant::ui::GetDefaultFontList().DeriveWithSizeDelta(2);

  // Textfield.
  auto textfield = std::make_unique<AssistantTextfield>();
  textfield->SetBackgroundColor(SK_ColorTRANSPARENT);
  textfield->SetBorder(views::NullBorder());
  textfield->set_controller(this);
  textfield->SetFontList(font_list);
  textfield->set_placeholder_font_list(font_list);

  auto textfield_hint =
      l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_DIALOG_PLATE_HINT);
  textfield->SetPlaceholderText(textfield_hint);
  textfield->GetViewAccessibility().SetName(textfield_hint);
  textfield_ = keyboard_layout_container->AddChildView(std::move(textfield));

  layout_manager->SetFlexForView(textfield_, 1);

  // Voice input toggle.
  AssistantButton::InitParams params;
  params.size_in_dip = kButtonSizeDip;
  params.icon_size_in_dip = kIconSizeDip;
  params.accessible_name_id = IDS_ASH_ASSISTANT_DIALOG_PLATE_MIC_ACCNAME;
  params.tooltip_id = IDS_ASH_ASSISTANT_DIALOG_PLATE_MIC_TOOLTIP;
  std::unique_ptr<AssistantButton> voice_input_toggle = AssistantButton::Create(
      this, kMicIcon, AssistantButtonId::kVoiceInputToggle, std::move(params));
  voice_input_toggle->SetID(AssistantViewID::kVoiceInputToggle);
  voice_input_toggle_ =
      keyboard_layout_container->AddChildView(std::move(voice_input_toggle));

  keyboard_layout_container_ = input_modality_layout_container_->AddChildView(
      std::move(keyboard_layout_container));
}

void AssistantDialogPlate::InitVoiceLayoutContainer() {
  auto voice_layout_container = std::make_unique<views::View>();

  // TODO(crbug.com/40232718): See View::SetLayoutManagerUseConstrainedSpace.
  voice_layout_container->SetLayoutManagerUseConstrainedSpace(false);
  voice_layout_container->SetPaintToLayer();
  voice_layout_container->layer()->SetFillsBoundsOpaquely(false);
  voice_layout_container->layer()->SetOpacity(0.f);

  views::BoxLayout* layout_manager = voice_layout_container->SetLayoutManager(
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
  auto offset = std::make_unique<views::View>();
  offset->SetPreferredSize(gfx::Size(difference, 1));
  voice_layout_container->AddChildView(std::move(offset));

  // Spacer.
  auto spacer = std::make_unique<views::View>();
  layout_manager->SetFlexForView(
      voice_layout_container->AddChildView(std::move(spacer)), 1);

  // Animated voice input toggle.
  auto animated_voice_input_toggle =
      std::make_unique<MicView>(this, AssistantButtonId::kVoiceInputToggle);
  animated_voice_input_toggle->SetID(AssistantViewID::kMicView);
  animated_voice_input_toggle->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_DIALOG_PLATE_MIC_ACCNAME));
  animated_voice_input_toggle_ = voice_layout_container->AddChildView(
      std::move(animated_voice_input_toggle));

  // Spacer.
  layout_manager->SetFlexForView(
      voice_layout_container->AddChildView(std::make_unique<views::View>()), 1);

  // Keyboard input toggle.
  AssistantButton::InitParams params;
  params.size_in_dip = kButtonSizeDip;
  params.icon_size_in_dip = kIconSizeDip;
  params.icon_color_type = cros_tokens::kColorPrimary;
  params.accessible_name_id = IDS_ASH_ASSISTANT_DIALOG_PLATE_KEYBOARD_ACCNAME;
  params.tooltip_id = IDS_ASH_ASSISTANT_DIALOG_PLATE_KEYBOARD_TOOLTIP;
  keyboard_input_toggle_ =
      voice_layout_container->AddChildView(AssistantButton::Create(
          this, vector_icons::kKeyboardIcon,
          AssistantButtonId::kKeyboardInputToggle,
          std::move(params)));
  keyboard_input_toggle_->SetID(AssistantViewID::kKeyboardInputToggle);

  voice_layout_container_ = input_modality_layout_container_->AddChildView(
      std::move(voice_layout_container));
}

void AssistantDialogPlate::UpdateModalityVisibility() {
  // Hide everything.
  keyboard_layout_container_->SetVisible(false);
  voice_layout_container_->SetVisible(false);
  // Reset opacity.
  keyboard_layout_container_->layer()->SetOpacity(1);
  voice_layout_container_->layer()->SetOpacity(1);
  // Show currently selected content.
  switch (input_modality()) {
    case InputModality::kKeyboard:
      keyboard_layout_container_->SetVisible(true);
      break;
    case InputModality::kVoice:
      voice_layout_container_->SetVisible(true);
      break;
  }
}

void AssistantDialogPlate::UpdateKeyboardVisibility() {
  if (!delegate_->IsTabletMode())
    return;

  bool should_show_keyboard = (input_modality() == InputModality::kKeyboard);

  if (should_show_keyboard)
    ShowKeyboardIfEnabled();
  else
    HideKeyboardIfEnabled();
}

void AssistantDialogPlate::OnAnimationStarted(
    const ui::CallbackLayerAnimationObserver& observer) {
  keyboard_layout_container_->SetCanProcessEventsWithinSubtree(false);
  voice_layout_container_->SetCanProcessEventsWithinSubtree(false);
}

bool AssistantDialogPlate::OnAnimationEnded(
    const ui::CallbackLayerAnimationObserver& observer) {
  keyboard_layout_container_->SetCanProcessEventsWithinSubtree(true);
  voice_layout_container_->SetCanProcessEventsWithinSubtree(true);

  UpdateModalityVisibility();
  RequestFocus();
  UpdateKeyboardVisibility();

  // We return false so that the animation observer will not destroy itself.
  return false;
}

InputModality AssistantDialogPlate::input_modality() const {
  return AssistantInteractionController::Get()->GetModel()->input_modality();
}

BEGIN_METADATA(AssistantDialogPlate)
END_METADATA

}  // namespace ash
