// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_ACCESS_CODE_INPUT_H_
#define ASH_LOGIN_UI_ACCESS_CODE_INPUT_H_

#include <optional>
#include <string>

#include "ash/style/system_textfield.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"

namespace gfx {
class Range;
}

namespace ash {

class AccessCodeInput : public views::View, public views::TextfieldController {
  METADATA_HEADER(AccessCodeInput, views::View)

 public:
  static constexpr int kAccessCodeInputFieldUnderlineThicknessDp = 2;
  static constexpr int kAccessCodeInputFieldHeightDp =
      24 + kAccessCodeInputFieldUnderlineThicknessDp;

  AccessCodeInput() = default;

  ~AccessCodeInput() override = default;

  // Deletes the last character.
  virtual void Backspace() = 0;

  // Appends a digit to the code.
  virtual void InsertDigit(int value) = 0;

  // Returns access code as string.
  virtual std::optional<std::string> GetCode() const = 0;

  // Sets the color of the input text.
  virtual void SetInputColorId(ui::ColorId color_id) = 0;

  virtual void SetInputEnabled(bool input_enabled) = 0;

  // Makes the internal fields read only. In contrast to 'SetInputEnabled',
  // the focus remain on the element.
  virtual void SetReadOnly(bool read_only) = 0;
  virtual bool IsReadOnly() const = 0;

  // Clears the input field(s).
  virtual void ClearInput() = 0;
};

class FlexCodeInput : public AccessCodeInput {
  METADATA_HEADER(FlexCodeInput, AccessCodeInput)

 public:
  using OnInputChange = base::RepeatingCallback<void(bool enable_submit)>;
  using OnEnter = base::RepeatingClosure;
  using OnEscape = base::RepeatingClosure;

  // Builds the view for an access code that consists out of an unknown number
  // of characters. |on_input_change| will be called upon character insertion,
  // deletion or change. |on_enter| will be called when code is complete and
  // user presses enter to submit it for validation. |on_escape| will be called
  // when pressing the escape key. |obscure_pin| determines whether the entered
  // pin is displayed as clear text or as bullet points.
  FlexCodeInput(OnInputChange on_input_change,
                OnEnter on_enter,
                OnEscape on_escape,
                bool obscure_pin);

  FlexCodeInput(const FlexCodeInput&) = delete;
  FlexCodeInput& operator=(const FlexCodeInput&) = delete;
  ~FlexCodeInput() override;

  // Appends |value| to the code
  void InsertDigit(int value) override;

  // Deletes the last character or the selected text.
  void Backspace() override;

  // Returns access code as string if field contains input.
  std::optional<std::string> GetCode() const override;

  // Sets the color of the input text.
  void SetInputColorId(ui::ColorId color_id) override;

  void SetInputEnabled(bool input_enabled) override;

  void SetReadOnly(bool read_only) override;
  bool IsReadOnly() const override;

  // Clears text in input text field.
  void ClearInput() override;

  void RequestFocus() override;

  void SetAccessibleNameOnTextfield(const std::u16string& name);

  // views::TextfieldController
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;

  // views::TextfieldController
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

 private:
  raw_ptr<SystemTextfield> code_field_;

  // To be called when access input code changes (character is inserted, deleted
  // or updated). Passes true when code non-empty.
  OnInputChange on_input_change_;

  // To be called when user pressed enter to submit.
  OnEnter on_enter_;

  // To be called when user presses escape to go back.
  OnEscape on_escape_;
};

// Accessible input field for a single digit in fixed length codes.
// Customizes field description and focus behavior.
class AccessibleInputField : public SystemTextfield {
  METADATA_HEADER(AccessibleInputField, SystemTextfield)

 public:
  AccessibleInputField();

  AccessibleInputField(const AccessibleInputField&) = delete;
  AccessibleInputField& operator=(const AccessibleInputField&) = delete;

  ~AccessibleInputField() override = default;

  // views::Textfield:
  bool IsGroupFocusTraversable() const override;
  View* GetSelectedViewForGroup(int group) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
};

// Digital access code input view for variable length of input codes.
// Displays a separate underscored field for every input code digit.
class FixedLengthCodeInput : public AccessCodeInput {
  METADATA_HEADER(FixedLengthCodeInput, AccessCodeInput)

 public:
  using OnInputChange =
      base::RepeatingCallback<void(bool last_field_active, bool complete)>;
  using OnEnter = base::RepeatingClosure;
  using OnEscape = base::RepeatingClosure;

  class TestApi {
   public:
    explicit TestApi(FixedLengthCodeInput* fixed_length_code_input)
        : fixed_length_code_input_(fixed_length_code_input) {}
    ~TestApi() = default;

    views::Textfield* GetInputTextField(int index) {
      DCHECK_LT(static_cast<size_t>(index),
                fixed_length_code_input_->input_fields_.size());
      return fixed_length_code_input_->input_fields_[index];
    }

