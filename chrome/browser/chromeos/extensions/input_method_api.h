// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_INPUT_METHOD_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_INPUT_METHOD_API_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/common/extensions/api/input_method_private.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"

namespace chromeos {
class ExtensionDictionaryEventRouter;
class ExtensionInputMethodEventRouter;
class ExtensionImeMenuEventRouter;
}

namespace extensions {

// Implements the inputMethodPrivate.getInputMethodConfig  method.
class InputMethodPrivateGetInputMethodConfigFunction
    : public ExtensionFunction {
 public:
  InputMethodPrivateGetInputMethodConfigFunction() {}

 protected:
  ~InputMethodPrivateGetInputMethodConfigFunction() override {}

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.getInputMethodConfig",
                             INPUTMETHODPRIVATE_GETINPUTMETHODCONFIG)
  DISALLOW_COPY_AND_ASSIGN(InputMethodPrivateGetInputMethodConfigFunction);
};

// Implements the inputMethodPrivate.getCurrentInputMethod method.
class InputMethodPrivateGetCurrentInputMethodFunction
    : public ExtensionFunction {
 public:
  InputMethodPrivateGetCurrentInputMethodFunction() {}

 protected:
  ~InputMethodPrivateGetCurrentInputMethodFunction() override {}

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.getCurrentInputMethod",
                             INPUTMETHODPRIVATE_GETCURRENTINPUTMETHOD)
  DISALLOW_COPY_AND_ASSIGN(InputMethodPrivateGetCurrentInputMethodFunction);
};

// Implements the inputMethodPrivate.setCurrentInputMethod method.
class InputMethodPrivateSetCurrentInputMethodFunction
    : public ExtensionFunction {
 public:
  InputMethodPrivateSetCurrentInputMethodFunction() {}

 protected:
  ~InputMethodPrivateSetCurrentInputMethodFunction() override {}

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.setCurrentInputMethod",
                             INPUTMETHODPRIVATE_SETCURRENTINPUTMETHOD)
  DISALLOW_COPY_AND_ASSIGN(InputMethodPrivateSetCurrentInputMethodFunction);
};

// Implements the inputMethodPrivate.getInputMethods method.
class InputMethodPrivateGetInputMethodsFunction : public ExtensionFunction {
 public:
  InputMethodPrivateGetInputMethodsFunction() {}

 protected:
  ~InputMethodPrivateGetInputMethodsFunction() override {}

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.getInputMethods",
                             INPUTMETHODPRIVATE_GETINPUTMETHODS)
  DISALLOW_COPY_AND_ASSIGN(InputMethodPrivateGetInputMethodsFunction);
};

// Implements the inputMethodPrivate.fetchAllDictionaryWords method.
class InputMethodPrivateFetchAllDictionaryWordsFunction
    : public ExtensionFunction {
 public:
  InputMethodPrivateFetchAllDictionaryWordsFunction() {}

 protected:
  ~InputMethodPrivateFetchAllDictionaryWordsFunction() override {}

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.fetchAllDictionaryWords",
                             INPUTMETHODPRIVATE_FETCHALLDICTIONARYWORDS)
  DISALLOW_COPY_AND_ASSIGN(InputMethodPrivateFetchAllDictionaryWordsFunction);
};

// Implements the inputMethodPrivate.addWordToDictionary method.
class InputMethodPrivateAddWordToDictionaryFunction : public ExtensionFunction {
 public:
  InputMethodPrivateAddWordToDictionaryFunction() {}

 protected:
  ~InputMethodPrivateAddWordToDictionaryFunction() override {}

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.addWordToDictionary",
                             INPUTMETHODPRIVATE_ADDWORDTODICTIONARY)
  DISALLOW_COPY_AND_ASSIGN(InputMethodPrivateAddWordToDictionaryFunction);
};

// Implements the inputMethodPrivate.getEncryptSyncEnabled method.
class InputMethodPrivateGetEncryptSyncEnabledFunction
    : public ExtensionFunction {
 public:
  InputMethodPrivateGetEncryptSyncEnabledFunction() {}

 protected:
  ~InputMethodPrivateGetEncryptSyncEnabledFunction() override {}

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.getEncryptSyncEnabled",
                             INPUTMETHODPRIVATE_GETENCRYPTSYNCENABLED)
  DISALLOW_COPY_AND_ASSIGN(InputMethodPrivateGetEncryptSyncEnabledFunction);
};

