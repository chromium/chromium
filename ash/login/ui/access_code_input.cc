// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/access_code_input.h"

#include <string>

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/system_textfield.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/range/range.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {
// Identifier for focus traversal.
constexpr int kFixedLengthInputGroup = 1;

constexpr int kAccessCodeFlexLengthWidthDp = 192;
constexpr int kAccessCodeFlexUnderlineThicknessDp = 1;
constexpr int kAccessCodeFontSizeDeltaDp = 4;
constexpr int kObscuredGlyphSpacingDp = 6;

constexpr int kAccessCodeInputFieldWidthDp = 24;
constexpr int kAccessCodeBetweenInputFieldsGapDp = 8;
}  // namespace

BEGIN_METADATA(AccessCodeInput)
END_METADATA

FlexCodeInput::FlexCodeInput(OnInputChange on_input_change,
                             OnEnter on_enter,
                             OnEscape on_escape,
                             bool obscure_pin)
    : on_input_change_(std::move(on_input_change)),
      on_enter_(std::move(on_enter)),
      on_escape_(std::move(on_escape)) {
  DCHECK(on_input_change_);
  DCHECK(on_enter_);
  DCHECK(on_escape_);

  SetLayoutManager(std::make_unique<views::FillLayout>());

  const ui::ColorId input_color_id =
      chromeos::features::IsJellyrollEnabled()
          ? static_cast<ui::ColorId>(cros_tokens::kCrosSysOnSurface)
          : kColorAshTextColorPrimary;
  code_field_ = AddChildView(
      std::make_unique<SystemTextfield>(SystemTextfield::Type::kMedium));
  code_field_->set_controller(this);
  code_field_->SetFontList(views::Textfield::GetDefaultFontList().Derive(
      kAccessCodeFontSizeDeltaDp, gfx::Font::FontStyle::NORMAL,
      gfx::Font::Weight::NORMAL));
  code_field_->SetBorder(views::CreateSolidSidedBorder(
      gfx::Insets::TLBR(0, 0, kAccessCodeFlexUnderlineThicknessDp, 0),
      input_color_id));
  code_field_->SetBackgroundEnabled(false);
  code_field_->SetFocusBehavior(FocusBehavior::ALWAYS);
  code_field_->SetPreferredSize(
      gfx::Size(kAccessCodeFlexLengthWidthDp, kAccessCodeInputFieldHeightDp));
  SetInputColorId(input_color_id);

  if (obscure_pin) {
    code_field_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
    code_field_->SetObscuredGlyphSpacing(kObscuredGlyphSpacingDp);
  } else {
    code_field_->SetTextInputType(ui::TEXT_INPUT_TYPE_NUMBER);
  }
}

FlexCodeInput::~FlexCodeInput() = default;

void FlexCodeInput::InsertDigit(int value) {
  DCHECK_LE(0, value);
  DCHECK_GE(9, value);
  if (code_field_->GetEnabled()) {
    code_field_->SetText(code_field_->GetText() +
                         base::NumberToString16(value));
    on_input_change_.Run(true);
  }
}

void FlexCodeInput::Backspace() {
  // Instead of just adjusting code_field_ text directly, fire a backspace key
  // event as this handles the various edge cases (ie, selected text).

  // views::Textfield::OnKeyPressed is private, so we call it via views::View.
  auto* view = static_cast<views::View*>(code_field_);
  view->OnKeyPressed(ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_BACK,
                                  ui::DomCode::BACKSPACE, ui::EF_NONE));
  view->OnKeyPressed(ui::KeyEvent(ui::EventType::kKeyReleased, ui::VKEY_BACK,
                                  ui::DomCode::BACKSPACE, ui::EF_NONE));
  // This triggers ContentsChanged(), which calls |on_input_change_|.
}

std::optional<std::string> FlexCodeInput::GetCode() const {
  std::u16string code = code_field_->GetText();
  if (!code.length()) {
    return std::nullopt;
  }
  return base::UTF16ToUTF8(code);
}

void FlexCodeInput::SetInputColorId(ui::ColorId color_id) {
  code_field_->SetTextColorId(color_id);
}

void FlexCodeInput::SetInputEnabled(bool input_enabled) {
  code_field_->SetEnabled(input_enabled);
}

void FlexCodeInput::SetReadOnly(bool read_only) {
  NOTIMPLEMENTED();
}

