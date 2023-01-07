// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/input_method_api.h"

#include <stddef.h>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/extensions/dictionary_event_router.h"
#include "chrome/browser/ash/extensions/ime_menu_event_router.h"
#include "chrome/browser/ash/extensions/input_method_event_router.h"
#include "chrome/browser/ash/input_method/autocorrect_manager.h"
#include "chrome/browser/ash/input_method/native_input_method_engine.h"
#include "chrome/browser/extensions/api/input_ime/input_ime_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/common/extensions/api/input_method_private.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_system.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/ime_keyboard.h"
#include "ui/base/ime/ash/input_method_descriptor.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/input_method_util.h"
#include "ui/display/screen.h"

namespace {

namespace input_method_private = extensions::api::input_method_private;
namespace AddWordToDictionary =
    extensions::api::input_method_private::AddWordToDictionary;
namespace SetCurrentInputMethod =
    extensions::api::input_method_private::SetCurrentInputMethod;
namespace SwitchToLastUsedInputMethod =
    extensions::api::input_method_private::SwitchToLastUsedInputMethod;
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
namespace GetSettings = extensions::api::input_method_private::GetSettings;
namespace SetSettings = extensions::api::input_method_private::SetSettings;
namespace SetCompositionRange =
    extensions::api::input_method_private::SetCompositionRange;
namespace OnInputMethodOptionsChanged =
    extensions::api::input_method_private::OnInputMethodOptionsChanged;
namespace OnAutocorrect = extensions::api::input_method_private::OnAutocorrect;
namespace GetTextFieldBounds =
    extensions::api::input_method_private::GetTextFieldBounds;

using ::ash::input_method::InputMethodEngine;

// Prefix, which is used by XKB.
const char kXkbPrefix[] = "xkb:";
const char kErrorFailToShowInputView[] =
    "Unable to show the input view window because the keyboard is not enabled.";
const char kErrorFailToHideInputView[] =
    "Unable to hide the input view window because the keyboard is not enabled.";
const char kErrorRouterNotAvailable[] = "The router is not available.";
const char kErrorInvalidInputMethod[] = "Input method not found.";
const char kErrorSpellCheckNotAvailable[] =
    "Spellcheck service is not available.";
const char kErrorCustomDictionaryNotLoaded[] =
    "Custom dictionary is not loaded yet.";
const char kErrorInvalidWord[] = "Unable to add invalid word to dictionary.";
const char kErrorInputContextHandlerNotAvailable[] =
    "Input context handler is not available.";
const char kErrorInvalidParametersForGetSurroundingText[] =
    "Invalid negative parameters for GetSurroundingText.";

InputMethodEngine* GetEngineIfActive(content::BrowserContext* browser_context,
                                     const std::string& extension_id,
                                     std::string* error) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  extensions::InputImeEventRouter* event_router =
      extensions::GetInputImeEventRouter(profile);
  DCHECK(event_router) << kErrorRouterNotAvailable;
  InputMethodEngine* engine =
      event_router->GetEngineIfActive(extension_id, error);
  return engine;
}

}  // namespace