// Implements the inputMethodPrivate.setXkbLayout method.
class InputMethodPrivateSetXkbLayoutFunction : public ExtensionFunction {
 public:
  InputMethodPrivateSetXkbLayoutFunction() {}

 protected:
  ~InputMethodPrivateSetXkbLayoutFunction() override {}

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.setXkbLayout",
                             INPUTMETHODPRIVATE_SETXKBLAYOUT)
  DISALLOW_COPY_AND_ASSIGN(InputMethodPrivateSetXkbLayoutFunction);
};

// Implements the inputMethodPrivate.showInputView method.
class InputMethodPrivateShowInputViewFunction : public ExtensionFunction {
 public:
  InputMethodPrivateShowInputViewFunction() {}

 protected:
  ~InputMethodPrivateShowInputViewFunction() override {}

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.showInputView",
                             INPUTMETHODPRIVATE_SHOWINPUTVIEW)
  DISALLOW_COPY_AND_ASSIGN(InputMethodPrivateShowInputViewFunction);
};

// Implements the inputMethodPrivate.hideInputView method.
class InputMethodPrivateHideInputViewFunction : public ExtensionFunction {
 public:
  InputMethodPrivateHideInputViewFunction() = default;
  InputMethodPrivateHideInputViewFunction(
      const InputMethodPrivateHideInputViewFunction&) = delete;
  InputMethodPrivateHideInputViewFunction& operator=(
      const InputMethodPrivateHideInputViewFunction&) = delete;

 protected:
  ~InputMethodPrivateHideInputViewFunction() override {}

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.hideInputView",
                             INPUTMETHODPRIVATE_HIDEINPUTVIEW)
};

// Implements the inputMethodPrivate.openOptionsPage method.
class InputMethodPrivateOpenOptionsPageFunction : public ExtensionFunction {
 public:
  InputMethodPrivateOpenOptionsPageFunction() {}

 protected:
  ~InputMethodPrivateOpenOptionsPageFunction() override {}

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.openOptionsPage",
                             INPUTMETHODPRIVATE_OPENOPTIONSPAGE)
  DISALLOW_COPY_AND_ASSIGN(InputMethodPrivateOpenOptionsPageFunction);
};

class InputMethodPrivateGetSurroundingTextFunction : public ExtensionFunction {
 public:
  InputMethodPrivateGetSurroundingTextFunction() {}

 protected:
  ~InputMethodPrivateGetSurroundingTextFunction() override {}

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.getSurroundingText",
                             INPUTMETHODPRIVATE_GETSURROUNDINGTEXT)
  DISALLOW_COPY_AND_ASSIGN(InputMethodPrivateGetSurroundingTextFunction);
};

class InputMethodPrivateGetSettingsFunction : public ExtensionFunction {
 public:
  InputMethodPrivateGetSettingsFunction() = default;

 protected:
  ~InputMethodPrivateGetSettingsFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.getSettings",
                             INPUTMETHODPRIVATE_GETSETTINGS)
  DISALLOW_COPY_AND_ASSIGN(InputMethodPrivateGetSettingsFunction);
};

class InputMethodPrivateSetSettingsFunction : public ExtensionFunction {
 public:
  InputMethodPrivateSetSettingsFunction() = default;

 protected:
  ~InputMethodPrivateSetSettingsFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.setSettings",
                             INPUTMETHODPRIVATE_SETSETTINGS)
  DISALLOW_COPY_AND_ASSIGN(InputMethodPrivateSetSettingsFunction);
};

class InputMethodPrivateSetCompositionRangeFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.setCompositionRange",
                             INPUTMETHODPRIVATE_SETCOMPOSITIONRANGE)

 protected:
  ~InputMethodPrivateSetCompositionRangeFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class InputMethodPrivateSetComposingRangeFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.setComposingRange",
                             INPUTMETHODPRIVATE_SETCOMPOSINGRANGE)

 protected:
  ~InputMethodPrivateSetComposingRangeFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class InputMethodPrivateSetSelectionRangeFunction : public ExtensionFunction {
 public:
  InputMethodPrivateSetSelectionRangeFunction(
      const InputMethodPrivateSetSelectionRangeFunction&) = delete;
  InputMethodPrivateSetSelectionRangeFunction& operator=(
      const InputMethodPrivateSetSelectionRangeFunction&) = delete;
  InputMethodPrivateSetSelectionRangeFunction() = default;

 protected:
  ~InputMethodPrivateSetSelectionRangeFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.setSelectionRange",
                             INPUTMETHODPRIVATE_SETSELECTIONRANGE)
};

