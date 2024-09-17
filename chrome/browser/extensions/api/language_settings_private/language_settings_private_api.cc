// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/language_settings_private/language_settings_private_api.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/language_settings_private/language_settings_private_delegate.h"
#include "chrome/browser/extensions/api/language_settings_private/language_settings_private_delegate_factory.h"
#include "chrome/browser/language/language_model_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/common/extensions/api/language_settings_private.h"
#include "chrome/common/pref_names.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/language_util.h"
#include "components/language/core/common/locale_util.h"
#include "components/spellcheck/browser/spellcheck_platform.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "extensions/browser/extensions_browser_client.h"
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_collator.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/ime/ash/component_extension_ime_manager.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/input_method_descriptor.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/input_method_util.h"
#endif

namespace extensions {

namespace language_settings_private = api::language_settings_private;

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
using ::ash::input_method::InputMethodDescriptor;
using ::ash::input_method::InputMethodDescriptors;
using ::ash::input_method::InputMethodManager;
using ::ash::input_method::InputMethodUtil;

// Number of IMEs that are needed to automatically enable the IME menu option.
const size_t kNumImesToAutoEnableImeMenu = 2;

// Returns the set of IDs of enabled IMEs for the given pref.
base::flat_set<std::string> GetIMEsFromPref(PrefService* prefs,
                                            const char* pref_name) {
  return base::SplitString(prefs->GetString(pref_name), ",",
                           base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
}

// Returns the set of allowed UI locales.
base::flat_set<std::string> GetAllowedLanguages(PrefService* prefs) {
  const auto& allowed_languages_values =
      prefs->GetList(prefs::kAllowedLanguages);
  return base::MakeFlatSet<std::string>(
      allowed_languages_values, {},
      [](const auto& locale_value) { return locale_value.GetString(); });
}

// Sorts the input methods by the order of their associated languages. For
// example, if the enabled language order is:
//  - French
//  - English
//  then the French keyboard should come before the English keyboard.
std::vector<std::string> GetSortedComponentIMEs(
    InputMethodManager* manager,
    scoped_refptr<InputMethodManager::State> ime_state,
    const base::flat_set<std::string>& component_ime_set,
    PrefService* prefs) {
  std::vector<std::string> enabled_languages =
      base::SplitString(prefs->GetString(language::prefs::kPreferredLanguages),
                        ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Duplicate set for membership testing.
  std::set<std::string> available_component_imes(component_ime_set.begin(),
                                                 component_ime_set.end());
  std::vector<std::string> component_ime_list;

  for (const auto& language_code : enabled_languages) {
    // Get all input methods for this language.
    std::vector<std::string> input_method_ids;
    manager->GetInputMethodUtil()->GetInputMethodIdsFromLanguageCode(
        language_code, ash::input_method::kAllInputMethods, &input_method_ids);
    // Append the enabled ones to the new list. Also remove them from the set
    // so they aren't duplicated for other languages.
    for (const auto& input_method_id : input_method_ids) {
      if (base::Contains(available_component_imes, input_method_id)) {
        component_ime_list.push_back(input_method_id);
        available_component_imes.erase(input_method_id);
      }
    }
  }
  for (const auto& input_method_id : available_component_imes) {
    component_ime_list.push_back(input_method_id);
  }

  return component_ime_list;
}

// Sorts the third-party IMEs by the order of their associated languages.
std::vector<std::string> GetSortedThirdPartyIMEs(
    scoped_refptr<InputMethodManager::State> ime_state,
    const base::flat_set<std::string>& third_party_ime_set,
    PrefService* prefs) {
  std::vector<std::string> ime_list;
  std::string preferred_languages =
      prefs->GetString(language::prefs::kPreferredLanguages);
  std::vector<std::string_view> enabled_languages =
      base::SplitStringPiece(preferred_languages, ",", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);

  // Add the fake language for ARC IMEs at the very last of the list. Unlike
  // Chrome OS IMEs, these ARC ones are not associated with any (real) language.
  enabled_languages.push_back(ash::extension_ime_util::kArcImeLanguage);

  InputMethodDescriptors descriptors;
  ime_state->GetInputMethodExtensions(&descriptors);

  // Filter out the IMEs not in |third_party_ime_set|.
  std::erase_if(descriptors, [&third_party_ime_set](
                                 const InputMethodDescriptor& descriptor) {
    return !third_party_ime_set.contains(descriptor.id());
  });

  // A set of the elements of |ime_list|.
  std::set<std::string> ime_set;

  // For each language, add any candidate IMEs that support it.
  for (const auto& language : enabled_languages) {
    for (const InputMethodDescriptor& descriptor : descriptors) {
      const std::string& id = descriptor.id();
      if (!base::Contains(ime_set, id) &&
          base::Contains(descriptor.language_codes(), language)) {
        ime_list.push_back(id);
        ime_set.insert(id);
      }
    }
  }

  // Add the rest of the third party IMEs
  for (const InputMethodDescriptor& descriptor : descriptors) {
    const std::string& id = descriptor.id();
    if (!base::Contains(ime_set, id)) {
      ime_list.push_back(id);
    }
  }

  return ime_list;
}

std::vector<std::string> GetInputMethodTags(
    language_settings_private::InputMethod* input_method) {
  std::vector<std::string> tags = {input_method->display_name};
  const std::string app_locale =
      ExtensionsBrowserClient::Get()->GetApplicationLocale();
  for (const auto& language_code : input_method->language_codes) {
    tags.push_back(base::UTF16ToUTF8(l10n_util::GetDisplayNameForLocale(
        language_code, app_locale, /*is_for_ui=*/true)));
  }
  return tags;
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

std::unique_ptr<translate::TranslatePrefs>
CreateTranslatePrefsForBrowserContext(
    content::BrowserContext* browser_context) {
  return ChromeTranslateClient::CreateTranslatePrefs(
      Profile::FromBrowserContext(browser_context)->GetPrefs());
}

}  // namespace

LanguageSettingsPrivateGetLanguageListFunction::
    LanguageSettingsPrivateGetLanguageListFunction() = default;

LanguageSettingsPrivateGetLanguageListFunction::
    ~LanguageSettingsPrivateGetLanguageListFunction() = default;

ExtensionFunction::ResponseAction
LanguageSettingsPrivateGetLanguageListFunction::Run() {
  // Collect the language codes from the supported accept-languages.
  const std::string app_locale =
      ExtensionsBrowserClient::Get()->GetApplicationLocale();
  const std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      CreateTranslatePrefsForBrowserContext(browser_context());

  std::vector<translate::TranslateLanguageInfo> languages;
  translate::TranslatePrefs::GetLanguageInfoList(
      app_locale, translate_prefs->IsTranslateAllowedByPolicy(), &languages);

  // Get the list of spell check languages and convert to a set.
  std::vector<std::string> spellcheck_languages =
      spellcheck::SpellCheckLanguages();
  const base::flat_set<std::string> spellcheck_language_set(
      std::move(spellcheck_languages));

  // Build the language list.
  language_list_.clear();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const base::flat_set<std::string> allowed_ui_locales(GetAllowedLanguages(
      Profile::FromBrowserContext(browser_context())->GetPrefs()));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  for (const auto& entry : languages) {
    language_settings_private::Language language;

    language.code = entry.code;
    language.display_name = entry.display_name;
    language.native_display_name = entry.native_display_name;

    // Set optional fields only if they differ from the default.
    if (base::Contains(spellcheck_language_set, entry.code)) {
      language.supports_spellcheck = true;
    }
    if (entry.supports_translate) {
      language.supports_translate = true;
    }

    if (l10n_util::IsUserFacingUILocale(entry.code)) {
      language.supports_ui = true;
    }
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (!allowed_ui_locales.empty() &&
        !base::Contains(allowed_ui_locales, language.code)) {
      language.is_prohibited_language = true;
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    language_list_.Append(language.ToValue());
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Send the display name of the fake language for ARC IMEs to the JS side.
  // |native_display_name| does't have to be set because the language selection
  // drop-down menu doesn't list the fake language.
  {
    language_settings_private::Language language;
    language.code = ash::extension_ime_util::kArcImeLanguage;
    language.display_name =
        l10n_util::GetStringUTF8(IDS_SETTINGS_LANGUAGES_KEYBOARD_APPS);
    language_list_.Append(base::Value(language.ToValue()));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
  if (spellcheck::UseBrowserSpellChecker()) {
    if (!base::FeatureList::IsEnabled(
            spellcheck::kWinDelaySpellcheckServiceInit)) {
      // Platform dictionary support already determined at browser startup.
      UpdateSupportedPlatformDictionaries();
    } else {
      // Asynchronously load the dictionaries to determine platform support.
      SpellcheckService* service =
          SpellcheckServiceFactory::GetForContext(browser_context());
      AddRef();  // Balanced in OnDictionariesInitialized
      service->InitializeDictionaries(
          base::BindOnce(&LanguageSettingsPrivateGetLanguageListFunction::
                             OnDictionariesInitialized,
                         base::Unretained(this)));
      return RespondLater();
    }
  }
#endif  // BUILDFLAG(IS_WIN)

  return RespondNow(WithArguments(std::move(language_list_)));
}

#if BUILDFLAG(IS_WIN)
void LanguageSettingsPrivateGetLanguageListFunction::
    OnDictionariesInitialized() {
  UpdateSupportedPlatformDictionaries();
  Respond(WithArguments(std::move(language_list_)));
  // Matches the AddRef in Run().
  Release();
}

void LanguageSettingsPrivateGetLanguageListFunction::
    UpdateSupportedPlatformDictionaries() {
  SpellcheckService* service =
      SpellcheckServiceFactory::GetForContext(browser_context());
  for (auto& language_val : language_list_) {
    base::Value::Dict& language_val_dict = language_val.GetDict();
    const std::string* str = language_val_dict.FindString("code");
    if (str && service->UsesWindowsDictionary(*str)) {
      language_val_dict.Set("supportsSpellcheck", true);
    }
  }
}
#endif  // BUILDFLAG(IS_WIN)

LanguageSettingsPrivateEnableLanguageFunction::
    LanguageSettingsPrivateEnableLanguageFunction() = default;

LanguageSettingsPrivateEnableLanguageFunction::
    ~LanguageSettingsPrivateEnableLanguageFunction() = default;

ExtensionFunction::ResponseAction
LanguageSettingsPrivateEnableLanguageFunction::Run() {
  const auto parameters =
      language_settings_private::EnableLanguage::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);
  const std::string& language_code = parameters->language_code;

  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      CreateTranslatePrefsForBrowserContext(browser_context());

  std::vector<std::string> languages;
  translate_prefs->GetLanguageList(&languages);
  std::string chrome_language = language_code;
  language::ToChromeLanguageSynonym(&chrome_language);

  translate_prefs->AddToLanguageList(language_code, /*force_blocked=*/false);

  return RespondNow(NoArguments());
}

LanguageSettingsPrivateDisableLanguageFunction::
    LanguageSettingsPrivateDisableLanguageFunction() = default;

LanguageSettingsPrivateDisableLanguageFunction::
    ~LanguageSettingsPrivateDisableLanguageFunction() = default;

ExtensionFunction::ResponseAction
LanguageSettingsPrivateDisableLanguageFunction::Run() {
  const auto parameters =
      language_settings_private::DisableLanguage::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);
  const std::string& language_code = parameters->language_code;

  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      CreateTranslatePrefsForBrowserContext(browser_context());

  std::vector<std::string> languages;
  translate_prefs->GetLanguageList(&languages);
  std::string chrome_language = language_code;
  language::ToChromeLanguageSynonym(&chrome_language);

  translate_prefs->RemoveFromLanguageList(language_code);

  return RespondNow(NoArguments());
}

LanguageSettingsPrivateSetEnableTranslationForLanguageFunction::
    LanguageSettingsPrivateSetEnableTranslationForLanguageFunction() = default;

LanguageSettingsPrivateSetEnableTranslationForLanguageFunction::
    ~LanguageSettingsPrivateSetEnableTranslationForLanguageFunction() = default;

ExtensionFunction::ResponseAction
LanguageSettingsPrivateSetEnableTranslationForLanguageFunction::Run() {
  const auto parameters = language_settings_private::
      SetEnableTranslationForLanguage::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);
  const std::string& language_code = parameters->language_code;
  // True if translation enabled, false if disabled.
  const bool enable = parameters->enable;

  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      CreateTranslatePrefsForBrowserContext(browser_context());

  if (enable) {
    translate_prefs->UnblockLanguage(language_code);
  } else {
    translate_prefs->BlockLanguage(language_code);
  }

  return RespondNow(NoArguments());
}

LanguageSettingsPrivateGetAlwaysTranslateLanguagesFunction::
    LanguageSettingsPrivateGetAlwaysTranslateLanguagesFunction() = default;

LanguageSettingsPrivateGetAlwaysTranslateLanguagesFunction::
    ~LanguageSettingsPrivateGetAlwaysTranslateLanguagesFunction() = default;

ExtensionFunction::ResponseAction
LanguageSettingsPrivateGetAlwaysTranslateLanguagesFunction::Run() {
  const std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      CreateTranslatePrefsForBrowserContext(browser_context());

  std::vector<std::string> languages =
      translate_prefs->GetAlwaysTranslateLanguages();

  base::Value::List always_translate_languages;
  for (const auto& entry : languages) {
    always_translate_languages.Append(entry);
  }

  return RespondNow(WithArguments(std::move(always_translate_languages)));
}

LanguageSettingsPrivateSetLanguageAlwaysTranslateStateFunction::
    LanguageSettingsPrivateSetLanguageAlwaysTranslateStateFunction() = default;

LanguageSettingsPrivateSetLanguageAlwaysTranslateStateFunction::
    ~LanguageSettingsPrivateSetLanguageAlwaysTranslateStateFunction() = default;

ExtensionFunction::ResponseAction
LanguageSettingsPrivateSetLanguageAlwaysTranslateStateFunction::Run() {
  const auto params = language_settings_private::
      SetLanguageAlwaysTranslateState::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  const std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      CreateTranslatePrefsForBrowserContext(browser_context());

  if (params->always_translate) {
    language::LanguageModel* language_model =
        LanguageModelManagerFactory::GetForBrowserContext(browser_context())
            ->GetPrimaryModel();
    std::string target_language = TranslateService::GetTargetLanguage(
        Profile::FromBrowserContext(browser_context())->GetPrefs(),
        language_model);
    translate_prefs->AddLanguagePairToAlwaysTranslateList(params->language_code,
                                                          target_language);
  } else {
    translate_prefs->RemoveLanguagePairFromAlwaysTranslateList(
        params->language_code);
  }

  return RespondNow(NoArguments());
}

LanguageSettingsPrivateGetNeverTranslateLanguagesFunction::
    LanguageSettingsPrivateGetNeverTranslateLanguagesFunction() = default;

LanguageSettingsPrivateGetNeverTranslateLanguagesFunction::
    ~LanguageSettingsPrivateGetNeverTranslateLanguagesFunction() = default;

ExtensionFunction::ResponseAction
LanguageSettingsPrivateGetNeverTranslateLanguagesFunction::Run() {
  const std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      CreateTranslatePrefsForBrowserContext(browser_context());

  std::vector<std::string> languages =
      translate_prefs->GetNeverTranslateLanguages();

  base::Value::List never_translate_languages;
  for (auto& entry : languages) {
    never_translate_languages.Append(std::move(entry));
  }
  return RespondNow(WithArguments(std::move(never_translate_languages)));
}

LanguageSettingsPrivateMoveLanguageFunction::
    LanguageSettingsPrivateMoveLanguageFunction() = default;

LanguageSettingsPrivateMoveLanguageFunction::
    ~LanguageSettingsPrivateMoveLanguageFunction() = default;

ExtensionFunction::ResponseAction
LanguageSettingsPrivateMoveLanguageFunction::Run() {
  const auto parameters =
      language_settings_private::MoveLanguage::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  const std::string app_locale =
      ExtensionsBrowserClient::Get()->GetApplicationLocale();
  std::vector<std::string> supported_language_codes;
  l10n_util::GetAcceptLanguagesForLocale(app_locale, &supported_language_codes);

  const std::string& language_code = parameters->language_code;
  const language_settings_private::MoveType move_type = parameters->move_type;

  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      CreateTranslatePrefsForBrowserContext(browser_context());

  translate::TranslatePrefs::RearrangeSpecifier where =
      translate::TranslatePrefs::kNone;
  switch (move_type) {
    case language_settings_private::MoveType::kTop:
      where = translate::TranslatePrefs::kTop;
      break;

    case language_settings_private::MoveType::kUp:
      where = translate::TranslatePrefs::kUp;
      break;

    case language_settings_private::MoveType::kDown:
      where = translate::TranslatePrefs::kDown;
      break;

    case language_settings_private::MoveType::kNone:
    case language_settings_private::MoveType::kMaxValue:
      NOTREACHED_IN_MIGRATION();
  }

  // On Desktop we can only move languages by one position.
  const int offset = 1;
  translate_prefs->RearrangeLanguage(language_code, where, offset,
                                     supported_language_codes);

  return RespondNow(NoArguments());
}

LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction::
    LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction() = default;

LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction::
    ~LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction() = default;

ExtensionFunction::ResponseAction
LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction::Run() {
  LanguageSettingsPrivateDelegate* delegate =
      LanguageSettingsPrivateDelegateFactory::GetForBrowserContext(
          browser_context());

  return RespondNow(ArgumentList(
      language_settings_private::GetSpellcheckDictionaryStatuses::Results::
          Create(delegate->GetHunspellDictionaryStatuses())));
}

LanguageSettingsPrivateGetSpellcheckWordsFunction::
    LanguageSettingsPrivateGetSpellcheckWordsFunction() = default;

LanguageSettingsPrivateGetSpellcheckWordsFunction::
    ~LanguageSettingsPrivateGetSpellcheckWordsFunction() = default;

ExtensionFunction::ResponseAction
LanguageSettingsPrivateGetSpellcheckWordsFunction::Run() {
  SpellcheckService* service =
      SpellcheckServiceFactory::GetForContext(browser_context());
  SpellcheckCustomDictionary* dictionary = service->GetCustomDictionary();

  if (dictionary->IsLoaded())
    return RespondNow(WithArguments(GetSpellcheckWords()));

  dictionary->AddObserver(this);
  AddRef();  // Balanced in OnCustomDictionaryLoaded().
  return RespondLater();
}

void LanguageSettingsPrivateGetSpellcheckWordsFunction::
    OnCustomDictionaryLoaded() {
  SpellcheckService* service =
      SpellcheckServiceFactory::GetForContext(browser_context());
  service->GetCustomDictionary()->RemoveObserver(this);
  Respond(WithArguments(GetSpellcheckWords()));
  Release();
}

void LanguageSettingsPrivateGetSpellcheckWordsFunction::
    OnCustomDictionaryChanged(
        const SpellcheckCustomDictionary::Change& dictionary_change) {
  NOTREACHED_IN_MIGRATION() << "SpellcheckCustomDictionary::Observer: "
                               "OnCustomDictionaryChanged() called before "
                               "OnCustomDictionaryLoaded()";
}

base::Value::List
LanguageSettingsPrivateGetSpellcheckWordsFunction::GetSpellcheckWords() const {
  SpellcheckService* service =
      SpellcheckServiceFactory::GetForContext(browser_context());
  SpellcheckCustomDictionary* dictionary = service->GetCustomDictionary();
  DCHECK(dictionary->IsLoaded());

  // TODO(michaelpg): Sort using app locale.
  base::Value::List word_list;
  const std::set<std::string>& words = dictionary->GetWords();
  word_list.reserve(words.size());
  for (const std::string& word : words)
    word_list.Append(word);
  return word_list;
}

LanguageSettingsPrivateAddSpellcheckWordFunction::
    LanguageSettingsPrivateAddSpellcheckWordFunction() = default;

LanguageSettingsPrivateAddSpellcheckWordFunction::
    ~LanguageSettingsPrivateAddSpellcheckWordFunction() = default;

ExtensionFunction::ResponseAction
LanguageSettingsPrivateAddSpellcheckWordFunction::Run() {
  const auto params =
      language_settings_private::AddSpellcheckWord::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  SpellcheckService* service =
      SpellcheckServiceFactory::GetForContext(browser_context());
  bool success = service->GetCustomDictionary()->AddWord(params->word);

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  if (spellcheck::UseBrowserSpellChecker()) {
    spellcheck_platform::AddWord(service->platform_spell_checker(),
                                 base::UTF8ToUTF16(params->word));
  }
#endif

  return RespondNow(WithArguments(success));
}

LanguageSettingsPrivateRemoveSpellcheckWordFunction::
    LanguageSettingsPrivateRemoveSpellcheckWordFunction() = default;

LanguageSettingsPrivateRemoveSpellcheckWordFunction::
    ~LanguageSettingsPrivateRemoveSpellcheckWordFunction() = default;

ExtensionFunction::ResponseAction
LanguageSettingsPrivateRemoveSpellcheckWordFunction::Run() {
  const auto params =
      language_settings_private::RemoveSpellcheckWord::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  SpellcheckService* service =
      SpellcheckServiceFactory::GetForContext(browser_context());
  bool success = service->GetCustomDictionary()->RemoveWord(params->word);

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  if (spellcheck::UseBrowserSpellChecker()) {
    spellcheck_platform::RemoveWord(service->platform_spell_checker(),
                                    base::UTF8ToUTF16(params->word));
  }
#endif

  return RespondNow(WithArguments(success));
}

LanguageSettingsPrivateGetTranslateTargetLanguageFunction::
    LanguageSettingsPrivateGetTranslateTargetLanguageFunction() = default;

LanguageSettingsPrivateGetTranslateTargetLanguageFunction::
    ~LanguageSettingsPrivateGetTranslateTargetLanguageFunction() = default;

ExtensionFunction::ResponseAction
LanguageSettingsPrivateGetTranslateTargetLanguageFunction::Run() {
  language::LanguageModel* language_model =
      LanguageModelManagerFactory::GetForBrowserContext(browser_context())
          ->GetPrimaryModel();
  return RespondNow(WithArguments(TranslateService::GetTargetLanguage(
      Profile::FromBrowserContext(browser_context())->GetPrefs(),
      language_model)));
}

LanguageSettingsPrivateSetTranslateTargetLanguageFunction::
    LanguageSettingsPrivateSetTranslateTargetLanguageFunction() = default;

LanguageSettingsPrivateSetTranslateTargetLanguageFunction::
    ~LanguageSettingsPrivateSetTranslateTargetLanguageFunction() = default;

ExtensionFunction::ResponseAction
LanguageSettingsPrivateSetTranslateTargetLanguageFunction::Run() {
  const auto parameters =
      language_settings_private::SetTranslateTargetLanguage::Params::Create(
          args());
  EXTENSION_FUNCTION_VALIDATE(parameters);
  const std::string& language_code = parameters->language_code;

  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      CreateTranslatePrefsForBrowserContext(browser_context());

  std::string chrome_language = language_code;

  if (language_code == translate_prefs->GetRecentTargetLanguage()) {
    return RespondNow(NoArguments());
  }
  translate_prefs->SetRecentTargetLanguage(language_code);

  return RespondNow(NoArguments());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Populates the vector of input methods using information in the list of
// descriptors. Used for languageSettingsPrivate.getInputMethodLists().
void PopulateInputMethodListFromDescriptors(
    const InputMethodDescriptors& descriptors,
    std::vector<language_settings_private::InputMethod>* input_methods) {
  InputMethodManager* manager = InputMethodManager::Get();
  InputMethodUtil* util = manager->GetInputMethodUtil();
  scoped_refptr<InputMethodManager::State> ime_state =
      manager->GetActiveIMEState();
  if (!ime_state.get())
    return;

  const base::flat_set<std::string> enabled_ids(
      ime_state->GetEnabledInputMethodIds());
  const base::flat_set<std::string> allowed_ids(
      ime_state->GetAllowedInputMethodIds());

  UErrorCode error = U_ZERO_ERROR;
  std::unique_ptr<icu::Collator> collator(
      icu::Collator::createInstance(error));  // use current ICU locale
  DCHECK(U_SUCCESS(error));

  // Map of sorted [display name -> input methods].
  std::map<std::u16string, language_settings_private::InputMethod,
           l10n_util::StringComparator<std::u16string>>
      input_map(l10n_util::StringComparator<std::u16string>(collator.get()));

  for (const auto& descriptor : descriptors) {
    language_settings_private::InputMethod input_method;
    input_method.id = descriptor.id();
    input_method.display_name = util->GetLocalizedDisplayName(descriptor);
    input_method.language_codes = descriptor.language_codes();
    input_method.tags = GetInputMethodTags(&input_method);
    if (base::Contains(enabled_ids, input_method.id))
      input_method.enabled = true;
    if (descriptor.options_page_url().is_valid())
      input_method.has_options_page = true;
    if (!allowed_ids.empty() && !base::Contains(allowed_ids, input_method.id)) {
      input_method.is_prohibited_by_policy = true;
    }
    input_map[base::UTF8ToUTF16(util->GetLocalizedDisplayName(descriptor))] =
        std::move(input_method);
  }

  for (auto& entry : input_map) {
    input_methods->push_back(std::move(entry.second));
  }
}
#endif

LanguageSettingsPrivateGetInputMethodListsFunction::
    LanguageSettingsPrivateGetInputMethodListsFunction() = default;

LanguageSettingsPrivateGetInputMethodListsFunction::
    ~LanguageSettingsPrivateGetInputMethodListsFunction() = default;

ExtensionFunction::ResponseAction
LanguageSettingsPrivateGetInputMethodListsFunction::Run() {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  EXTENSION_FUNCTION_VALIDATE(false);
  return RespondNow(NoArguments());
#else
  language_settings_private::InputMethodLists input_method_lists;
  InputMethodManager* manager = InputMethodManager::Get();

  ash::ComponentExtensionIMEManager* component_extension_manager =
      manager->GetComponentExtensionIMEManager();
  PopulateInputMethodListFromDescriptors(
      component_extension_manager->GetAllIMEAsInputMethodDescriptor(),
      &input_method_lists.component_extension_imes);

  scoped_refptr<InputMethodManager::State> ime_state =
      manager->GetActiveIMEState();
  if (ime_state.get()) {
    InputMethodDescriptors ext_ime_descriptors;
    ime_state->GetInputMethodExtensions(&ext_ime_descriptors);
    PopulateInputMethodListFromDescriptors(
        ext_ime_descriptors, &input_method_lists.third_party_extension_imes);
  }

  return RespondNow(WithArguments(input_method_lists.ToValue()));
#endif
}

LanguageSettingsPrivateAddInputMethodFunction::
    LanguageSettingsPrivateAddInputMethodFunction() = default;

LanguageSettingsPrivateAddInputMethodFunction::
    ~LanguageSettingsPrivateAddInputMethodFunction() = default;

ExtensionFunction::ResponseAction
LanguageSettingsPrivateAddInputMethodFunction::Run() {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  EXTENSION_FUNCTION_VALIDATE(false);
#else
  const auto params =
      language_settings_private::AddInputMethod::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  InputMethodManager* manager = InputMethodManager::Get();
  scoped_refptr<InputMethodManager::State> ime_state =
      manager->GetActiveIMEState();
  if (!ime_state.get())
    return RespondNow(NoArguments());

  std::string new_input_method_id = params->input_method_id;
  bool is_component_extension_ime =
      ash::extension_ime_util::IsComponentExtensionIME(new_input_method_id);

  PrefService* prefs =
      Profile::FromBrowserContext(browser_context())->GetPrefs();
  const char* pref_name = is_component_extension_ime
                              ? prefs::kLanguagePreloadEngines
                              : prefs::kLanguageEnabledImes;

  // Get the input methods we are adding to.
  base::flat_set<std::string> input_method_set(
      GetIMEsFromPref(prefs, pref_name));

  // Add the new input method.
  input_method_set.insert(new_input_method_id);

  // Sort the new input method list by preferred languages.
  std::vector<std::string> input_method_list;
  if (is_component_extension_ime) {
    input_method_list =
        GetSortedComponentIMEs(manager, ime_state, input_method_set, prefs);
  } else {
    input_method_list =
        GetSortedThirdPartyIMEs(ime_state, input_method_set, prefs);
  }

  std::string input_methods = base::JoinString(input_method_list, ",");
  prefs->SetString(pref_name, input_methods);

  // We want to automatically enable "Show input options in shelf" when the user
  // has multiple input methods.
  // We don't want to repeatedly enable it every time the user adds an input
  // method, as a user may want to intentionally turn it off - so we only enable
  // it once the user reaches two input methods.
  // As pref_name and input_method_set only refer to the preference related to
  // the list of IMEs for which this newly-added IME is in, we need the other
  // IME list to calculate the total number of IMEs.
  const char* other_ime_list_pref_name = is_component_extension_ime
                                             ? prefs::kLanguageEnabledImes
                                             : prefs::kLanguagePreloadEngines;
  base::flat_set<std::string> other_input_method_set(
      GetIMEsFromPref(prefs, other_ime_list_pref_name));
  if (input_method_set.size() + other_input_method_set.size() ==
      kNumImesToAutoEnableImeMenu) {
    prefs->SetBoolean(prefs::kLanguageImeMenuActivated, true);
  }
#endif
  return RespondNow(NoArguments());
}

LanguageSettingsPrivateRemoveInputMethodFunction::
    LanguageSettingsPrivateRemoveInputMethodFunction() = default;

LanguageSettingsPrivateRemoveInputMethodFunction::
    ~LanguageSettingsPrivateRemoveInputMethodFunction() = default;

ExtensionFunction::ResponseAction
LanguageSettingsPrivateRemoveInputMethodFunction::Run() {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  EXTENSION_FUNCTION_VALIDATE(false);
#else
  const auto params =
      language_settings_private::RemoveInputMethod::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  InputMethodManager* manager = InputMethodManager::Get();
  scoped_refptr<InputMethodManager::State> ime_state =
      manager->GetActiveIMEState();
  if (!ime_state.get())
    return RespondNow(NoArguments());

  std::string input_method_id = params->input_method_id;
  bool is_component_extension_ime =
      ash::extension_ime_util::IsComponentExtensionIME(input_method_id);

  // Use the pref for the corresponding input method type.
  PrefService* prefs =
      Profile::FromBrowserContext(browser_context())->GetPrefs();
  const char* pref_name = is_component_extension_ime
                              ? prefs::kLanguagePreloadEngines
                              : prefs::kLanguageEnabledImes;

  std::string input_method_ids = prefs->GetString(pref_name);
  std::vector<std::string> input_method_list = base::SplitString(
      input_method_ids, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Find and remove the matching input method id.
  const auto& pos = base::ranges::find(input_method_list, input_method_id);
  if (pos != input_method_list.end()) {
    input_method_list.erase(pos);
    prefs->SetString(pref_name, base::JoinString(input_method_list, ","));
  }
#endif
  return RespondNow(NoArguments());
}

LanguageSettingsPrivateRetryDownloadDictionaryFunction::
    LanguageSettingsPrivateRetryDownloadDictionaryFunction() = default;

LanguageSettingsPrivateRetryDownloadDictionaryFunction::
    ~LanguageSettingsPrivateRetryDownloadDictionaryFunction() = default;

ExtensionFunction::ResponseAction
LanguageSettingsPrivateRetryDownloadDictionaryFunction::Run() {
  const auto parameters =
      language_settings_private::RetryDownloadDictionary::Params::Create(
          args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  LanguageSettingsPrivateDelegate* delegate =
      LanguageSettingsPrivateDelegateFactory::GetForBrowserContext(
          browser_context());
  delegate->RetryDownloadHunspellDictionary(parameters->language_code);
  return RespondNow(NoArguments());
}

}  // namespace extensions