bool FlexCodeInput::IsReadOnly() const {
  NOTIMPLEMENTED();
  return false;
}

void FlexCodeInput::ClearInput() {
  code_field_->SetText(std::u16string());
  on_input_change_.Run(false);
}

void FlexCodeInput::RequestFocus() {
  code_field_->RequestFocus();
}

void FlexCodeInput::SetAccessibleNameOnTextfield(const std::u16string& name) {
  code_field_->GetViewAccessibility().SetName(name);
}

void FlexCodeInput::ContentsChanged(views::Textfield* sender,
                                    const std::u16string& new_contents) {
  const bool has_content = new_contents.length() > 0;
  on_input_change_.Run(has_content);
}

bool FlexCodeInput::HandleKeyEvent(views::Textfield* sender,
                                   const ui::KeyEvent& key_event) {
  // Only handle keys.
  if (key_event.type() != ui::EventType::kKeyPressed) {
    return false;
  }

  // Default handling for events with Alt modifier like spoken feedback.
  if (key_event.IsAltDown()) {
    return false;
  }

  // FlexCodeInput class responds to a limited subset of key press events.
  // All events not handled below are sent to |code_field_|.
  const ui::KeyboardCode key_code = key_event.key_code();

  // Allow using tab for keyboard navigation.
  if (key_code == ui::VKEY_TAB || key_code == ui::VKEY_BACKTAB) {
    return false;
  }

  if (key_code == ui::VKEY_RETURN) {
    if (GetCode().has_value()) {
      on_enter_.Run();
    }
    return true;
  }

  if (key_code == ui::VKEY_ESCAPE) {
    on_escape_.Run();
    return true;
  }

  return false;
}

BEGIN_METADATA(FlexCodeInput)
END_METADATA

AccessibleInputField::AccessibleInputField()
    : SystemTextfield(SystemTextfield::Type::kMedium) {}

bool AccessibleInputField::IsGroupFocusTraversable() const {
  return false;
}

views::View* AccessibleInputField::GetSelectedViewForGroup(int group) {
  return parent() ? parent()->GetSelectedViewForGroup(group) : nullptr;
}

void AccessibleInputField::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::EventType::kGestureTap) {
    RequestFocusWithPointer(event->details().primary_pointer_type());
    return;
  }

  views::Textfield::OnGestureEvent(event);
}

void AccessibleInputField::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // Focusable nodes generally must have a name, but the focus of an accessible
  // input field is propagated to its ancestor.
  views::Textfield::GetAccessibleNodeData(node_data);

  // We want the PIN input field, an empty input field, to retain
  // NameFrom::kAttributeExplicitlyEmpty. However
  // Textfield::GetAccessibleNodeData() sets NameFrom to NameFrom::kContent.
  // We override NameFrom after this call.
  node_data->SetNameFrom(ax::mojom::NameFrom::kAttributeExplicitlyEmpty);
}

BEGIN_METADATA(AccessibleInputField)
END_METADATA

