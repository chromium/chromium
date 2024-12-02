// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_INPUT_METHOD_API_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_INPUT_METHOD_API_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/common/extensions/api/input_method_private.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"

namespace chromeos {
class ExtensionDictionaryEventRouter;
class ExtensionInputMethodEventRouter;
class ExtensionImeMenuEventRouter;
class LanguagePackEventRouter;
}

namespace extensions {

// Implements the inputMethodPrivate.getInputMethodConfig  method.
class InputMethodPrivateGetInputMethodConfigFunction
    : public ExtensionFunction {
 public:
  InputMethodPrivateGetInputMethodConfigFunction() = default;

  InputMethodPrivateGetInputMethodConfigFunction(
      const InputMethodPrivateGetInputMethodConfigFunction&) = delete;
  InputMethodPrivateGetInputMethodConfigFunction& operator=(
      const InputMethodPrivateGetInputMethodConfigFunction&) = delete;

 protected:
  ~InputMethodPrivateGetInputMethodConfigFunction() override = default;

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.getInputMethodConfig",
                             INPUTMETHODPRIVATE_GETINPUTMETHODCONFIG)
};

// Implements the inputMethodPrivate.getCurrentInputMethod method.
class InputMethodPrivateGetCurrentInputMethodFunction
    : public ExtensionFunction {
 public:
  InputMethodPrivateGetCurrentInputMethodFunction() = default;

  InputMethodPrivateGetCurrentInputMethodFunction(
      const InputMethodPrivateGetCurrentInputMethodFunction&) = delete;
  InputMethodPrivateGetCurrentInputMethodFunction& operator=(
      const InputMethodPrivateGetCurrentInputMethodFunction&) = delete;

 protected:
  ~InputMethodPrivateGetCurrentInputMethodFunction() override = default;

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.getCurrentInputMethod",
                             INPUTMETHODPRIVATE_GETCURRENTINPUTMETHOD)
};

// Implements the inputMethodPrivate.setCurrentInputMethod method.
class InputMethodPrivateSetCurrentInputMethodFunction
    : public ExtensionFunction {
 public:
  InputMethodPrivateSetCurrentInputMethodFunction() = default;

  InputMethodPrivateSetCurrentInputMethodFunction(
      const InputMethodPrivateSetCurrentInputMethodFunction&) = delete;
  InputMethodPrivateSetCurrentInputMethodFunction& operator=(
      const InputMethodPrivateSetCurrentInputMethodFunction&) = delete;

 protected:
  ~InputMethodPrivateSetCurrentInputMethodFunction() override = default;

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.setCurrentInputMethod",
                             INPUTMETHODPRIVATE_SETCURRENTINPUTMETHOD)
};

// Implements the inputMethodPrivate.switchToLastUsedInputMethod method.
class InputMethodPrivateSwitchToLastUsedInputMethodFunction
    : public ExtensionFunction {
 public:
  InputMethodPrivateSwitchToLastUsedInputMethodFunction() = default;

  InputMethodPrivateSwitchToLastUsedInputMethodFunction(
      const InputMethodPrivateSwitchToLastUsedInputMethodFunction&) = delete;
  InputMethodPrivateSwitchToLastUsedInputMethodFunction& operator=(
      const InputMethodPrivateSwitchToLastUsedInputMethodFunction&) = delete;

 protected:
  ~InputMethodPrivateSwitchToLastUsedInputMethodFunction() override = default;

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.switchToLastUsedInputMethod",
                             INPUTMETHODPRIVATE_SWITCHTOLASTUSEDINPUTMETHOD)
};

// Implements the inputMethodPrivate.getInputMethods method.
class InputMethodPrivateGetInputMethodsFunction : public ExtensionFunction {
 public:
  InputMethodPrivateGetInputMethodsFunction() = default;

  InputMethodPrivateGetInputMethodsFunction(
      const InputMethodPrivateGetInputMethodsFunction&) = delete;
  InputMethodPrivateGetInputMethodsFunction& operator=(
      const InputMethodPrivateGetInputMethodsFunction&) = delete;

 protected:
  ~InputMethodPrivateGetInputMethodsFunction() override = default;

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.getInputMethods",
                             INPUTMETHODPRIVATE_GETINPUTMETHODS)
};

