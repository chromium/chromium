// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/input_method_api.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/check_op.h"
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
#include "chrome/browser/ash/extensions/language_packs/language_pack_event_router.h"
#include "chrome/browser/ash/extensions/language_packs/language_packs_extensions_util.h"
#include "chrome/browser/ash/input_method/autocorrect_manager.h"
#include "chrome/browser/ash/input_method/native_input_method_engine.h"
#include "chrome/browser/extensions/api/input_ime/input_ime_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/api/input_method_private.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/language_packs/handwriting.h"
#include "chromeos/ash/components/language_packs/language_pack_manager.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
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
#include "ui/base/window_open_disposition.h"

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
namespace GetLanguagePackStatus =
    extensions::api::input_method_private::GetLanguagePackStatus;
namespace OnLanguagePackStatusChanged =
    extensions::api::input_method_private::OnLanguagePackStatusChanged;

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
  std::optional<SetCurrentInputMethod::Params> params =
      SetCurrentInputMethod::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
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
  std::optional<AddWordToDictionary::Params> params =
      AddWordToDictionary::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
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
  std::optional<SetXkbLayout::Params> params =
      SetXkbLayout::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  auto* manager = ash::input_method::InputMethodManager::Get();
  ash::input_method::ImeKeyboard* keyboard = manager->GetImeKeyboard();
  keyboard->SetCurrentKeyboardLayoutByName(params->xkb_name, base::DoNothing());
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
  std::optional<OpenOptionsPage::Params> params =
      OpenOptionsPage::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
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
    content::WebContents* web_contents = GetSenderWebContents();
    if (web_contents) {
      Browser* browser = chrome::FindBrowserWithTab(web_contents);
      content::OpenURLParams url_params(options_page_url, content::Referrer(),
                                        WindowOpenDisposition::SINGLETON_TAB,
                                        ui::PAGE_TRANSITION_LINK, false);
      browser->OpenURL(url_params, /*navigation_handle_callback=*/{});
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

  std::optional<GetSurroundingText::Params> params =
      GetSurroundingText::Params::Create(args());
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
  EXTENSION_FUNCTION_VALIDATE(params);

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
  EXTENSION_FUNCTION_VALIDATE(params);

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
        case input_method_private::UnderlineStyle::kUnderline:
          segment_info.style = InputMethodEngine::SEGMENT_STYLE_UNDERLINE;
          break;
        case input_method_private::UnderlineStyle::kDoubleUnderline:
          segment_info.style =
              InputMethodEngine::SEGMENT_STYLE_DOUBLE_UNDERLINE;
          break;
        case input_method_private::UnderlineStyle::kNoUnderline:
          segment_info.style = InputMethodEngine::SEGMENT_STYLE_NO_UNDERLINE;
          break;
        case input_method_private::UnderlineStyle::kNone:
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
  std::optional<OnAutocorrect::Params> parent_params =
      OnAutocorrect::Params::Create(args());
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

ExtensionFunction::ResponseAction
InputMethodPrivateGetLanguagePackStatusFunction::Run() {
  std::optional<GetLanguagePackStatus::Params> params =
      GetLanguagePackStatus::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  // This currently only handles handwriting, but this should (in theory)
  // handle a collection of language packs once input methods depend on multiple
  // language packs.
  auto* manager = ash::input_method::InputMethodManager::Get();

  std::optional<std::string> handwriting_locale =
      ash::language_packs::MapInputMethodIdToHandwritingLocale(
          manager->GetInputMethodUtil(), params->input_method_id);
  // If there are no language packs associated with an input method, installed
  // is returned.
  if (!handwriting_locale.has_value()) {
    return RespondNow(WithArguments(
        ToString(input_method_private::LanguagePackStatus::kInstalled)));
  }
  if (!ash::language_packs::HandwritingLocaleToDlc(*handwriting_locale)
           .has_value()) {
    // We obtained a handwriting locale, but it doesn't have an associated
    // language pack. This means that there are no language packs associated
    // with this input method.
    //
    // "en" is the only handwriting locale which does not have an associated
    // language pack (as of writing).
    if (*handwriting_locale != "en") {
      LOG(DFATAL) << "Got non-English handwriting locale from manifest which "
                     "does not have DLC: "
                  << *handwriting_locale;
    }
    return RespondNow(WithArguments(
        ToString(input_method_private::LanguagePackStatus::kInstalled)));
  }

  ash::language_packs::LanguagePackManager::GetPackState(
      ash::language_packs::kHandwritingFeatureId, *handwriting_locale,
      // This `BindOnce` into a `.Then` is required to avoid having a method on
      // this class which has a language pack type in its function signature,
      // which would cause language packs to be included in this file's headers,
      // which would cause a slew of dependency issues.
      base::BindOnce(&chromeos::LanguagePackResultToExtensionStatus)
          .Then(
              base::BindOnce(&InputMethodPrivateGetLanguagePackStatusFunction::
                                 OnGetLanguagePackStatusComplete,
                             this)));
  return RespondLater();
}

void InputMethodPrivateGetLanguagePackStatusFunction::
    OnGetLanguagePackStatusComplete(
        const input_method_private::LanguagePackStatus status) {
  base::Value::List results =
      input_method_private::GetLanguagePackStatus::Results::Create(status);
  Respond(ArgumentList(std::move(results)));
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
  EventRouter::Get(context_)->RegisterObserver(
      this, OnLanguagePackStatusChanged::kEventName);
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
  } else if (details.event_name == OnLanguagePackStatusChanged::kEventName &&
             !language_pack_event_router_.get()) {
    language_pack_event_router_ =
        std::make_unique<chromeos::LanguagePackEventRouter>(context_);
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