FixedLengthCodeInput::FixedLengthCodeInput(int length,
                                           OnInputChange on_input_change,
                                           OnEnter on_enter,
                                           OnEscape on_escape,
                                           bool obscure_pin)
    : on_input_change_(std::move(on_input_change)),
      on_enter_(std::move(on_enter)),
      on_escape_(std::move(on_escape)),
      is_obscure_pin_(obscure_pin) {
  DCHECK_LT(0, length);
  DCHECK(on_input_change_);

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kAccessCodeBetweenInputFieldsGapDp));
  SetGroup(kFixedLengthInputGroup);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  const ui::ColorId text_color_id =
      chromeos::features::IsJellyrollEnabled()
          ? static_cast<ui::ColorId>(cros_tokens::kCrosSysOnSurface)
          : kColorAshTextColorPrimary;

  for (int i = 0; i < length; ++i) {
    auto* field = new AccessibleInputField();
    views::FocusRing::Get(field)->SetHasFocusPredicate(
        base::BindRepeating([](const views::View* view) { return false; }));
    field->SetBackgroundEnabled(false);
    field->set_controller(this);
    field->SetPreferredSize(
        gfx::Size(kAccessCodeInputFieldWidthDp, kAccessCodeInputFieldHeightDp));
    field->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
    if (is_obscure_pin_) {
      field->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
    } else {
      field->SetTextInputType(ui::TEXT_INPUT_TYPE_NUMBER);
    }
    field->SetTextColorId(text_color_id);
    field->SetFontList(views::Textfield::GetDefaultFontList().Derive(
        kAccessCodeFontSizeDeltaDp, gfx::Font::FontStyle::NORMAL,
        gfx::Font::Weight::NORMAL));
    field->SetBorder(views::CreateThemedSolidSidedBorder(
        gfx::Insets::TLBR(0, 0, kAccessCodeInputFieldUnderlineThicknessDp, 0),
        text_color_id));
    field->SetGroup(kFixedLengthInputGroup);

    // Ignores the a11y focus of |field| because the a11y needs to focus to the
    // FixedLengthCodeInput object.
    field->GetViewAccessibility().set_propagate_focus_to_ancestor(true);
    input_fields_.push_back(field);
    AddChildView(field);
    layout->SetFlexForView(field, 1);
  }
  text_value_for_a11y_ = std::u16string(length, ' ');

  GetViewAccessibility().SetRole(ax::mojom::Role::kTextField);
  GetViewAccessibility().SetIsProtected(is_obscure_pin_);
  GetViewAccessibility().SetValue(text_value_for_a11y_);
  GetViewAccessibility().SetIsEditable(true);
  GetViewAccessibility().AddHTMLAttributes(std::make_pair("type", "tel"));
  OnTextSelectionChanged();
}

FixedLengthCodeInput::~FixedLengthCodeInput() = default;

// Inserts |value| into the |active_field_| and moves focus to the next field
// if it exists.
void FixedLengthCodeInput::InsertDigit(int value) {
  DCHECK_LE(0, value);
  DCHECK_GE(9, value);

  ActiveField()->SetText(base::NumberToString16(value));
  bool was_last_field = IsLastFieldActive();
  ResetTextValueForA11y();
  FocusNextField();
  NotifyAccessibilityEvent(ax::mojom::Event::kTextSelectionChanged, true);
  on_input_change_.Run(was_last_field, GetCode().has_value());
}

// Clears input from the |active_field_|. If |active_field| is empty moves
// focus to the previous field (if exists) and clears input there.
void FixedLengthCodeInput::Backspace() {
  // Ignore backspace on the first field, if empty.
  if (IsFirstFieldActive() && ActiveInput().empty()) {
    return;
  }

  if (ActiveInput().empty()) {
    FocusPreviousField();
  }

  ActiveField()->SetText(std::u16string());
  ResetTextValueForA11y();

  NotifyAccessibilityEvent(ax::mojom::Event::kTextSelectionChanged, true);
  on_input_change_.Run(IsLastFieldActive(), false /*complete*/);
}

// Returns access code as string if all fields contain input.
std::optional<std::string> FixedLengthCodeInput::GetCode() const {
  std::string result;
  size_t length;
  for (ash::AccessibleInputField* field : input_fields_) {
    length = field->GetText().length();
    if (!length) {
      return std::nullopt;
    }

    DCHECK_EQ(1u, length);
    base::StrAppend(&result, {base::UTF16ToUTF8(field->GetText())});
  }
  return result;
}

void FixedLengthCodeInput::SetInputColorId(ui::ColorId color_id) {
  const ui::ColorId error_color_id =
      chromeos::features::IsJellyrollEnabled()
          ? static_cast<ui::ColorId>(cros_tokens::kCrosSysError)
          : kColorAshTextColorAlert;

  for (ash::AccessibleInputField* field : input_fields_) {
    field->SetTextColorId(color_id);
    // We don't update the underline color to red.
    if (color_id != error_color_id) {
      field->SetBorder(views::CreateThemedSolidSidedBorder(
          gfx::Insets::TLBR(0, 0, kAccessCodeInputFieldUnderlineThicknessDp, 0),
          color_id));
    }
  }
}

bool FixedLengthCodeInput::IsGroupFocusTraversable() const {
  return false;
}

views::View* FixedLengthCodeInput::GetSelectedViewForGroup(int group) {
  return ActiveField();
}

void FixedLengthCodeInput::RequestFocus() {
  ActiveField()->RequestFocus();
  OnTextSelectionChanged();
}