// Implements the inputMethodPrivate.fetchAllDictionaryWords method.
class InputMethodPrivateFetchAllDictionaryWordsFunction
    : public ExtensionFunction {
 public:
  InputMethodPrivateFetchAllDictionaryWordsFunction() = default;

  InputMethodPrivateFetchAllDictionaryWordsFunction(
      const InputMethodPrivateFetchAllDictionaryWordsFunction&) = delete;
  InputMethodPrivateFetchAllDictionaryWordsFunction& operator=(
      const InputMethodPrivateFetchAllDictionaryWordsFunction&) = delete;

 protected:
  ~InputMethodPrivateFetchAllDictionaryWordsFunction() override = default;

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.fetchAllDictionaryWords",
                             INPUTMETHODPRIVATE_FETCHALLDICTIONARYWORDS)
};

// Implements the inputMethodPrivate.addWordToDictionary method.
class InputMethodPrivateAddWordToDictionaryFunction : public ExtensionFunction {
 public:
  InputMethodPrivateAddWordToDictionaryFunction() = default;

  InputMethodPrivateAddWordToDictionaryFunction(
      const InputMethodPrivateAddWordToDictionaryFunction&) = delete;
  InputMethodPrivateAddWordToDictionaryFunction& operator=(
      const InputMethodPrivateAddWordToDictionaryFunction&) = delete;

 protected:
  ~InputMethodPrivateAddWordToDictionaryFunction() override = default;

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.addWordToDictionary",
                             INPUTMETHODPRIVATE_ADDWORDTODICTIONARY)
};

// Implements the inputMethodPrivate.setXkbLayout method.
class InputMethodPrivateSetXkbLayoutFunction : public ExtensionFunction {
 public:
  InputMethodPrivateSetXkbLayoutFunction() = default;

  InputMethodPrivateSetXkbLayoutFunction(
      const InputMethodPrivateSetXkbLayoutFunction&) = delete;
  InputMethodPrivateSetXkbLayoutFunction& operator=(
      const InputMethodPrivateSetXkbLayoutFunction&) = delete;

 protected:
  ~InputMethodPrivateSetXkbLayoutFunction() override = default;

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.setXkbLayout",
                             INPUTMETHODPRIVATE_SETXKBLAYOUT)
};

// Implements the inputMethodPrivate.showInputView method.
class InputMethodPrivateShowInputViewFunction : public ExtensionFunction {
 public:
  InputMethodPrivateShowInputViewFunction() = default;

  InputMethodPrivateShowInputViewFunction(
      const InputMethodPrivateShowInputViewFunction&) = delete;
  InputMethodPrivateShowInputViewFunction& operator=(
      const InputMethodPrivateShowInputViewFunction&) = delete;

 protected:
  ~InputMethodPrivateShowInputViewFunction() override = default;

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.showInputView",
                             INPUTMETHODPRIVATE_SHOWINPUTVIEW)
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
  ~InputMethodPrivateHideInputViewFunction() override = default;

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.hideInputView",
                             INPUTMETHODPRIVATE_HIDEINPUTVIEW)
};

// Implements the inputMethodPrivate.openOptionsPage method.
class InputMethodPrivateOpenOptionsPageFunction : public ExtensionFunction {
 public:
  InputMethodPrivateOpenOptionsPageFunction() = default;

  InputMethodPrivateOpenOptionsPageFunction(
      const InputMethodPrivateOpenOptionsPageFunction&) = delete;
  InputMethodPrivateOpenOptionsPageFunction& operator=(
      const InputMethodPrivateOpenOptionsPageFunction&) = delete;

 protected:
  ~InputMethodPrivateOpenOptionsPageFunction() override = default;

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.openOptionsPage",
                             INPUTMETHODPRIVATE_OPENOPTIONSPAGE)
};

class InputMethodPrivateGetSurroundingTextFunction : public ExtensionFunction {
 public:
  InputMethodPrivateGetSurroundingTextFunction() = default;

  InputMethodPrivateGetSurroundingTextFunction(
      const InputMethodPrivateGetSurroundingTextFunction&) = delete;
  InputMethodPrivateGetSurroundingTextFunction& operator=(
      const InputMethodPrivateGetSurroundingTextFunction&) = delete;