    std::optional<std::string> GetCode() const {
      return fixed_length_code_input_->GetCode();
    }

    int GetActiveIndex() const {
      return fixed_length_code_input_->active_input_index_;
    }

   private:
    raw_ptr<FixedLengthCodeInput> fixed_length_code_input_;
  };

  // Builds the view for an access code that consists out of |length| digits.
  // |on_input_change| will be called upon access code digit insertion, deletion
  // or change. True will be passed if the current code is complete (all digits
  // have input values) and false otherwise. |on_enter| will be called when code
  // is complete and user presses enter to submit it for validation. |on_escape|
  // will be called when pressing the escape key. |obscure_pin| determines
  // whether the entered pin is displayed as clear text or as bullet points.
  FixedLengthCodeInput(int length,
                       OnInputChange on_input_change,
                       OnEnter on_enter,
                       OnEscape on_escape,
                       bool obscure_pin);

  ~FixedLengthCodeInput() override;
  FixedLengthCodeInput(const FixedLengthCodeInput&) = delete;
  FixedLengthCodeInput& operator=(const FixedLengthCodeInput&) = delete;

  // Inserts |value| into the |active_field_| and moves focus to the next field
  // if it exists.
  void InsertDigit(int value) override;
  void OnTextSelectionChanged();

  // Clears input from the |active_field_|. If |active_field| is empty moves
  // focus to the previous field (if exists) and clears input there.
  void Backspace() override;

  // Returns access code as string if all fields contain input.
  std::optional<std::string> GetCode() const override;

  // Sets the color of the input text.
  void SetInputColorId(ui::ColorId color_id) override;

  // views::View:
  bool IsGroupFocusTraversable() const override;

  View* GetSelectedViewForGroup(int group) override;

  void RequestFocus() override;

  // Resets the |text_value_for_a11y_| when input fields have changed.
  void ResetTextValueForA11y();

  // Returns current selected text range of |text_value_for_a11y_|.
  gfx::Range GetSelectedRangeOfTextValueForA11y();

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  bool HandleMouseEvent(views::Textfield* sender,
                        const ui::MouseEvent& mouse_event) override;

  bool HandleGestureEvent(views::Textfield* sender,
                          const ui::GestureEvent& gesture_event) override;

  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;

  // Enables/disables entering a PIN. Currently, there is no use-case that uses
  // this with fixed length PINs.
  void SetInputEnabled(bool input_enabled) override;

  void SetReadOnly(bool read_only) override;
  bool IsReadOnly() const override;

  // Clears the PIN fields.
  void ClearInput() override;

  // Whether all fields are empty.
  bool IsEmpty() const;

 protected:
  // Allow subclasses to control whether the fields can be navigated with
  // arrows.
  void SetAllowArrowNavigation(bool allowed);

  int active_input_index() { return active_input_index_; }

  base::CallbackListSubscription AddActiveInputIndexChanged(
      views::PropertyChangedCallback callback) {
    return AddPropertyChangedCallback(&active_input_index_,
                                      std::move(callback));
  }

 private:
  // Moves focus to the current input field.
  void FocusActiveField();

  // Moves focus to the previous input field if it exists.
  void FocusPreviousField();

  // Moves focus to the next input field if it exists.
  void FocusNextField();

  // Returns whether first/last input field is currently active.
  bool IsFirstFieldActive() const;
  bool IsLastFieldActive() const;

  bool HasEmptyFieldToTheLeft() const;

  // Returns pointer to the active input field.
  AccessibleInputField* ActiveField() const;

  // Returns text in the active input field.
  const std::u16string& ActiveInput() const;

  // To be called when access input code changes (digit is inserted, deleted or
  // updated). Passes true when code is complete (all digits have input value)
  // and false otherwise.
  OnInputChange on_input_change_;

  // To be called when user pressed enter to submit.
  OnEnter on_enter_;
  // To be called when user pressed escape to close view.
  OnEscape on_escape_;

  // An active/focused input field index. Incoming digit will be inserted here.
  int active_input_index_ = 0;

  // Unowned input textfields ordered from the first to the last digit.
  std::vector<raw_ptr<AccessibleInputField, VectorExperimental>> input_fields_;

  // Value of current input, associate with AX event. The value will be the
  // concat string of input fields. i.e. [1][2][3][|][][], text_value_for_a11y_
  // = "123   ".
  std::u16string text_value_for_a11y_;

  // Whether the user can navigate the input fields with the arrow keys.
  bool arrow_navigation_allowed_ = true;

  // Whether the digits should be rendered as '*' (bullets) instead of digits.
  // This also affects the ChromeVox behaviour, preventing the digits from
  // being read out loud.
  bool is_obscure_pin_ = true;

  base::WeakPtrFactory<FixedLengthCodeInput> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_ACCESS_CODE_INPUT_H_