void FixedLengthCodeInput::ResetTextValueForA11y() {
  std::u16string result;

  for (size_t i = 0; i < input_fields_.size(); ++i) {
    if (input_fields_[i]->GetText().empty()) {
      result.push_back(' ');
    } else {
      result.push_back(is_obscure_pin_ ? u'•' : input_fields_[i]->GetText()[0]);
    }
  }

  text_value_for_a11y_ = result;
  GetViewAccessibility().SetValue(text_value_for_a11y_);
  OnTextSelectionChanged();
}

gfx::Range FixedLengthCodeInput::GetSelectedRangeOfTextValueForA11y() {
  int text_sel_start = active_input_index_;
  bool empty = text_value_for_a11y_[text_sel_start] == ' ';
  int text_sel_end = text_sel_start + (empty ? 0 : 1);
  return gfx::Range(text_sel_start, text_sel_end);
}

void FixedLengthCodeInput::OnTextSelectionChanged() {
  const gfx::Range& range = GetSelectedRangeOfTextValueForA11y();
  GetViewAccessibility().SetTextSelStart(range.start());
  GetViewAccessibility().SetTextSelEnd(range.end());
}

void FixedLengthCodeInput::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->AddStringAttribute(
      ax::mojom::StringAttribute::kName,
      l10n_util::GetStringUTF8(
          IDS_ASH_LOGIN_PARENT_ACCESS_GENERIC_DESCRIPTION));
}

bool FixedLengthCodeInput::HandleKeyEvent(views::Textfield* sender,
                                          const ui::KeyEvent& key_event) {
  if (key_event.type() != ui::EventType::kKeyPressed) {
    return false;
  }

  // Default handling for events with Alt modifier like spoken feedback.
  if (key_event.IsAltDown()) {
    return false;
  }

  // Default handling for events with Control modifier like sign out.
  if (key_event.IsControlDown()) {
    return false;
  }

  if (sender->GetReadOnly()) {
    return false;
  }

  // FixedLengthCodeInput class responds to limited subset of key press
  // events. All key pressed events not handled below are ignored.
  const ui::KeyboardCode key_code = key_event.key_code();
  if (key_code == ui::VKEY_TAB || key_code == ui::VKEY_BACKTAB) {
    // Allow using tab for keyboard navigation.
    return false;
  } else if (key_code == ui::VKEY_PROCESSKEY) {
    // Default handling for keyboard events that are not generated by physical
    // key press. This can happen, for example, when virtual keyboard button
    // is pressed.
    return false;
  } else if (key_code >= ui::VKEY_0 && key_code <= ui::VKEY_9) {
    InsertDigit(key_code - ui::VKEY_0);
  } else if (key_code >= ui::VKEY_NUMPAD0 && key_code <= ui::VKEY_NUMPAD9) {
    InsertDigit(key_code - ui::VKEY_NUMPAD0);
  } else if (key_code == ui::VKEY_LEFT && arrow_navigation_allowed_) {
    FocusPreviousField();
    NotifyAccessibilityEvent(ax::mojom::Event::kTextSelectionChanged, true);
  } else if (key_code == ui::VKEY_RIGHT && arrow_navigation_allowed_) {
    // Do not allow to leave empty field when moving focus with arrow key.
    if (!ActiveInput().empty()) {
      FocusNextField();
      NotifyAccessibilityEvent(ax::mojom::Event::kTextSelectionChanged, true);
    }
  } else if (key_code == ui::VKEY_BACK) {
    Backspace();
  } else if (key_code == ui::VKEY_RETURN) {
    if (GetCode().has_value()) {
      on_enter_.Run();
    }
  } else if (key_code == ui::VKEY_ESCAPE) {
    on_escape_.Run();
  }

  return true;
}

void FixedLengthCodeInput::ContentsChanged(views::Textfield* sender,
                                           const std::u16string& new_contents) {
  if (new_contents.empty()) {
    return;
  }
  // Called when a character or text is inserted from the virtual keyboard.
  if (new_contents.size() > 1) {
    sender->SetText(std::u16string());
    return;
  }
  unsigned new_digit = 0;
  // If a non-numeric character is inserted from the virtual keyboard, clear it.
  if (!base::StringToUint(new_contents, &new_digit) ||
      !base::IsValueInRangeForNumericType<uint8_t>(new_digit)) {
    sender->SetText(std::u16string());
    return;
  }
  bool was_last_field = IsLastFieldActive();
  ResetTextValueForA11y();
  FocusNextField();
  NotifyAccessibilityEvent(ax::mojom::Event::kTextSelectionChanged, true);
  on_input_change_.Run(was_last_field, GetCode().has_value());
}