class InputMethodPrivateGetAutocorrectRangeFunction : public ExtensionFunction {
 public:
  InputMethodPrivateGetAutocorrectRangeFunction(
      const InputMethodPrivateGetAutocorrectRangeFunction&) = delete;
  InputMethodPrivateGetAutocorrectRangeFunction& operator=(
      const InputMethodPrivateGetAutocorrectRangeFunction&) = delete;
  InputMethodPrivateGetAutocorrectRangeFunction() = default;

 protected:
  ~InputMethodPrivateGetAutocorrectRangeFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.getAutocorrectRange",
                             INPUTMETHODPRIVATE_GETAUTOCORRECTRANGE)
};

class InputMethodPrivateGetAutocorrectCharacterBoundsFunction
    : public ExtensionFunction {
 public:
  InputMethodPrivateGetAutocorrectCharacterBoundsFunction(
      const InputMethodPrivateGetAutocorrectCharacterBoundsFunction&) = delete;
  InputMethodPrivateGetAutocorrectCharacterBoundsFunction& operator=(
      const InputMethodPrivateGetAutocorrectCharacterBoundsFunction&) = delete;
  InputMethodPrivateGetAutocorrectCharacterBoundsFunction() = default;

 protected:
  ~InputMethodPrivateGetAutocorrectCharacterBoundsFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.getAutocorrectCharacterBounds",
                             INPUTMETHODPRIVATE_GETAUTOCORRECTCHARACTERBOUNDS)
};

class InputMethodPrivateSetAutocorrectRangeFunction : public ExtensionFunction {
 public:
  InputMethodPrivateSetAutocorrectRangeFunction(
      const InputMethodPrivateSetAutocorrectRangeFunction&) = delete;
  InputMethodPrivateSetAutocorrectRangeFunction& operator=(
      const InputMethodPrivateSetAutocorrectRangeFunction&) = delete;
  InputMethodPrivateSetAutocorrectRangeFunction() = default;

 protected:
  ~InputMethodPrivateSetAutocorrectRangeFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.setAutocorrectRange",
                             INPUTMETHODPRIVATE_SETAUTOCORRECTRANGE)
};

class InputMethodPrivateResetFunction : public ExtensionFunction {
 public:
  InputMethodPrivateResetFunction() = default;
  InputMethodPrivateResetFunction(const InputMethodPrivateResetFunction&) =
      delete;
  InputMethodPrivateResetFunction& operator=(
      const InputMethodPrivateResetFunction&) = delete;

 protected:
  ~InputMethodPrivateResetFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.reset",
                             INPUTMETHODPRIVATE_RESET)
};

class InputMethodPrivateOnAutocorrectFunction : public ExtensionFunction {
 public:
  InputMethodPrivateOnAutocorrectFunction(
      const InputMethodPrivateOnAutocorrectFunction&) = delete;
  InputMethodPrivateOnAutocorrectFunction& operator=(
      const InputMethodPrivateOnAutocorrectFunction&) = delete;
  InputMethodPrivateOnAutocorrectFunction() = default;

 protected:
  ~InputMethodPrivateOnAutocorrectFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.onAutocorrect",
                             INPUTMETHODPRIVATE_ONAUTOCORRECT)
};

class InputMethodAPI : public BrowserContextKeyedAPI,
                       public extensions::EventRouter::Observer {
 public:
  explicit InputMethodAPI(content::BrowserContext* context);
  ~InputMethodAPI() override;

  // Returns input method name for the given XKB (X keyboard extensions in X
  // Window System) id.
  static std::string GetInputMethodForXkb(const std::string& xkb_id);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<InputMethodAPI>* GetFactoryInstance();

  // BrowserContextKeyedAPI implementation.
  void Shutdown() override;

  // EventRouter::Observer implementation.
  void OnListenerAdded(const extensions::EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<InputMethodAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "InputMethodAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  content::BrowserContext* const context_;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<chromeos::ExtensionInputMethodEventRouter>
      input_method_event_router_;
  std::unique_ptr<chromeos::ExtensionDictionaryEventRouter>
      dictionary_event_router_;
  std::unique_ptr<chromeos::ExtensionImeMenuEventRouter> ime_menu_event_router_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodAPI);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_INPUT_METHOD_API_H_
