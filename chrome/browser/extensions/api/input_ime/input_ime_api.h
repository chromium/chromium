// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_INPUT_IME_INPUT_IME_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_INPUT_IME_INPUT_IME_API_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/input_method/input_method_engine.h"
#include "chrome/browser/extensions/api/input_ime/input_ime_api_chromeos.h"
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
#include "ui/base/ime/ash/text_input_method.h"
#include "ui/base/ime/text_input_flags.h"

class Profile;

namespace extensions {
class InputImeEventRouter;
class ExtensionRegistry;

class InputImeEventRouterFactory {
 public:
  InputImeEventRouterFactory(const InputImeEventRouterFactory&) = delete;
  InputImeEventRouterFactory& operator=(const InputImeEventRouterFactory&) =
      delete;

  static InputImeEventRouterFactory* GetInstance();
  InputImeEventRouter* GetRouter(Profile* profile);
  void RemoveProfile(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<InputImeEventRouterFactory>;
  InputImeEventRouterFactory();
  ~InputImeEventRouterFactory();

  std::map<Profile*,
           raw_ptr<InputImeEventRouter, CtnExperimental>,
           ProfileCompare>
      router_map_;
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
  void OnListenerRemoved(const EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<InputImeAPI>;
  InputImeEventRouter* input_ime_event_router();

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "InputImeAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  const raw_ptr<content::BrowserContext> browser_context_;

  // Listen to extension load, unloaded notifications.
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};
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