bool FixedLengthCodeInput::HandleMouseEvent(views::Textfield* sender,
                                            const ui::MouseEvent& mouse_event) {
  if (!(mouse_event.IsOnlyLeftMouseButton() ||
        mouse_event.IsOnlyRightMouseButton())) {
    return false;
  }

  // Move focus to the field that was selected with mouse input.
  for (size_t i = 0; i < input_fields_.size(); ++i) {
    if (input_fields_[i] == sender) {
      active_input_index_ = i;
      RequestFocus();
      NotifyAccessibilityEvent(ax::mojom::Event::kTextSelectionChanged, true);
      break;
    }
  }

  return true;
}

bool FixedLengthCodeInput::HandleGestureEvent(
    views::Textfield* sender,
    const ui::GestureEvent& gesture_event) {
  if (gesture_event.details().type() != ui::EventType::kGestureTap) {
    return false;
  }

  // Move focus to the field that was selected with gesture.
  for (size_t i = 0; i < input_fields_.size(); ++i) {
    if (input_fields_[i] == sender) {
      active_input_index_ = i;
      RequestFocus();
      NotifyAccessibilityEvent(ax::mojom::Event::kTextSelectionChanged, true);
      break;
    }
  }

  return true;
}

void FixedLengthCodeInput::SetInputEnabled(bool input_enabled) {
  for (ash::AccessibleInputField* field : input_fields_) {
    field->SetEnabled(input_enabled);
  }
}

void FixedLengthCodeInput::SetReadOnly(bool read_only) {
  const bool is_jelly = chromeos::features::IsJellyrollEnabled();
  const ui::ColorId underline_color_id =
      is_jelly ? static_cast<ui::ColorId>(cros_tokens::kCrosSysOnSurface)
               : kColorAshTextColorPrimary;
  for (ash::AccessibleInputField* field : input_fields_) {
    field->SetReadOnly(read_only);
    field->SetBackground(nullptr);
    field->SetBorder(views::CreateThemedSolidSidedBorder(
        gfx::Insets::TLBR(0, 0, kAccessCodeInputFieldUnderlineThicknessDp, 0),
        underline_color_id));
    field->SetCursorEnabled(!read_only);
  }
}

bool FixedLengthCodeInput::IsReadOnly() const {
  if (!input_fields_.empty()) {
    // As SetReadOnly above propagates flag to all fields, just
    // check the first field here instead of implementing complex
    // combining logic.
    return static_cast<views::SelectionControllerDelegate*>(input_fields_[0])
        ->IsReadOnly();
  }
  return false;
}

void FixedLengthCodeInput::ClearInput() {
  for (ash::AccessibleInputField* field : input_fields_) {
    field->SetText(std::u16string());
  }
  active_input_index_ = 0;
  text_value_for_a11y_.clear();
  GetViewAccessibility().RemoveValue();
  RequestFocus();
}

bool FixedLengthCodeInput::IsEmpty() const {
  for (ash::AccessibleInputField* field : input_fields_) {
    if (field->GetText().length()) {
      return false;
    }
  }
  return true;
}

void FixedLengthCodeInput::SetAllowArrowNavigation(bool allowed) {
  arrow_navigation_allowed_ = allowed;
}

void FixedLengthCodeInput::FocusPreviousField() {
  if (active_input_index_ == 0) {
    return;
  }

  --active_input_index_;
  RequestFocus();
}

void FixedLengthCodeInput::FocusNextField() {
  if (IsLastFieldActive()) {
    return;
  }

  ++active_input_index_;
  RequestFocus();
}

bool FixedLengthCodeInput::IsLastFieldActive() const {
  return active_input_index_ == (static_cast<int>(input_fields_.size()) - 1);
}

bool FixedLengthCodeInput::IsFirstFieldActive() const {
  return active_input_index_ == 0;
}

AccessibleInputField* FixedLengthCodeInput::ActiveField() const {
  return input_fields_[active_input_index_];
}

const std::u16string& FixedLengthCodeInput::ActiveInput() const {
  return ActiveField()->GetText();
}

BEGIN_METADATA(FixedLengthCodeInput)
END_METADATA

}  // namespace ash
