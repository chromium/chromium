// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/input_method_api.h"

#include <stddef.h>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/extensions/dictionary_event_router.h"
#include "chrome/browser/chromeos/extensions/ime_menu_event_router.h"
#include "chrome/browser/chromeos/extensions/input_method_event_router.h"
#include "chrome/browser/extensions/api/input_ime/input_ime_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/api/input_method_private.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_system.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/ime/chromeos/input_method_descriptor.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/ime/chromeos/input_method_util.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_util.h"

namespace AddWordToDictionary =
    extensions::api::input_method_private::AddWordToDictionary;
namespace SetCurrentInputMethod =
    extensions::api::input_method_private::SetCurrentInputMethod;
namespace SetXkbLayout = extensions::api::input_method_private::SetXkbLayout;
namespace OpenOptionsPage =
    extensions::api::input_method_private::OpenOptionsPage;
namespace OnChanged = extensions::api::input_method_private::OnChanged;
namespace OnDictionaryChanged =
    extensions::api::input_method_private::OnDictionaryChanged;
namespace OnDictionaryLoaded =
    extensions::api::input_method_private::OnDictionaryLoaded;
namespace OnImeMenuActivationChanged =
    extensions::api::input_method_private::OnImeMenuActivationChanged;
namespace OnImeMenuListChanged =
    extensions::api::input_method_private::OnImeMenuListChanged;
namespace OnImeMenuItemsChanged =
    extensions::api::input_method_private::OnImeMenuItemsChanged;
namespace GetSurroundingText =
    extensions::api::input_method_private::GetSurroundingText;

namespace {

// Prefix, which is used by XKB.
const char kXkbPrefix[] = "xkb:";
const char kErrorFailToShowInputView[] =
    "Unable to show the input view window.";

}  // namespace

namespace extensions {

ExtensionFunction::ResponseAction
InputMethodPrivateGetInputMethodConfigFunction::Run() {
#if !defined(OS_CHROMEOS)
  EXTENSION_FUNCTION_VALIDATE(false);
#else
  std::unique_ptr<base::DictionaryValue> output(new base::DictionaryValue());
  output->SetBoolean(
      "isPhysicalKeyboardAutocorrectEnabled",
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDisablePhysicalKeyboardAutocorrect));
  output->SetBoolean("isImeMenuActivated",
                     base::FeatureList::IsEnabled(features::kOptInImeMenu) &&
                         Profile::FromBrowserContext(browser_context())
                             ->GetPrefs()
                             ->GetBoolean(prefs::kLanguageImeMenuActivated));
  return RespondNow(OneArgument(std::move(output)));
#endif
}

ExtensionFunction::ResponseAction
InputMethodPrivateGetCurrentInputMethodFunction::Run() {
#if !defined(OS_CHROMEOS)
  EXTENSION_FUNCTION_VALIDATE(false);
#else
  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::Get();
  return RespondNow(OneArgument(std::make_unique<base::Value>(
      manager->GetActiveIMEState()->GetCurrentInputMethod().id())));
#endif
}

