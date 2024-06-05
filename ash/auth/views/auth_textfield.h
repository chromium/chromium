// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AUTH_VIEWS_AUTH_TEXTFIELD_H_
#define ASH_AUTH_VIEWS_AUTH_TEXTFIELD_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/style/system_textfield.h"
#include "ash/style/system_textfield_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

class AuthTextfieldTimer;

// A textfield that selects all text on focus and allows to switch between
// show/hide password modes.
class ASH_EXPORT AuthTextfield : public SystemTextfield,
                                  public SystemTextfieldController {
  METADATA_HEADER(AuthTextfield, SystemTextfield)
 public:
  enum class AuthType {
    kPassword,
    kPin,
  };

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnTextfieldBlur() {}
    virtual void OnTextfieldFocus() {}
    virtual void OnContentsChanged(const std::u16string& new_contents) {}
    virtual void OnTextVisibleChanged(bool visible) {}
    virtual void OnSubmit() {}
    virtual void OnEscape() {}
  };

  AuthTextfield(AuthType auth_type);
  AuthTextfield(const AuthTextfield&) = delete;
  AuthTextfield& operator=(const AuthTextfield&) = delete;
  ~AuthTextfield() override;

  // views::Textfield:
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;
  void OnBlur() override;
  void OnFocus() override;
  ui::TextInputMode GetTextInputMode() const override;
  bool ShouldDoLearning() override;

  // SystemTextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;
  void ContentsChanged(Textfield* sender,
                       const std::u16string& new_contents) override;

  // This is useful when the display password button is not shown. In such a
  // case, the login text field needs to define its size.
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  void Reset();

  void InsertDigit(int digit);
  void Backspace();

  void SetTextVisible(bool visible);
  bool IsTextVisible() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void ApplyTimerLogic();
  void ResetTimerLogic();

 private:
  void ShowText();
  void HideText();

  const AuthType auth_type_;
  base::ObserverList<Observer> observers_;

  std::unique_ptr<AuthTextfieldTimer> timer_logic_;
};

}  // namespace ash

#endif  // ASH_AUTH_VIEWS_AUTH_TEXTFIELD_H_
