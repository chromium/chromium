// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_INPUT_IME_INPUT_IME_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_INPUT_IME_INPUT_IME_API_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/input_method/input_method_engine_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/input_ime.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "ui/base/ime/chromeos/ime_bridge_observer.h"
#include "ui/base/ime/chromeos/ime_engine_handler_interface.h"
#include "ui/base/ime/text_input_flags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/extensions/api/input_ime/input_ime_api_chromeos.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class Profile;

namespace ui {
class IMEEngineHandlerInterface;

using chromeos::InputMethodEngineBase;

class ImeObserver : public InputMethodEngineBase::Observer {
 public:
  ImeObserver(const std::string& extension_id, Profile* profile);

  ~ImeObserver() override = default;

  // InputMethodEngineBase::Observer overrides.
  void OnActivate(const std::string& component_id) override;
  void OnFocus(int context_id,
               const IMEEngineHandlerInterface::InputContext& context) override;
  void OnBlur(int context_id) override;
  void OnKeyEvent(
      const std::string& component_id,
      const ui::KeyEvent& event,
      IMEEngineHandlerInterface::KeyEventDoneCallback key_data) override;
  void OnReset(const std::string& component_id) override;
  void OnDeactivated(const std::string& component_id) override;
  void OnCompositionBoundsChanged(
      const std::vector<gfx::Rect>& bounds) override;
  void OnSurroundingTextChanged(const std::string& component_id,
                                const std::u16string& text,
                                int cursor_pos,
                                int anchor_pos,
                                int offset_pos) override;

 protected:
  // Helper function used to forward the given event to the |profile_|'s event
  // router, which dipatches the event the extension with |extension_id_|.
  virtual void DispatchEventToExtension(
      extensions::events::HistogramValue histogram_value,
      const std::string& event_name,
      std::unique_ptr<base::ListValue> args) = 0;

  // Returns the type of the current screen.
  virtual std::string GetCurrentScreenType() = 0;

  // Returns true if the extension is ready to accept key event, otherwise
  // returns false.
  bool ShouldForwardKeyEvent() const;

  // Returns true if there are any listeners on the given event.
  // TODO(https://crbug.com/835699): Merge this with |ExtensionHasListener|.
  bool HasListener(const std::string& event_name) const;

  // Returns true if the extension has any listeners on the given event.
  bool ExtensionHasListener(const std::string& event_name) const;

  // Functions used to convert InputContext struct to string
  std::string ConvertInputContextType(
      IMEEngineHandlerInterface::InputContext input_context);
  virtual bool ConvertInputContextAutoCorrect(
      IMEEngineHandlerInterface::InputContext input_context);
  virtual bool ConvertInputContextAutoComplete(
      IMEEngineHandlerInterface::InputContext input_context);
  virtual bool ConvertInputContextSpellCheck(
      IMEEngineHandlerInterface::InputContext input_context);

  std::string extension_id_;
  Profile* profile_;

 private:
  extensions::api::input_ime::AutoCapitalizeType
  ConvertInputContextAutoCapitalize(
      IMEEngineHandlerInterface::InputContext input_context);

  DISALLOW_COPY_AND_ASSIGN(ImeObserver);
};

}  // namespace ui

namespace extensions {
class InputImeEventRouter;
class ExtensionRegistry;

class InputImeEventRouterFactory {
 public:
  static InputImeEventRouterFactory* GetInstance();
  InputImeEventRouter* GetRouter(Profile* profile);
  void RemoveProfile(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<InputImeEventRouterFactory>;
  InputImeEventRouterFactory();
  ~InputImeEventRouterFactory();

  std::map<Profile*, InputImeEventRouter*, ProfileCompare> router_map_;

  DISALLOW_COPY_AND_ASSIGN(InputImeEventRouterFactory);
};

class InputImeKeyEventHandledFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.keyEventHandled",
                             INPUT_IME_KEYEVENTHANDLED)

 protected:
  ~InputImeKeyEventHandledFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class InputImeSetCompositionFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.setComposition",
                             INPUT_IME_SETCOMPOSITION)

 protected:
  ~InputImeSetCompositionFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class InputImeCommitTextFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.commitText", INPUT_IME_COMMITTEXT)

 protected:
  ~InputImeCommitTextFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class InputImeSendKeyEventsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.sendKeyEvents", INPUT_IME_SENDKEYEVENTS)

 protected:
  ~InputImeSendKeyEventsFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class InputImeAPI : public BrowserContextKeyedAPI,
                    public ExtensionRegistryObserver,
                    public EventRouter::Observer {
 public:
  explicit InputImeAPI(content::BrowserContext* context);
  ~InputImeAPI() override;

  static BrowserContextKeyedAPIFactory<InputImeAPI>* GetFactoryInstance();

  // BrowserContextKeyedAPI implementation.
  void Shutdown() override;

  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<InputImeAPI>;
  InputImeEventRouter* input_ime_event_router();

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "InputImeAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  content::BrowserContext* const browser_context_;

  // Listen to extension load, unloaded notifications.
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};

  std::unique_ptr<ui::IMEBridgeObserver> observer_;
};

template <>
struct BrowserContextFactoryDependencies<InputImeAPI> {
  static void DeclareFactoryDependencies(
      BrowserContextKeyedAPIFactory<InputImeAPI>* factory) {
    factory->DependsOn(EventRouterFactory::GetInstance());
    factory->DependsOn(ExtensionRegistryFactory::GetInstance());
  }
};

InputImeEventRouter* GetInputImeEventRouter(Profile* profile);

// Append the extension function name to the error message so that we know where
// the error is from during debugging.
std::string InformativeError(const std::string& error,
                             const char* function_name);
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_INPUT_IME_INPUT_IME_API_H_
