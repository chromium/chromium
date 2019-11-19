// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_UI_KEYBOARD_UI_H_
#define ASH_KEYBOARD_UI_KEYBOARD_UI_H_

#include "ash/keyboard/ui/keyboard_export.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "ui/base/ime/text_input_type.h"

namespace aura {
class Window;
}
namespace ui {
class InputMethod;
}

namespace keyboard {

class KeyboardUIController;

// Interface representing a window containing virtual keyboard UI.
class KEYBOARD_EXPORT KeyboardUI {
 public:
  using LoadCallback = base::OnceCallback<void()>;

  KeyboardUI();
  virtual ~KeyboardUI();

  // Begin loading the virtual keyboard window asynchronously.
  // Returns a window immediately, but the UI within the window is not
  // guaranteed to be fully loaded until |callback| is called.
  // |callback| must be called after this function returns.
  // This function can only be called once.
  virtual aura::Window* LoadKeyboardWindow(LoadCallback callback) = 0;

  // Gets the virtual keyboard window i.e. the WebContents window where
  // keyboard extensions are loaded. Returns null if the window has not started
  // loading.
  virtual aura::Window* GetKeyboardWindow() const = 0;

  // Gets the InputMethod that will provide notifications about changes in the
  // text input context.
  virtual ui::InputMethod* GetInputMethod() = 0;

  // Shows the keyboard window. The default implementation simply calls |Show|
  // on the window. An overridden implementation can set up animations or delay
  // the visibility change.
  virtual void ShowKeyboardWindow();

  // Hides the keyboard window. The default implementation simply calls |Hide|
  // on the window. An overridden implementation can set up animations or delay
  // the visibility change.
  virtual void HideKeyboardWindow();

  // Reloads virtual keyboard URL if the current keyboard's web content URL is
  // different. The URL can be different if user switch from password field to
  // any other type input field.
  // At password field, the system virtual keyboard is forced to load even if
  // the current IME provides a customized virtual keyboard. This is needed to
  // prevent IME virtual keyboard logging user's password. Once user switch to
  // other input fields, the virtual keyboard should switch back to the IME
  // provided keyboard, or keep using the system virtual keyboard if IME doesn't
  // provide one.
  // TODO(https://crbug.com/845780): Change this to accept a callback.
  virtual void ReloadKeyboardIfNeeded() = 0;

  // |controller| may be null when KeyboardUIController is being destroyed.
  void SetController(KeyboardUIController* controller);

 protected:
  KeyboardUIController* keyboard_controller() { return keyboard_controller_; }

 private:
  keyboard::KeyboardUIController* keyboard_controller_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(KeyboardUI);
};

}  // namespace keyboard

#endif  // ASH_KEYBOARD_UI_KEYBOARD_UI_H_
