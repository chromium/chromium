// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/edit_label.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/bubble/bubble_utils.h"
#include "ash/shell.h"
#include "ash/style/typography.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/actions/input_element.h"
#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_metrics.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view_list_item.h"
#include "chrome/browser/ash/arc/input_overlay/ui/edit_labels.h"
#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view_utils.h"

namespace arc::input_overlay {

namespace {

constexpr float kCornerRadius = 8.0f;
constexpr int kLabelSize = 32;

constexpr ui::ColorId kPenIconColor = cros_tokens::kCrosSysOnPrimaryContainer;

// Pulse animation specs.
constexpr int kPulseTimes = 3;
constexpr int kPulseExtraHalfSize = 32;
constexpr base::TimeDelta kPulseDuration = base::Seconds(2);

// Returns "<up/down/left/right>" for `direction`.
std::u16string GetAccessibleNameSuffixForDirection(Direction direction) {
  switch (direction) {
    case Direction::kUp:
      return l10n_util ::GetStringUTF16(
          IDS_INPUT_OVERLAY_JOYSTICK_DIRECTION_UP_A11Y_LABEL);
    case Direction::kLeft:
      return l10n_util ::GetStringUTF16(
          IDS_INPUT_OVERLAY_JOYSTICK_DIRECTION_LEFT_A11Y_LABEL);
    case Direction::kDown:
      return l10n_util ::GetStringUTF16(
          IDS_INPUT_OVERLAY_JOYSTICK_DIRECTION_DOWN_A11Y_LABEL);
    case Direction::kRight:
      return l10n_util ::GetStringUTF16(
          IDS_INPUT_OVERLAY_JOYSTICK_DIRECTION_RIGHT_A11Y_LABEL);
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

}  // namespace

EditLabel::EditLabel(DisplayOverlayController* controller,
                     Action* action,
                     bool for_editing_list,
                     size_t index)
    : views::LabelButton(),
      controller_(controller),
      action_(action),
      for_editing_list_(for_editing_list),
      direction_index_(static_cast<Direction>(index)) {
  Init();
}

EditLabel::~EditLabel() = default;

void EditLabel::OnActionInputBindingUpdated() {
  SetLabelContent();
}

bool EditLabel::IsInputUnbound() {
  return GetText().compare(kUnknownBind) == 0 || GetText().empty();
}

void EditLabel::RemoveNewState() {
  SetLabelContent();
}

void EditLabel::PerformPulseAnimation(int pulse_count) {
  // Destroy the pulse layer if it pulses after `kPulseTimes` times.
  if (pulse_count >= kPulseTimes) {
    pulse_layer_.reset();
    return;
  }

  auto* widget = GetWidget();
  DCHECK(widget);

  // Initiate pulse layer if it starts to pulse for the first time.
  if (pulse_count == 0) {
    pulse_layer_ = std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
    widget->GetLayer()->Add(pulse_layer_.get());
    pulse_layer_->SetColor(widget->GetColorProvider()->GetColor(
        cros_tokens::kCrosSysHighlightText));
  }

  DCHECK(pulse_layer_);

  // Initial bounds in its widget coordinate.
  auto view_bounds = ConvertRectToWidget(gfx::Rect(size()));

  // Set initial properties.
  pulse_layer_->SetBounds(view_bounds);
  pulse_layer_->SetOpacity(1.0f);
  pulse_layer_->SetRoundedCornerRadius(gfx::RoundedCornersF(kCornerRadius));

  // Animate from a square to a circle with larger target bounds and to a
  // smaller opacity.
  view_bounds.Outset(kPulseExtraHalfSize);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindOnce(&EditLabel::PerformPulseAnimation,
                              base::Unretained(this), pulse_count + 1))
      .Once()
      .SetDuration(kPulseDuration)
      .SetBounds(pulse_layer_.get(), view_bounds,
                 gfx::Tween::ACCEL_0_40_DECEL_100)
      .SetOpacity(pulse_layer_.get(), /*opacity=*/0.0f,
                  gfx::Tween::ACCEL_0_80_DECEL_80)
      .SetRoundedCorners(
          pulse_layer_.get(),
          gfx::RoundedCornersF(kPulseExtraHalfSize + kLabelSize / 2.0f),
          gfx::Tween::ACCEL_0_40_DECEL_100);
}

void EditLabel::Init() {
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetPreferredSize(gfx::Size(kLabelSize, kLabelSize));
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetInstallFocusRingOnFocus(false);
  SetRequestFocusOnPress(true);
  SetAnimateOnStateChange(false);
  SetHotTracked(false);
  SetShowInkDropWhenHotTracked(false);
  SetHasInkDropActionOnClick(false);
  ash::bubble_utils::ApplyStyle(label(), ash::TypographyToken::kCrosHeadline1,
                                cros_tokens::kCrosSysOnPrimaryContainer);
  SetLabelContent();
}

void EditLabel::SetLabelContent() {
  DCHECK(!action_->IsDeleted());
  const auto& keys = action_->GetCurrentDisplayedInput().keys();
  DCHECK(size_t(direction_index_) < keys.size());
  std::u16string output_string = GetDisplayText(keys[size_t(direction_index_)]);
  if (action_->is_new() && output_string == kUnknownBind) {
    output_string = u"";
  }

  // Clear icon if it is a valid key for new action.
  SetImageModel(views::Button::STATE_NORMAL,
                output_string.empty()
                    ? ui::ImageModel::FromVectorIcon(kGameControlsEditPenIcon,
                                                     kPenIconColor)
                    : ui::ImageModel());
  // Set text label by `output_string` even it is empty to clear the text label.
  SetTextLabel(output_string);
}

void EditLabel::SetTextLabel(const std::u16string& text) {
  SetText(text);
  UpdateAccessibleName();

  SetBackground(views::CreateThemedRoundedRectBackground(
      text == kUnknownBind && !action_->is_new()
          ? cros_tokens::kCrosSysErrorHighlight
          : cros_tokens::kCrosSysHighlightShape,
      kCornerRadius));
  if (HasFocus()) {
    SetToFocused();
  } else {
    SetToDefault();
  }
}

void EditLabel::SetNameTagState(bool is_error,
                                const std::u16string& error_tooltip) {
  DCHECK(parent());
  auto* parent_view = views::AsViewClass<EditLabels>(parent());
  parent_view->SetNameTagState(is_error, error_tooltip);
}

void EditLabel::UpdateAccessibleName() {
  const std::u16string a11y_name =
      GetDisplayTextAccessibleName(label()->GetText());
  const bool unassigned =
      a11y_name.empty() || a11y_name.compare(kUnknownBind) == 0;
  const std::u16string suffix_instruction = l10n_util::GetStringUTF16(
      unassigned
          ? IDS_INPUT_OVERLAY_EDIT_LABEL_KEYBOARD_ASSIGN_INSTRUCTION_A11Y_LABEL
          : IDS_INPUT_OVERLAY_EDIT_LABEL_KEYBOARD_REASSIGN_INSTRUCTION_A11Y_LABEL);

  switch (action_->GetType()) {
    case ActionType::TAP:
      if (unassigned) {
        GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
            IDS_INPUT_OVERLAY_EDIT_LABEL_BUTTON_KEYBOARD_UNASSIGNED_A11Y_TPL,
            suffix_instruction));
      } else {
        GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
            IDS_INPUT_OVERLAY_EDIT_LABEL_BUTTON_KEYBOARD_A11Y_TPL, a11y_name,
            suffix_instruction));
      }
      break;
    case ActionType::MOVE: {
      const std::u16string direction =
          GetAccessibleNameSuffixForDirection(direction_index_);
      if (unassigned) {
        GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
            IDS_INPUT_OVERLAY_EDIT_LABEL_JOYSTICK_KEYBOARD_UNASSIGNED_A11Y_TPL,
            direction, suffix_instruction));
      } else {
        GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
            IDS_INPUT_OVERLAY_EDIT_LABEL_JOYSTICK_KEYBOARD_A11Y_TPL, a11y_name,
            direction, suffix_instruction));
      }
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