ExtensionFunction::ResponseAction
InputMethodPrivateSetCurrentInputMethodFunction::Run() {
#if !defined(OS_CHROMEOS)
  EXTENSION_FUNCTION_VALIDATE(false);
#else
  std::unique_ptr<SetCurrentInputMethod::Params> params(
      SetCurrentInputMethod::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  scoped_refptr<chromeos::input_method::InputMethodManager::State> ime_state =
      chromeos::input_method::InputMethodManager::Get()->GetActiveIMEState();
  const std::vector<std::string>& input_methods =
      ime_state->GetActiveInputMethodIds();
  for (size_t i = 0; i < input_methods.size(); ++i) {
    const std::string& input_method = input_methods[i];
    if (input_method == params->input_method_id) {
      ime_state->ChangeInputMethod(params->input_method_id,
                                   false /* show_message */);
      return RespondNow(NoArguments());
    }
  }
  return RespondNow(Error("Invalid input method id."));
#endif
}

ExtensionFunction::ResponseAction
InputMethodPrivateGetInputMethodsFunction::Run() {
#if !defined(OS_CHROMEOS)
  EXTENSION_FUNCTION_VALIDATE(false);
#else
  std::unique_ptr<base::ListValue> output(new base::ListValue());
  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::Get();
  chromeos::input_method::InputMethodUtil* util = manager->GetInputMethodUtil();
  scoped_refptr<chromeos::input_method::InputMethodManager::State> ime_state =
      manager->GetActiveIMEState();
  std::unique_ptr<chromeos::input_method::InputMethodDescriptors>
      input_methods = ime_state->GetActiveInputMethods();
  for (size_t i = 0; i < input_methods->size(); ++i) {
    const chromeos::input_method::InputMethodDescriptor& input_method =
        (*input_methods)[i];
    auto val = std::make_unique<base::DictionaryValue>();
    val->SetString("id", input_method.id());
    val->SetString("name", util->GetInputMethodLongName(input_method));
    val->SetString("indicator", util->GetInputMethodShortName(input_method));
    output->Append(std::move(val));
  }
  return RespondNow(OneArgument(std::move(output)));
#endif
}

ExtensionFunction::ResponseAction
InputMethodPrivateFetchAllDictionaryWordsFunction::Run() {
#if !defined(OS_CHROMEOS)
  EXTENSION_FUNCTION_VALIDATE(false);
#else
  SpellcheckService* spellcheck = SpellcheckServiceFactory::GetForContext(
      context_);
  if (!spellcheck) {
    return RespondNow(Error("Spellcheck service not available."));
  }
  SpellcheckCustomDictionary* dictionary = spellcheck->GetCustomDictionary();
  if (!dictionary->IsLoaded()) {
    return RespondNow(Error("Custom dictionary not loaded yet."));
  }

  const std::set<std::string>& words = dictionary->GetWords();
  std::unique_ptr<base::ListValue> output(new base::ListValue());
  for (auto it = words.begin(); it != words.end(); ++it) {
    output->AppendString(*it);
  }
  return RespondNow(OneArgument(std::move(output)));
#endif
}

ExtensionFunction::ResponseAction
InputMethodPrivateAddWordToDictionaryFunction::Run() {
#if !defined(OS_CHROMEOS)
  EXTENSION_FUNCTION_VALIDATE(false);
#else
  std::unique_ptr<AddWordToDictionary::Params> params(
      AddWordToDictionary::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  SpellcheckService* spellcheck = SpellcheckServiceFactory::GetForContext(
      context_);
  if (!spellcheck) {
    return RespondNow(Error("Spellcheck service not available."));
  }
  SpellcheckCustomDictionary* dictionary = spellcheck->GetCustomDictionary();
  if (!dictionary->IsLoaded()) {
    return RespondNow(Error("Custom dictionary not loaded yet."));
  }

  if (dictionary->AddWord(params->word))
    return RespondNow(NoArguments());
  // Invalid words:
  // - Already in the dictionary.
  // - Not a UTF8 string.
  // - Longer than 99 bytes (kMaxCustomDictionaryWordBytes).
  // - Leading/trailing whitespace.
  // - Empty.
  return RespondNow(Error("Unable to add invalid word to dictionary."));
#endif
}

ExtensionFunction::ResponseAction
InputMethodPrivateGetEncryptSyncEnabledFunction::Run() {
#if !defined(OS_CHROMEOS)
  EXTENSION_FUNCTION_VALIDATE(false);
#else
  browser_sync::ProfileSyncService* profile_sync_service =
      ProfileSyncServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  if (!profile_sync_service)
    return RespondNow(Error("Sync service is not ready for current profile."));
  std::unique_ptr<base::Value> ret(
      new base::Value(profile_sync_service->IsEncryptEverythingEnabled()));
  return RespondNow(OneArgument(std::move(ret)));
#endif
}

ExtensionFunction::ResponseAction
InputMethodPrivateSetXkbLayoutFunction::Run() {
#if !defined(OS_CHROMEOS)
  EXTENSION_FUNCTION_VALIDATE(false);
#else
  std::unique_ptr<SetXkbLayout::Params> params(
      SetXkbLayout::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::Get();
  chromeos::input_method::ImeKeyboard* keyboard = manager->GetImeKeyboard();
  keyboard->SetCurrentKeyboardLayoutByName(params->xkb_name);
  return RespondNow(NoArguments());
#endif
}

ExtensionFunction::ResponseAction
InputMethodPrivateShowInputViewFunction::Run() {
#if !defined(OS_CHROMEOS)
  EXTENSION_FUNCTION_VALIDATE(false);
#else
  auto* keyboard_controller = keyboard::KeyboardController::Get();
  if (keyboard_controller->IsEnabled()) {
    keyboard_controller->ShowKeyboard(false);
    return RespondNow(NoArguments());
  }

  if (keyboard::IsKeyboardEnabled())
    return RespondNow(Error(kErrorFailToShowInputView));

  // Forcibly enables the a11y onscreen keyboard if there is on keyboard enabled
  // for now. And re-disables it after showing once.
  keyboard::SetAccessibilityKeyboardEnabled(true);
  keyboard_controller = keyboard::KeyboardController::Get();
  if (!keyboard_controller->IsEnabled()) {
    keyboard::SetAccessibilityKeyboardEnabled(false);
    return RespondNow(Error(kErrorFailToShowInputView));
  }
  keyboard_controller->ShowKeyboard(false);
  keyboard::SetAccessibilityKeyboardEnabled(false);
  return RespondNow(NoArguments());
#endif
}

ExtensionFunction::ResponseAction
InputMethodPrivateOpenOptionsPageFunction::Run() {
#if !defined(OS_CHROMEOS)
  EXTENSION_FUNCTION_VALIDATE(false);
#else
  std::unique_ptr<OpenOptionsPage::Params> params(
      OpenOptionsPage::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  scoped_refptr<chromeos::input_method::InputMethodManager::State> ime_state =
      chromeos::input_method::InputMethodManager::Get()->GetActiveIMEState();
  const chromeos::input_method::InputMethodDescriptor* ime =
      ime_state->GetInputMethodFromId(params->input_method_id);
  if (!ime)
    return RespondNow(Error("IME not found: *", params->input_method_id));

  content::WebContents* web_contents = GetSenderWebContents();
  if (web_contents) {
    const GURL& options_page_url = ime->options_page_url();
    if (!options_page_url.is_empty()) {
      Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
      content::OpenURLParams url_params(options_page_url, content::Referrer(),
                                        WindowOpenDisposition::SINGLETON_TAB,
                                        ui::PAGE_TRANSITION_LINK, false);
      browser->OpenURL(url_params);
    }
  }
  return RespondNow(NoArguments());
#endif
}

ExtensionFunction::ResponseAction
InputMethodPrivateGetSurroundingTextFunction::Run() {
#if !defined(OS_CHROMEOS)
  EXTENSION_FUNCTION_VALIDATE(false);
#else
  ui::IMEInputContextHandlerInterface* input_context =
      ui::IMEBridge::Get()->GetInputContextHandler();
  if (!input_context)
    return RespondNow(Error("No input context handler."));

  std::unique_ptr<GetSurroundingText::Params> params(
      GetSurroundingText::Params::Create(*args_));
  if (params->before_length < 0 || params->after_length < 0)
    return RespondNow(Error("Invalid parameters."));

  uint32_t param_before_length = (uint32_t)params->before_length;
  uint32_t param_after_length = (uint32_t)params->after_length;

  auto ret = std::make_unique<base::DictionaryValue>();
  ui::SurroundingTextInfo info = input_context->GetSurroundingTextInfo();
  uint32_t text_before_end = info.selection_range.start();
  uint32_t text_before_start = text_before_end > param_before_length
                                   ? text_before_end - param_before_length
                                   : 0;
  uint32_t text_after_start = info.selection_range.end();
  uint32_t text_after_end =
      text_after_start + param_after_length < info.surrounding_text.length()
          ? text_after_start + param_after_length
          : info.surrounding_text.length();

  ret->SetString("before",
                 info.surrounding_text.substr(
                     text_before_start, text_before_end - text_before_start));
  ret->SetString("selected",
                 info.surrounding_text.substr(
                     text_before_end, text_after_start - text_before_end));
  ret->SetString(
      "after", info.surrounding_text.substr(text_after_start,
                                            text_after_end - text_after_start));

  return RespondNow(OneArgument(std::move(ret)));
#endif
}

InputMethodAPI::InputMethodAPI(content::BrowserContext* context)
    : context_(context) {
  EventRouter::Get(context_)->RegisterObserver(this, OnChanged::kEventName);
  EventRouter::Get(context_)
      ->RegisterObserver(this, OnDictionaryChanged::kEventName);
  EventRouter::Get(context_)
      ->RegisterObserver(this, OnDictionaryLoaded::kEventName);
  EventRouter::Get(context_)
      ->RegisterObserver(this, OnImeMenuActivationChanged::kEventName);
  EventRouter::Get(context_)
      ->RegisterObserver(this, OnImeMenuListChanged::kEventName);
  EventRouter::Get(context_)
      ->RegisterObserver(this, OnImeMenuItemsChanged::kEventName);
  ExtensionFunctionRegistry& registry =
      ExtensionFunctionRegistry::GetInstance();
  registry.RegisterFunction<InputMethodPrivateGetInputMethodConfigFunction>();
  registry.RegisterFunction<InputMethodPrivateGetCurrentInputMethodFunction>();
  registry.RegisterFunction<InputMethodPrivateSetCurrentInputMethodFunction>();
  registry.RegisterFunction<InputMethodPrivateGetInputMethodsFunction>();
  registry
      .RegisterFunction<InputMethodPrivateFetchAllDictionaryWordsFunction>();
  registry.RegisterFunction<InputMethodPrivateAddWordToDictionaryFunction>();
  registry.RegisterFunction<InputMethodPrivateGetEncryptSyncEnabledFunction>();
  registry
      .RegisterFunction<InputMethodPrivateNotifyImeMenuItemActivatedFunction>();
  registry.RegisterFunction<InputMethodPrivateOpenOptionsPageFunction>();
}

InputMethodAPI::~InputMethodAPI() {
}

// static
std::string InputMethodAPI::GetInputMethodForXkb(const std::string& xkb_id) {
  std::string xkb_prefix =
      chromeos::extension_ime_util::GetInputMethodIDByEngineID(kXkbPrefix);
  size_t prefix_length = xkb_prefix.length();
  DCHECK(xkb_id.substr(0, prefix_length) == xkb_prefix);
  return xkb_id.substr(prefix_length);
}

void InputMethodAPI::Shutdown() {
  EventRouter::Get(context_)->UnregisterObserver(this);
}

void InputMethodAPI::OnListenerAdded(
    const extensions::EventListenerInfo& details) {
  if (details.event_name == OnChanged::kEventName &&
      !input_method_event_router_.get()) {
    input_method_event_router_.reset(
        new chromeos::ExtensionInputMethodEventRouter(context_));
  } else if (details.event_name == OnDictionaryChanged::kEventName ||
             details.event_name == OnDictionaryLoaded::kEventName) {
    if (!dictionary_event_router_.get()) {
      dictionary_event_router_.reset(
          new chromeos::ExtensionDictionaryEventRouter(context_));
    }
    if (details.event_name == OnDictionaryLoaded::kEventName) {
      dictionary_event_router_->DispatchLoadedEventIfLoaded();
    }
  } else if ((details.event_name == OnImeMenuActivationChanged::kEventName ||
              details.event_name == OnImeMenuListChanged::kEventName ||
              details.event_name == OnImeMenuItemsChanged::kEventName) &&
             !ime_menu_event_router_.get()) {
    ime_menu_event_router_.reset(
        new chromeos::ExtensionImeMenuEventRouter(context_));
  }
}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<InputMethodAPI>>::DestructorAtExit g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<InputMethodAPI>*
InputMethodAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

}  // namespace extensions
