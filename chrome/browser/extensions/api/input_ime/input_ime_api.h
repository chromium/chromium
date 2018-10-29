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
#include "base/scoped_observer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/input_method/input_method_engine_base.h"
#include "chrome/common/extensions/api/input_ime.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "ui/base/ime/ime_bridge_observer.h"
#include "ui/base/ime/ime_engine_handler_interface.h"
#include "ui/base/ime/text_input_flags.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/extensions/api/input_ime/input_ime_api_chromeos.h"
#elif defined(OS_LINUX) || defined(OS_WIN)
#include "chrome/browser/extensions/api/input_ime/input_ime_api_nonchromeos.h"
#endif  // defined(OS_CHROMEOS)

class Profile;

namespace ui {
class IMEEngineHandlerInterface;

class ImeObserver : public input_method::InputMethodEngineBase::Observer {
 public:
  ImeObserver(const std::string& extension_id, Profile* profile);

  ~ImeObserver() override {}

  // input_method::InputMethodEngineBase::Observer overrides.
  void OnActivate(const std::string& component_id) override;
  void OnFocus(const IMEEngineHandlerInterface::InputContext& context) override;
  void OnBlur(int context_id) override;
  void OnKeyEvent(
      const std::string& component_id,
      const input_method::InputMethodEngineBase::KeyboardEvent& event,
      IMEEngineHandlerInterface::KeyEventDoneCallback key_data) override;
  void OnReset(const std::string& component_id) override;
  void OnDeactivated(const std::string& component_id) override;
  void OnCompositionBoundsChanged(
      const std::vector<gfx::Rect>& bounds) override;
  bool IsInterestedInKeyEvent() const override;
  void OnSurroundingTextChanged(const std::string& component_id,
                                const std::string& text,
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
  virtual extensions::api::input_ime::AutoCapitalizeType
  ConvertInputContextAutoCapitalize(
      IMEEngineHandlerInterface::InputContext input_context);
  virtual bool ConvertInputContextSpellCheck(
      IMEEngineHandlerInterface::InputContext input_context);

  std::string extension_id_;
  Profile* profile_;

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

class InputImeKeyEventHandledFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.keyEventHandled",
                             INPUT_IME_KEYEVENTHANDLED)

 protected:
  ~InputImeKeyEventHandledFunction() override {}

  // UIThreadExtensionFunction:
  ResponseAction Run() override;
};

class InputImeSetCompositionFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.setComposition",
                             INPUT_IME_SETCOMPOSITION)

 protected:
  ~InputImeSetCompositionFunction() override {}

  // UIThreadExtensionFunction:
  ResponseAction Run() override;
};

class InputImeCommitTextFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.commitText", INPUT_IME_COMMITTEXT)

 protected:
  ~InputImeCommitTextFunction() override {}

  // UIThreadExtensionFunction:
  ResponseAction Run() override;
};

class InputImeSendKeyEventsFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("input.ime.sendKeyEvents", INPUT_IME_SENDKEYEVENTS)

 protected:
  ~InputImeSendKeyEventsFunction() override {}

  // UIThreadExtensionFunction:
  ResponseAction Run() override;
};

class InputImeAPI : public BrowserContextKeyedAPI,
                    public ExtensionRegistryObserver,
                    public EventRouter::Observer,
                    public content::NotificationObserver {
 public:
  explicit InputImeAPI(content::BrowserContext* context);
  ~InputImeAPI() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<InputImeAPI>* GetFactoryInstance();

  void Shutdown() override;

  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

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
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  content::NotificationRegistrar registrar_;

  std::unique_ptr<ui::IMEBridgeObserver> observer_;
};

InputImeEventRouter* GetInputImeEventRouter(Profile* profile);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_INPUT_IME_INPUT_IME_API_H_