void EditLabel::ChangeFocusToNextLabel() {
  DCHECK(parent());
  if (auto* parent_view = views::AsViewClass<EditLabels>(parent())) {
    parent_view->FocusLabel();
  }
}

void EditLabel::SetToDefault() {
  SetEnabledTextColorIds(IsInputUnbound() && !action_->is_new()
                             ? cros_tokens::kCrosSysError
                             : cros_tokens::kCrosSysOnPrimaryContainer);
  SetBorder(nullptr);
}

void EditLabel::SetToFocused() {
  SetEnabledTextColorIds(IsInputUnbound() && !action_->is_new()
                             ? cros_tokens::kCrosSysError
                             : cros_tokens::kCrosSysOnSurface);
  SetBorder(views::CreateThemedRoundedRectBorder(
      /*thickness=*/2, kCornerRadius, cros_tokens::kCrosSysPrimary));
}

void EditLabel::OnFocus() {
  LabelButton::OnFocus();
  if (action_->is_new()) {
    // Hide the pen icon once the label is focused to edit.
    SetImageModel(views::Button::STATE_NORMAL, ui::ImageModel());
  }
  SetToFocused();
  if (for_editing_list_) {
    controller_->AddActionHighlightWidget(action_);
    RecordEditingListFunctionTriggered(controller_->GetPackageName(),
                                       EditingListFunction::kEditLabelFocused);
  } else {
    RecordButtonOptionsMenuFunctionTriggered(
        controller_->GetPackageName(),
        ButtonOptionsMenuFunction::kEditLabelFocused);
  }
}

