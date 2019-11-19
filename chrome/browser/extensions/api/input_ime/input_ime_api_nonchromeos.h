// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_INPUT_IME_INPUT_IME_API_NONCHROMEOS_H_
#define CHROME_BROWSER_EXTENSIONS_API_INPUT_IME_INPUT_IME_API_NONCHROMEOS_H_

#include "chrome/browser/extensions/api/input_ime/input_ime_event_router_base.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_function.h"

class Profile;

namespace input_method {
class InputMethodEngine;
}  // namespace input_method

// The status indicates whether the permission has been granted or denied when
// the IME warning bubble has been closed.
enum class ImeWarningBubblePermissionStatus {
  GRANTED,
  GRANTED_AND_NEVER_SHOW,
  DENIED,
  ABORTED
};

namespace extensions {

class InputImeEventRouterBase;

class InputImeEventRouter : public InputImeEventRouterBase {
 public:
  explicit InputImeEventRouter(Profile* profile);
  ~InputImeEventRouter() override;

  // Gets the input method engine if the extension is active.
  input_method::InputMethodEngineBase* GetEngineIfActive(
      const std::string& extension_id) override;

  // Actives the extension with new input method engine, and deletes the
  // previous engine if another extension was active.
  void SetActiveEngine(const std::string& extension_id);

  input_method::InputMethodEngine* active_engine() {
    return active_engine_;
  }

  // Deletes the current input method engine of the specific extension.
  void DeleteInputMethodEngine(const std::string& extension_id);

 private:
  // The active input method engine.
  input_method::InputMethodEngine* active_engine_;

  DISALLOW_COPY_AND_ASSIGN(InputImeEventRouter);
};

class InputImeCreateWindowFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.createWindow", INPUT_IME_CREATEWINDOW)

 protected:
  ~InputImeCreateWindowFunction() override = default;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

class InputImeShowWindowFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.showWindow", INPUT_IME_SHOWWINDOW)

 protected:
  ~InputImeShowWindowFunction() override = default;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

class InputImeHideWindowFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.hideWindow", INPUT_IME_HIDEWINDOW)

 protected:
  ~InputImeHideWindowFunction() override = default;

  // ExtensionFunction:
  ExtensionFunction::ResponseAction Run() override;
};

class InputImeActivateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.activate", INPUT_IME_ACTIVATE)

  // During testing we can disable showing a warning bubble by setting this flag
  // to true, so that the extension can be activated directly.
  static bool disable_bubble_for_testing_;

 protected:
  ~InputImeActivateFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  // Called when the user finishes interacting with the warning bubble.
  // |status| indicates whether the user allows or denies to activate the
  // extension.
  void OnPermissionBubbleFinished(ImeWarningBubblePermissionStatus status);
};

class InputImeDeactivateFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.deactivate", INPUT_IME_DEACTIVATE)

 protected:
  ~InputImeDeactivateFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_INPUT_IME_INPUT_IME_API_NONCHROMEOS_H_