 protected:
  ~InputMethodPrivateGetSurroundingTextFunction() override = default;

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.getSurroundingText",
                             INPUTMETHODPRIVATE_GETSURROUNDINGTEXT)
};

class InputMethodPrivateGetSettingsFunction : public ExtensionFunction {
 public:
  InputMethodPrivateGetSettingsFunction() = default;

  InputMethodPrivateGetSettingsFunction(
      const InputMethodPrivateGetSettingsFunction&) = delete;
  InputMethodPrivateGetSettingsFunction& operator=(
      const InputMethodPrivateGetSettingsFunction&) = delete;

 protected:
  ~InputMethodPrivateGetSettingsFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.getSettings",
                             INPUTMETHODPRIVATE_GETSETTINGS)
};

class InputMethodPrivateSetSettingsFunction : public ExtensionFunction {
 public:
  InputMethodPrivateSetSettingsFunction() = default;

  InputMethodPrivateSetSettingsFunction(
      const InputMethodPrivateSetSettingsFunction&) = delete;
  InputMethodPrivateSetSettingsFunction& operator=(
      const InputMethodPrivateSetSettingsFunction&) = delete;

 protected:
  ~InputMethodPrivateSetSettingsFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.setSettings",
                             INPUTMETHODPRIVATE_SETSETTINGS)
};

class InputMethodPrivateSetCompositionRangeFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.setCompositionRange",
                             INPUTMETHODPRIVATE_SETCOMPOSITIONRANGE)

 protected:
  ~InputMethodPrivateSetCompositionRangeFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
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

class InputMethodPrivateNotifyInputMethodReadyForTestingFunction
    : public ExtensionFunction {
 public:
  InputMethodPrivateNotifyInputMethodReadyForTestingFunction() = default;

  InputMethodPrivateNotifyInputMethodReadyForTestingFunction(
      const InputMethodPrivateNotifyInputMethodReadyForTestingFunction&) =
      delete;
  InputMethodPrivateNotifyInputMethodReadyForTestingFunction& operator=(
      const InputMethodPrivateNotifyInputMethodReadyForTestingFunction&) =
      delete;

 protected:
  ~InputMethodPrivateNotifyInputMethodReadyForTestingFunction() override =
      default;

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION(
      "inputMethodPrivate.notifyInputMethodReadyForTesting",
      INPUTMETHODPRIVATE_NOTIFYINPUTMETHODREADYFORTESTING)
};

class InputMethodPrivateGetLanguagePackStatusFunction
    : public ExtensionFunction {
 public:
  InputMethodPrivateGetLanguagePackStatusFunction() = default;

  InputMethodPrivateGetLanguagePackStatusFunction(
      const InputMethodPrivateGetLanguagePackStatusFunction&) = delete;
  InputMethodPrivateGetLanguagePackStatusFunction& operator=(
      const InputMethodPrivateGetLanguagePackStatusFunction&) = delete;

 protected:
  ~InputMethodPrivateGetLanguagePackStatusFunction() override = default;

  ResponseAction Run() override;

 private:
  DECLARE_EXTENSION_FUNCTION("inputMethodPrivate.getLanguagePackStatus",
                             INPUTMETHODPRIVATE_GETLANGUAGEPACKSTATUS)

  void OnGetLanguagePackStatusComplete(
      const api::input_method_private::LanguagePackStatus result);
};

class InputMethodAPI : public BrowserContextKeyedAPI,
                       public extensions::EventRouter::Observer {
 public:
  explicit InputMethodAPI(content::BrowserContext* context);

  InputMethodAPI(const InputMethodAPI&) = delete;
  InputMethodAPI& operator=(const InputMethodAPI&) = delete;

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

  const raw_ptr<content::BrowserContext> context_;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<chromeos::ExtensionInputMethodEventRouter>
      input_method_event_router_;
  std::unique_ptr<chromeos::ExtensionDictionaryEventRouter>
      dictionary_event_router_;
  std::unique_ptr<chromeos::ExtensionImeMenuEventRouter> ime_menu_event_router_;
  std::unique_ptr<chromeos::LanguagePackEventRouter>
      language_pack_event_router_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_INPUT_METHOD_API_H_
