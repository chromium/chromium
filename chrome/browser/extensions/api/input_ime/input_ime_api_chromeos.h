// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_INPUT_IME_INPUT_IME_API_CHROMEOS_H_
#define CHROME_BROWSER_EXTENSIONS_API_INPUT_IME_INPUT_IME_API_CHROMEOS_H_

#include <map>
#include <string>
#include <vector>

#include "chrome/browser/extensions/api/input_ime/input_ime_event_router_base.h"
#include "chrome/common/extensions/api/input_ime/input_components_handler.h"
#include "extensions/browser/extension_function.h"

namespace chromeos {

class InputMethodEngine;

}  // namespace chromeos

namespace extensions {

class InputImeClearCompositionFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.clearComposition",
                             INPUT_IME_CLEARCOMPOSITION)

 protected:
  ~InputImeClearCompositionFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class InputImeSetCandidateWindowPropertiesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.setCandidateWindowProperties",
                             INPUT_IME_SETCANDIDATEWINDOWPROPERTIES)

 protected:
  ~InputImeSetCandidateWindowPropertiesFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class InputImeSetCandidatesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.setCandidates", INPUT_IME_SETCANDIDATES)

 protected:
  ~InputImeSetCandidatesFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class InputImeSetCursorPositionFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.setCursorPosition",
                             INPUT_IME_SETCURSORPOSITION)

 protected:
  ~InputImeSetCursorPositionFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class InputImeSetAssistiveWindowPropertiesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.setAssistiveWindowProperties",
                             INPUT_IME_SETASSISTIVEWINDOWPROPERTIES)

 protected:
  ~InputImeSetAssistiveWindowPropertiesFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class InputImeSetAssistiveWindowButtonHighlightedFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.setAssistiveWindowButtonHighlighted",
                             INPUT_IME_SETASSISTIVEWINDOWBUTTONHIGHLIGHTED)
 protected:
  ~InputImeSetAssistiveWindowButtonHighlightedFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class InputImeSetMenuItemsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.setMenuItems", INPUT_IME_SETMENUITEMS)

 protected:
  ~InputImeSetMenuItemsFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class InputImeUpdateMenuItemsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.updateMenuItems",
                             INPUT_IME_UPDATEMENUITEMS)

 protected:
  ~InputImeUpdateMenuItemsFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class InputImeDeleteSurroundingTextFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.deleteSurroundingText",
                             INPUT_IME_DELETESURROUNDINGTEXT)
 protected:
  ~InputImeDeleteSurroundingTextFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class InputImeHideInputViewFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.hideInputView",
                             INPUT_IME_HIDEINPUTVIEW)

 protected:
  ~InputImeHideInputViewFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class InputMethodPrivateFinishComposingTextFunction : public ExtensionFunction {
 public:
  InputMethodPrivateFinishComposingTextFunction(
      const InputMethodPrivateFinishComposingTextFunction&) = delete;
  InputMethodPrivateFinishComposingTextFunction& operator=(
      const InputMethodPrivateFinishComposingTextFunction&) = delete;

  InputMethodPrivateFinishComposingTextFunction() = default;

 protected:
  ~InputMethodPrivateFinishComposingTextFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.finishComposingText",
                             INPUTMETHODPRIVATE_FINISHCOMPOSINGTEXT)
};

class InputMethodPrivateNotifyImeMenuItemActivatedFunction
    : public ExtensionFunction {
 public:
  InputMethodPrivateNotifyImeMenuItemActivatedFunction() = default;

 protected:
  ~InputMethodPrivateNotifyImeMenuItemActivatedFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.notifyImeMenuItemActivated",
                             INPUTMETHODPRIVATE_NOTIFYIMEMENUITEMACTIVATED)
  DISALLOW_COPY_AND_ASSIGN(
      InputMethodPrivateNotifyImeMenuItemActivatedFunction);
};

class InputMethodPrivateGetCompositionBoundsFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.getCompositionBounds",
                             INPUTMETHODPRIVATE_GETCOMPOSITIONBOUNDS)

 protected:
  ~InputMethodPrivateGetCompositionBoundsFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class InputImeEventRouter : public InputImeEventRouterBase {
 public:
  explicit InputImeEventRouter(Profile* profile);
  ~InputImeEventRouter() override;

  bool RegisterImeExtension(
      const std::string& extension_id,
      const std::vector<extensions::InputComponentInfo>& input_components);
  void UnregisterAllImes(const std::string& extension_id);

  chromeos::InputMethodEngine* GetEngine(const std::string& extension_id);
  chromeos::InputMethodEngineBase* GetEngineIfActive(
      const std::string& extension_id,
      std::string* error) override;

  std::string GetUnloadedExtensionId() const {
    return unloaded_component_extension_id_;
  }

  void SetUnloadedExtensionId(const std::string& extension_id) {
    unloaded_component_extension_id_ = extension_id;
  }

 private:
  // The engine map from extension_id to an engine.
  std::map<std::string, std::unique_ptr<chromeos::InputMethodEngine>>
      engine_map_;
  // The first party ime extension which is unloaded unexpectedly.
  std::string unloaded_component_extension_id_;

  DISALLOW_COPY_AND_ASSIGN(InputImeEventRouter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_INPUT_IME_INPUT_IME_API_CHROMEOS_H_