namespace extensions {

ExtensionFunction::ResponseAction
InputMethodPrivateGetInputMethodConfigFunction::Run() {
  base::Value::Dict output;
  output.Set("isPhysicalKeyboardAutocorrectEnabled", true);
  output.Set("isImeMenuActivated",
             Profile::FromBrowserContext(browser_context())
                 ->GetPrefs()
                 ->GetBoolean(prefs::kLanguageImeMenuActivated));
  return RespondNow(WithArguments(std::move(output)));
}

ExtensionFunction::ResponseAction
InputMethodPrivateGetCurrentInputMethodFunction::Run() {
  auto* manager = ash::input_method::InputMethodManager::Get();
  return RespondNow(WithArguments(
      manager->GetActiveIMEState()->GetCurrentInputMethod().id()));
}

ExtensionFunction::ResponseAction
InputMethodPrivateSetCurrentInputMethodFunction::Run() {
  std::unique_ptr<SetCurrentInputMethod::Params> params(
      SetCurrentInputMethod::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  scoped_refptr<ash::input_method::InputMethodManager::State> ime_state =
      ash::input_method::InputMethodManager::Get()->GetActiveIMEState();
  const std::vector<std::string>& input_methods =
      ime_state->GetEnabledInputMethodIds();
  for (const auto& input_method : input_methods) {
    if (input_method == params->input_method_id) {
      ime_state->ChangeInputMethod(params->input_method_id,
                                   false /* show_message */);
      return RespondNow(NoArguments());
    }
  }
  return RespondNow(Error(InformativeError(
      base::StringPrintf("%s Input Method: %s", kErrorInvalidInputMethod,
                         params->input_method_id.c_str()),
      static_function_name())));
}

ExtensionFunction::ResponseAction
InputMethodPrivateSwitchToLastUsedInputMethodFunction::Run() {
  scoped_refptr<ash::input_method::InputMethodManager::State> ime_state =
      ash::input_method::InputMethodManager::Get()->GetActiveIMEState();
  ime_state->SwitchToLastUsedInputMethod();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
InputMethodPrivateGetInputMethodsFunction::Run() {
  base::Value::List output;
  auto* manager = ash::input_method::InputMethodManager::Get();
  ash::input_method::InputMethodUtil* util = manager->GetInputMethodUtil();
  scoped_refptr<ash::input_method::InputMethodManager::State> ime_state =
      manager->GetActiveIMEState();
  ash::input_method::InputMethodDescriptors input_methods =
      ime_state->GetEnabledInputMethodsSortedByLocalizedDisplayNames();
  for (size_t i = 0; i < input_methods.size(); ++i) {
    const ash::input_method::InputMethodDescriptor& input_method =
        input_methods[i];
    base::Value::Dict val;
    val.Set("id", input_method.id());
    val.Set("name", util->GetInputMethodLongName(input_method));
    val.Set("indicator", input_method.GetIndicator());
    output.Append(std::move(val));
  }
  return RespondNow(WithArguments(std::move(output)));
}

ExtensionFunction::ResponseAction
InputMethodPrivateFetchAllDictionaryWordsFunction::Run() {
  SpellcheckService* spellcheck =
      SpellcheckServiceFactory::GetForContext(browser_context());
  if (!spellcheck) {
    return RespondNow(Error(InformativeError(kErrorSpellCheckNotAvailable,
                                             static_function_name())));
  }
  SpellcheckCustomDictionary* dictionary = spellcheck->GetCustomDictionary();
  if (!dictionary->IsLoaded()) {
    return RespondNow(Error(InformativeError(kErrorCustomDictionaryNotLoaded,
                                             static_function_name())));
  }

  const std::set<std::string>& words = dictionary->GetWords();
  base::Value::List output;
  for (const auto& word : words) {
    output.Append(word);
  }
  return RespondNow(WithArguments(std::move(output)));
}

ExtensionFunction::ResponseAction
InputMethodPrivateAddWordToDictionaryFunction::Run() {
  std::unique_ptr<AddWordToDictionary::Params> params(
      AddWordToDictionary::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  SpellcheckService* spellcheck =
      SpellcheckServiceFactory::GetForContext(browser_context());
  if (!spellcheck) {
    return RespondNow(Error(InformativeError(kErrorSpellCheckNotAvailable,
                                             static_function_name())));
  }
  SpellcheckCustomDictionary* dictionary = spellcheck->GetCustomDictionary();
  if (!dictionary->IsLoaded()) {
    return RespondNow(Error(InformativeError(kErrorCustomDictionaryNotLoaded,
                                             static_function_name())));
  }

  if (dictionary->AddWord(params->word))
    return RespondNow(NoArguments());
  // Invalid words:
  // - Already in the dictionary.
  // - Not a UTF8 string.
  // - Longer than 99 bytes (kMaxCustomDictionaryWordBytes).
  // - Leading/trailing whitespace.
  // - Empty.
  return RespondNow(Error(
      InformativeError(base::StringPrintf("%s. Word: %s", kErrorInvalidWord,
                                          params->word.c_str()),
                       static_function_name())));
}

ExtensionFunction::ResponseAction
InputMethodPrivateSetXkbLayoutFunction::Run() {
  std::unique_ptr<SetXkbLayout::Params> params(
      SetXkbLayout::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  auto* manager = ash::input_method::InputMethodManager::Get();
  ash::input_method::ImeKeyboard* keyboard = manager->GetImeKeyboard();
  keyboard->SetCurrentKeyboardLayoutByName(params->xkb_name);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
InputMethodPrivateShowInputViewFunction::Run() {
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  if (!keyboard_client->is_keyboard_enabled()) {
    return RespondNow(Error(kErrorFailToShowInputView));
  }

  keyboard_client->ShowKeyboard();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
InputMethodPrivateHideInputViewFunction::Run() {
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  if (!keyboard_client->is_keyboard_enabled()) {
    return RespondNow(Error(kErrorFailToHideInputView));
  }

  keyboard_client->HideKeyboard(ash::HideReason::kUser);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
InputMethodPrivateOpenOptionsPageFunction::Run() {
  std::unique_ptr<OpenOptionsPage::Params> params(
      OpenOptionsPage::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  scoped_refptr<ash::input_method::InputMethodManager::State> ime_state =
      ash::input_method::InputMethodManager::Get()->GetActiveIMEState();
  const ash::input_method::InputMethodDescriptor* ime =
      ime_state->GetInputMethodFromId(params->input_method_id);
  if (!ime)
    return RespondNow(Error(InformativeError(
        base::StringPrintf("%s Input Method: %s", kErrorInvalidInputMethod,
                           params->input_method_id.c_str()),
        static_function_name())));

  const GURL& options_page_url = ime->options_page_url();
  if (!options_page_url.is_empty()) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // If Lacros is the only browser, open the options page in an Ash app window
    // instead of a regular Ash browser window.
    if (!crosapi::browser_util::IsAshWebBrowserEnabled()) {
      auto* profile = ProfileManager::GetPrimaryUserProfile();
      ash::SystemAppLaunchParams launch_params;
      launch_params.url = options_page_url;
      int64_t display_id =
          display::Screen::GetScreen()->GetDisplayForNewWindows().id();
      ash::LaunchSystemWebAppAsync(
          profile, ash::SystemWebAppType::OS_URL_HANDLER, launch_params,
          std::make_unique<apps::WindowInfo>(display_id));
      return RespondNow(NoArguments());
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    content::WebContents* web_contents = GetSenderWebContents();
    if (web_contents) {
      Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
      content::OpenURLParams url_params(options_page_url, content::Referrer(),
                                        WindowOpenDisposition::SINGLETON_TAB,
                                        ui::PAGE_TRANSITION_LINK, false);
      browser->OpenURL(url_params);
    }
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
InputMethodPrivateGetSurroundingTextFunction::Run() {
  ash::TextInputTarget* input_context =
      ash::IMEBridge::Get()->GetInputContextHandler();
  if (!input_context)
    return RespondNow(Error(InformativeError(
        kErrorInputContextHandlerNotAvailable, static_function_name())));

  std::unique_ptr<GetSurroundingText::Params> params(
      GetSurroundingText::Params::Create(args()));
  if (params->before_length < 0 || params->after_length < 0)
    return RespondNow(Error(InformativeError(
        base::StringPrintf("%s before_length = %d, after_length = %d.",
                           kErrorInvalidParametersForGetSurroundingText,
                           params->before_length, params->after_length),
        static_function_name())));

  uint32_t param_before_length = (uint32_t)params->before_length;
  uint32_t param_after_length = (uint32_t)params->after_length;

  ash::SurroundingTextInfo info = input_context->GetSurroundingTextInfo();
  if (!info.selection_range.IsValid())
    return RespondNow(WithArguments(base::Value()));

  base::Value::Dict ret;
  uint32_t selection_start = info.selection_range.start();
  uint32_t selection_end = info.selection_range.end();
  // Makes sure |selection_start| is less or equals to |selection_end|.
  if (selection_start > selection_end)
    std::swap(selection_start, selection_end);

  uint32_t text_before_end = selection_start;
  uint32_t text_before_start = text_before_end > param_before_length
                                   ? text_before_end - param_before_length
                                   : 0;
  uint32_t text_after_start = selection_end;
  uint32_t text_after_end =
      text_after_start + param_after_length < info.surrounding_text.length()
          ? text_after_start + param_after_length
          : info.surrounding_text.length();

  ret.Set("before",
          info.surrounding_text.substr(text_before_start,
                                       text_before_end - text_before_start));
  ret.Set("selected", info.surrounding_text.substr(
                          text_before_end, text_after_start - text_before_end));
  ret.Set("after", info.surrounding_text.substr(
                       text_after_start, text_after_end - text_after_start));

  return RespondNow(WithArguments(std::move(ret)));
}

ExtensionFunction::ResponseAction InputMethodPrivateGetSettingsFunction::Run() {
  const auto params = GetSettings::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const base::Value::Dict& input_methods =
      Profile::FromBrowserContext(browser_context())
          ->GetPrefs()
          ->GetDict(prefs::kLanguageInputMethodSpecificSettings);
  const base::Value* engine_result =
      input_methods.FindByDottedPath(params->engine_id);
  base::Value result;
  if (engine_result)
    result = engine_result->Clone();
  return RespondNow(WithArguments(std::move(result)));
}

ExtensionFunction::ResponseAction InputMethodPrivateSetSettingsFunction::Run() {
  const auto params = SetSettings::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params.get());

  ScopedDictPrefUpdate update(
      Profile::FromBrowserContext(browser_context())->GetPrefs(),
      prefs::kLanguageInputMethodSpecificSettings);
  update->SetByDottedPath(params->engine_id, params->settings.ToValue());

  // The router will only send the event to extensions that are listening.
  extensions::EventRouter* router =
      extensions::EventRouter::Get(browser_context());
  if (router->HasEventListener(OnInputMethodOptionsChanged::kEventName)) {
    auto event = std::make_unique<extensions::Event>(
        extensions::events::INPUT_IME_ON_INPUT_METHOD_OPTIONS_CHANGED,
        OnInputMethodOptionsChanged::kEventName,
        OnInputMethodOptionsChanged::Create(params->engine_id),
        browser_context());
    router->BroadcastEvent(std::move(event));
  }

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
InputMethodPrivateSetCompositionRangeFunction::Run() {
  std::string error;
  InputMethodEngine* engine =
      GetEngineIfActive(browser_context(), extension_id(), &error);
  if (!engine)
    return RespondNow(Error(InformativeError(error, static_function_name())));

  const auto parent_params = SetCompositionRange::Params::Create(args());
  const auto& params = parent_params->parameters;
  std::vector<InputMethodEngine::SegmentInfo> segments;
  if (params.segments) {
    for (const auto& segments_arg : *params.segments) {
      InputMethodEngine::SegmentInfo segment_info;
      segment_info.start = segments_arg.start;
      segment_info.end = segments_arg.end;
      switch (segments_arg.style) {
        case input_method_private::UNDERLINE_STYLE_UNDERLINE:
          segment_info.style = InputMethodEngine::SEGMENT_STYLE_UNDERLINE;
          break;
        case input_method_private::UNDERLINE_STYLE_DOUBLEUNDERLINE:
          segment_info.style =
              InputMethodEngine::SEGMENT_STYLE_DOUBLE_UNDERLINE;
          break;
        case input_method_private::UNDERLINE_STYLE_NOUNDERLINE:
          segment_info.style = InputMethodEngine::SEGMENT_STYLE_NO_UNDERLINE;
          break;
        case input_method_private::UNDERLINE_STYLE_NONE:
          EXTENSION_FUNCTION_VALIDATE(false);
          break;
      }
      segments.push_back(segment_info);
    }
  }

  if (!engine->InputMethodEngine::SetCompositionRange(
          params.context_id, params.selection_before, params.selection_after,
          segments, &error)) {
    return RespondNow(Error(InformativeError(error, static_function_name())));
  }
  return RespondNow(WithArguments(base::Value(true)));
}

ExtensionFunction::ResponseAction
InputMethodPrivateGetTextFieldBoundsFunction::Run() {
  std::string error;
  InputMethodEngine* engine =
      GetEngineIfActive(browser_context(), extension_id(), &error);
  if (!engine)
    return RespondNow(Error(InformativeError(error, static_function_name())));

  const auto parent_params = GetTextFieldBounds::Params::Create(args());
  const auto& params = parent_params->parameters;
  const gfx::Rect rect =
      engine->InputMethodEngine::GetTextFieldBounds(params.context_id, &error);
  if (rect.IsEmpty()) {
    return RespondNow(Error(InformativeError(error, static_function_name())));
  }
  base::Value::Dict ret;
  ret.Set("x", rect.x());
  ret.Set("y", rect.y());
  ret.Set("width", rect.width());
  ret.Set("height", rect.height());
  return RespondNow(WithArguments(std::move(ret)));
}

ExtensionFunction::ResponseAction InputMethodPrivateResetFunction::Run() {
  std::string error;
  InputMethodEngine* engine =
      GetEngineIfActive(browser_context(), extension_id(), &error);
  if (!engine)
    return RespondNow(Error(InformativeError(error, static_function_name())));

  engine->Reset();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
InputMethodPrivateOnAutocorrectFunction::Run() {
  std::unique_ptr<OnAutocorrect::Params> parent_params(
      OnAutocorrect::Params::Create(args()));
  const OnAutocorrect::Params::Parameters& params = parent_params->parameters;
  std::string error;
  ash::input_method::NativeInputMethodEngine* engine =
      static_cast<ash::input_method::NativeInputMethodEngine*>(
          GetEngineIfActive(Profile::FromBrowserContext(browser_context()),
                            extension_id(), &error));
  if (!engine)
    return RespondNow(Error(InformativeError(error, static_function_name())));

  // `typed_word` and `corrected_word` are both originally encoded in UTF-16 by
  // JavaScript, but the extensions bindings will convert them to UTF-8 without
  // changing `start_index` (which is in UTF-16 code units). Hence, convert the
  // two strngs back without changing `start_index`.
  engine->OnAutocorrect(base::UTF8ToUTF16(params.typed_word),
                        base::UTF8ToUTF16(params.corrected_word),
                        params.start_index);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
InputMethodPrivateNotifyInputMethodReadyForTestingFunction::Run() {
  std::string error;
  ash::input_method::InputMethodEngine* engine = GetEngineIfActive(
      Profile::FromBrowserContext(browser_context()), extension_id(), &error);
  if (!engine)
    return RespondNow(Error(InformativeError(error, static_function_name())));

  engine->NotifyInputMethodExtensionReadyForTesting();  // IN-TEST
  return RespondNow(NoArguments());
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
  registry.RegisterFunction<InputMethodPrivateOpenOptionsPageFunction>();
}

InputMethodAPI::~InputMethodAPI() = default;

// static
std::string InputMethodAPI::GetInputMethodForXkb(const std::string& xkb_id) {
  std::string xkb_prefix =
      ash::extension_ime_util::GetInputMethodIDByEngineID(kXkbPrefix);
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
    input_method_event_router_ =
        std::make_unique<chromeos::ExtensionInputMethodEventRouter>(context_);
  } else if (details.event_name == OnDictionaryChanged::kEventName ||
             details.event_name == OnDictionaryLoaded::kEventName) {
    if (!dictionary_event_router_.get()) {
      dictionary_event_router_ =
          std::make_unique<chromeos::ExtensionDictionaryEventRouter>(context_);
    }
    if (details.event_name == OnDictionaryLoaded::kEventName) {
      dictionary_event_router_->DispatchLoadedEventIfLoaded();
    }
  } else if ((details.event_name == OnImeMenuActivationChanged::kEventName ||
              details.event_name == OnImeMenuListChanged::kEventName ||
              details.event_name == OnImeMenuItemsChanged::kEventName) &&
             !ime_menu_event_router_.get()) {
    ime_menu_event_router_ =
        std::make_unique<chromeos::ExtensionImeMenuEventRouter>(context_);
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