void EditLabel::OnBlur() {
  LabelButton::OnBlur();
  // `OnBlur()` will be called before removing this view. This view is removed
  // after changing action type and previous `action_` may be invalid. If
  // `action_` is deleted, there is no need to update the content. This view
  // will be removed after this.
  if (!controller_->IsActiveAction(action_)) {
    return;
  }

  if (action_->is_new() && GetText().empty()) {
    SetImageModel(views::Button::STATE_NORMAL,
                  ui::ImageModel::FromVectorIcon(kGameControlsEditPenIcon,
                                                 kPenIconColor));
  }
  SetToDefault();
  // Reset the error state if an reserved key was pressed.
  SetNameTagState(/*is_error=*/false, u"");

  if (!for_editing_list_) {
    return;
  }

  if (auto* list_item = controller_->GetEditingListItemForAction(action_);
      !list_item || !list_item->IsMouseHovered()) {
    controller_->HideActionHighlightWidgetForAction(action_);
  }
}

bool EditLabel::OnKeyPressed(const ui::KeyEvent& event) {
  auto code = event.code();
  std::u16string new_bind = GetDisplayText(code);
  // Don't show error when the same key is pressed.
  if (GetText() == new_bind) {
    SetNameTagState(/*is_error=*/false, u"");
    ChangeFocusToNextLabel();
    return true;
  }

  // Show error when the reserved keys and modifier keys are pressed.
  if ((!action_->support_modifier_key() &&
       ModifierDomCodeToEventFlag(code) != ui::EF_NONE) ||
      IsReservedDomCode(code)) {
    SetNameTagState(
        /*is_error=*/true,
        l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_EDIT_RESERVED_KEYS));
    ash::Shell::Get()
        ->accessibility_controller()
        ->TriggerAccessibilityAlertWithMessage(
            l10n_util::GetStringUTF8(IDS_INPUT_OVERLAY_EDIT_RESERVED_KEYS));
    return false;
  }

  SetTextLabel(new_bind);
  const std::string& package_name = controller_->GetPackageName();
  if (for_editing_list_) {
    RecordEditingListFunctionTriggered(package_name,
                                       EditingListFunction::kKeyAssigned);
  } else {
    RecordButtonOptionsMenuFunctionTriggered(
        package_name, ButtonOptionsMenuFunction::kKeyAssigned);
  }

  std::unique_ptr<InputElement> input;
  switch (action_->GetType()) {
    case ActionType::TAP:
      input = InputElement::CreateActionTapKeyElement(code);
      break;
    case ActionType::MOVE: {
      const auto& input_binding = action_->GetCurrentDisplayedInput();
      auto new_keys = input_binding.keys();
      // If there is duplicated key in its own action, unset the key.
      const int unassigned_index = input_binding.GetIndexOfKey(code);
      if (unassigned_index != -1 &&
          size_t(unassigned_index) != size_t(direction_index_)) {
        new_keys[unassigned_index] = ui::DomCode::NONE;
      }
      // Set the new key.
      new_keys[size_t(direction_index_)] = code;
      input = InputElement::CreateActionMoveKeyElement(new_keys);
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION();
  }
  DCHECK(input);
  controller_->OnInputBindingChange(action_, std::move(input));
  ChangeFocusToNextLabel();
  return true;
}

BEGIN_METADATA(EditLabel)
END_METADATA

}  // namespace arc::input_overlay
