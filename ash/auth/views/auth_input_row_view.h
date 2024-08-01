// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AUTH_VIEWS_AUTH_INPUT_ROW_VIEW_H_
#define ASH_AUTH_VIEWS_AUTH_INPUT_ROW_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/auth/views/auth_textfield.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/style/icon_button.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "ui/base/ime/ash/ime_keyboard.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class ToggleImageButton;
}  // namespace views

namespace ash {

class ASH_EXPORT AuthInputRowView : public views::View,
                                    public ImeController::Observer,
                                    public ui::ImplicitAnimationObserver,
                                    public AuthTextfield::Observer {
  METADATA_HEADER(AuthInputRowView, views::View)

 public:
  enum class AuthType {
    kPassword,
    kPin,
  };

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnSubmit(const std::u16string& text) {}
    virtual void OnEscape() {}
    virtual void OnContentsChanged(const std::u16string& text) {}
    virtual void OnCapsLockStateChanged(bool visible) {}
    virtual void OnTextVisibleChanged(bool visible) {}
    virtual void OnTextfieldBlur() {}
    virtual void OnTextfieldFocus() {}
  };

  class TestApi {
   public:
    explicit TestApi(AuthInputRowView* view);
    ~TestApi();
    TestApi(const TestApi&) = delete;
    TestApi& operator=(const TestApi&) = delete;

    raw_ptr<AuthTextfield> GetTextfield() const;
    raw_ptr<views::ToggleImageButton> GetDisplayTextButton() const;
    raw_ptr<IconButton> GetSubmitButton() const;
    raw_ptr<views::ImageView> GetCapsLockIcon() const;
    raw_ptr<AuthInputRowView> GetView() const;

   private:
    const raw_ptr<AuthInputRowView> view_;
  };

  AuthInputRowView(AuthType type);

  AuthInputRowView(const AuthInputRowView&) = delete;
  AuthInputRowView& operator=(const AuthInputRowView&) = delete;

  ~AuthInputRowView() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void RequestFocus() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;

  // AuthTextfield::Observer:
  void OnTextfieldBlur() override;
  void OnTextfieldFocus() override;
  void OnContentsChanged(const std::u16string& new_contents) override;
  void OnTextVisibleChanged(bool visible) override;
  void OnSubmit() override;
  void OnEscape() override;

  // Initialize display text button's color, text, a11y.
  void InitDispalyPasswordButton();

  // Sets whether the display text button is visible.
  void SetDisplayTextButtonVisible(bool visible);

  // Invert the textfield type and toggle the display password button.
  void ToggleTextDisplayingState();

  // ImeController::Observer:
  void OnCapsLockChanged(bool enabled) override;
  void OnKeyboardLayoutNameChanged(const std::string&) override {}

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  void HandleLeftIconsVisibilities(bool handling_capslock);

  void InsertDigit(int digit);
  void Backspace();

  // Notify the observers about the submit purpose.
  void Submit();

  void SetAccessibleNameOnTextfield(const std::u16string& new_name);

  // Enables or disables the following UI elements:
  // - View
  // - Auth textfield
  // - Submit button
  // - Display text button
  // No "Get" function is needed since the state is the same as
  // the GetEnabled return value.
  void SetInputEnabled(bool enabled);

  // Clear the textfield and set the display text button to hide state.
  void ResetState();

  base::WeakPtr<AuthInputRowView> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  void ConfigureRootLayout();
  void CreateAndConfigureInputRow();
  void CreateAndConfigureCapslockIcon();
  void CreateAndConfigureTextfieldContainer();
  void CreateFocusRingForInputRow();
  void CreateAndConfigureDisplayTextButton();
  void CreateAndConfigureSubmitButton();

  // Increases/decreases the contrast of the capslock icon.
  void SetCapsLockHighlighted(bool highlight);

  // Notify the observers the ESC press.
  void Escape();

  // Needs to be true in order for SubmitPassword to be ran. Returns true if the
  // textfield is not empty and the text is editable.
  bool IsInputSubmittable() const;

  raw_ptr<AuthTextfield> textfield_ = nullptr;
  raw_ptr<IconButton> submit_button_ = nullptr;
  raw_ptr<views::ToggleImageButton> display_text_button_ = nullptr;

  raw_ptr<views::ImageView> capslock_icon_ = nullptr;

  raw_ptr<views::View> input_row_ = nullptr;

  raw_ptr<views::BoxLayout> input_row_layout_ = nullptr;

  const AuthType auth_type_;

  base::ScopedObservation<ImeController, ImeController::Observer>
      input_methods_observer_{this};

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<AuthInputRowView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_AUTH_VIEWS_AUTH_INPUT_ROW_VIEW_H_
