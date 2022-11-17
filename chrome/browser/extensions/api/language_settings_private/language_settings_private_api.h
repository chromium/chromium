// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_LANGUAGE_SETTINGS_PRIVATE_LANGUAGE_SETTINGS_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_LANGUAGE_SETTINGS_PRIVATE_LANGUAGE_SETTINGS_PRIVATE_API_H_

#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/spellchecker/spellcheck_custom_dictionary.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// Implements the languageSettingsPrivate.getLanguageList method.
class LanguageSettingsPrivateGetLanguageListFunction
    : public ExtensionFunction {
 public:
  LanguageSettingsPrivateGetLanguageListFunction();

  LanguageSettingsPrivateGetLanguageListFunction(
      const LanguageSettingsPrivateGetLanguageListFunction&) = delete;
  LanguageSettingsPrivateGetLanguageListFunction& operator=(
      const LanguageSettingsPrivateGetLanguageListFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("languageSettingsPrivate.getLanguageList",
                             LANGUAGESETTINGSPRIVATE_GETLANGUAGELIST)

 protected:
  ~LanguageSettingsPrivateGetLanguageListFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

#if BUILDFLAG(IS_WIN)
  void OnDictionariesInitialized();
  void UpdateSupportedPlatformDictionaries();
#endif  // BUILDFLAG(IS_WIN)

 private:
  base::Value::List language_list_;
};

// Implements the languageSettingsPrivate.enableLanguage method.
class LanguageSettingsPrivateEnableLanguageFunction : public ExtensionFunction {
 public:
  LanguageSettingsPrivateEnableLanguageFunction();

  LanguageSettingsPrivateEnableLanguageFunction(
      const LanguageSettingsPrivateEnableLanguageFunction&) = delete;
  LanguageSettingsPrivateEnableLanguageFunction& operator=(
      const LanguageSettingsPrivateEnableLanguageFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("languageSettingsPrivate.enableLanguage",
                             LANGUAGESETTINGSPRIVATE_ENABLELANGUAGE)

 protected:
  ~LanguageSettingsPrivateEnableLanguageFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the languageSettingsPrivate.disableLanguage method.
class LanguageSettingsPrivateDisableLanguageFunction
    : public ExtensionFunction {
 public:
  LanguageSettingsPrivateDisableLanguageFunction();

  LanguageSettingsPrivateDisableLanguageFunction(
      const LanguageSettingsPrivateDisableLanguageFunction&) = delete;
  LanguageSettingsPrivateDisableLanguageFunction& operator=(
      const LanguageSettingsPrivateDisableLanguageFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("languageSettingsPrivate.disableLanguage",
                             LANGUAGESETTINGSPRIVATE_DISABLELANGUAGE)

 protected:
  ~LanguageSettingsPrivateDisableLanguageFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the languageSettingsPrivate.setEnableTranslationForLanguage
// method.
class LanguageSettingsPrivateSetEnableTranslationForLanguageFunction
    : public ExtensionFunction {
 public:
  LanguageSettingsPrivateSetEnableTranslationForLanguageFunction();

  LanguageSettingsPrivateSetEnableTranslationForLanguageFunction(
      const LanguageSettingsPrivateSetEnableTranslationForLanguageFunction&) =
      delete;
  LanguageSettingsPrivateSetEnableTranslationForLanguageFunction& operator=(
      const LanguageSettingsPrivateSetEnableTranslationForLanguageFunction&) =
      delete;

  DECLARE_EXTENSION_FUNCTION(
      "languageSettingsPrivate.setEnableTranslationForLanguage",
      LANGUAGESETTINGSPRIVATE_SETENABLETRANSLATIONFORLANGUAGE)

 protected:
  ~LanguageSettingsPrivateSetEnableTranslationForLanguageFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the languageSettingsPrivate.moveLanguage method.
class LanguageSettingsPrivateMoveLanguageFunction : public ExtensionFunction {
 public:
  LanguageSettingsPrivateMoveLanguageFunction();

  LanguageSettingsPrivateMoveLanguageFunction(
      const LanguageSettingsPrivateMoveLanguageFunction&) = delete;
  LanguageSettingsPrivateMoveLanguageFunction& operator=(
      const LanguageSettingsPrivateMoveLanguageFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("languageSettingsPrivate.moveLanguage",
                             LANGUAGESETTINGSPRIVATE_MOVELANGUAGE)

 protected:
  ~LanguageSettingsPrivateMoveLanguageFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the languageSettingsPrivate.getAlwaysTranslateLanguages method.
class LanguageSettingsPrivateGetAlwaysTranslateLanguagesFunction
    : public ExtensionFunction {
 public:
  LanguageSettingsPrivateGetAlwaysTranslateLanguagesFunction();

  LanguageSettingsPrivateGetAlwaysTranslateLanguagesFunction(
      const LanguageSettingsPrivateGetAlwaysTranslateLanguagesFunction&) =
      delete;
  LanguageSettingsPrivateGetAlwaysTranslateLanguagesFunction& operator=(
      const LanguageSettingsPrivateGetAlwaysTranslateLanguagesFunction&) =
      delete;

  DECLARE_EXTENSION_FUNCTION(
      "languageSettingsPrivate.getAlwaysTranslateLanguages",
      LANGUAGESETTINGSPRIVATE_GETALWAYSTRANSLATELANGUAGES)

 protected:
  ~LanguageSettingsPrivateGetAlwaysTranslateLanguagesFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the languageSettingsPrivate.setLanguageAlwaysTranslateState
// method.
class LanguageSettingsPrivateSetLanguageAlwaysTranslateStateFunction
    : public ExtensionFunction {
 public:
  LanguageSettingsPrivateSetLanguageAlwaysTranslateStateFunction();

  LanguageSettingsPrivateSetLanguageAlwaysTranslateStateFunction(
      const LanguageSettingsPrivateSetLanguageAlwaysTranslateStateFunction&) =
      delete;
  LanguageSettingsPrivateSetLanguageAlwaysTranslateStateFunction& operator=(
      const LanguageSettingsPrivateSetLanguageAlwaysTranslateStateFunction&) =
      delete;

  DECLARE_EXTENSION_FUNCTION(
      "languageSettingsPrivate.setLanguageAlwaysTranslateState",
      LANGUAGESETTINGSPRIVATE_SETLANGUAGEALWAYSTRANSLATESTATE)

 protected:
  ~LanguageSettingsPrivateSetLanguageAlwaysTranslateStateFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the languageSettingsPrivate.getNeverTranslateLanguages method.
class LanguageSettingsPrivateGetNeverTranslateLanguagesFunction
    : public ExtensionFunction {
 public:
  LanguageSettingsPrivateGetNeverTranslateLanguagesFunction();

  LanguageSettingsPrivateGetNeverTranslateLanguagesFunction(
      const LanguageSettingsPrivateGetNeverTranslateLanguagesFunction&) =
      delete;
  LanguageSettingsPrivateGetNeverTranslateLanguagesFunction& operator=(
      const LanguageSettingsPrivateGetNeverTranslateLanguagesFunction&) =
      delete;

  DECLARE_EXTENSION_FUNCTION(
      "languageSettingsPrivate.getNeverTranslateLanguages",
      LANGUAGESETTINGSPRIVATE_GETNEVERTRANSLATELANGUAGES)

 protected:
  ~LanguageSettingsPrivateGetNeverTranslateLanguagesFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the languageSettingsPrivate.getSpellcheckDictionaryStatuses
// method.
class LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction
    : public ExtensionFunction {
 public:
  LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction();

  LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction(
      const LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction&) =
      delete;
  LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction& operator=(
      const LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction&) =
      delete;

  DECLARE_EXTENSION_FUNCTION(
      "languageSettingsPrivate.getSpellcheckDictionaryStatuses",
      LANGUAGESETTINGSPRIVATE_GETSPELLCHECKDICTIONARYSTATUS)

 protected:
  ~LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the languageSettingsPrivate.getSpellcheckWords method.
class LanguageSettingsPrivateGetSpellcheckWordsFunction
    : public ExtensionFunction,
      public SpellcheckCustomDictionary::Observer {
 public:
  LanguageSettingsPrivateGetSpellcheckWordsFunction();

  LanguageSettingsPrivateGetSpellcheckWordsFunction(
      const LanguageSettingsPrivateGetSpellcheckWordsFunction&) = delete;
  LanguageSettingsPrivateGetSpellcheckWordsFunction& operator=(
      const LanguageSettingsPrivateGetSpellcheckWordsFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("languageSettingsPrivate.getSpellcheckWords",
                             LANGUAGESETTINGSPRIVATE_GETSPELLCHECKWORDS)

 protected:
  ~LanguageSettingsPrivateGetSpellcheckWordsFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

  // SpellcheckCustomDictionary::Observer overrides.
  void OnCustomDictionaryLoaded() override;
  void OnCustomDictionaryChanged(
      const SpellcheckCustomDictionary::Change& dictionary_change) override;

  // Returns the list of words from the loaded custom dictionary.
  base::Value::List GetSpellcheckWords() const;
};

// Implements the languageSettingsPrivate.addSpellcheckWord method.
class LanguageSettingsPrivateAddSpellcheckWordFunction
    : public ExtensionFunction {
 public:
  LanguageSettingsPrivateAddSpellcheckWordFunction();

  LanguageSettingsPrivateAddSpellcheckWordFunction(
      const LanguageSettingsPrivateAddSpellcheckWordFunction&) = delete;
  LanguageSettingsPrivateAddSpellcheckWordFunction& operator=(
      const LanguageSettingsPrivateAddSpellcheckWordFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("languageSettingsPrivate.addSpellcheckWord",
                             LANGUAGESETTINGSPRIVATE_ADDSPELLCHECKWORD)

 protected:
  ~LanguageSettingsPrivateAddSpellcheckWordFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the languageSettingsPrivate.removeSpellcheckWord method.
class LanguageSettingsPrivateRemoveSpellcheckWordFunction
    : public ExtensionFunction {
 public:
  LanguageSettingsPrivateRemoveSpellcheckWordFunction();

  LanguageSettingsPrivateRemoveSpellcheckWordFunction(
      const LanguageSettingsPrivateRemoveSpellcheckWordFunction&) = delete;
  LanguageSettingsPrivateRemoveSpellcheckWordFunction& operator=(
      const LanguageSettingsPrivateRemoveSpellcheckWordFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("languageSettingsPrivate.removeSpellcheckWord",
                             LANGUAGESETTINGSPRIVATE_REMOVESPELLCHECKWORD)

 protected:
  ~LanguageSettingsPrivateRemoveSpellcheckWordFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the languageSettingsPrivate.getTranslateTargetLanguage method.
class LanguageSettingsPrivateGetTranslateTargetLanguageFunction
    : public ExtensionFunction {
 public:
  LanguageSettingsPrivateGetTranslateTargetLanguageFunction();

  LanguageSettingsPrivateGetTranslateTargetLanguageFunction(
      const LanguageSettingsPrivateGetTranslateTargetLanguageFunction&) =
      delete;
  LanguageSettingsPrivateGetTranslateTargetLanguageFunction& operator=(
      const LanguageSettingsPrivateGetTranslateTargetLanguageFunction&) =
      delete;

  DECLARE_EXTENSION_FUNCTION(
      "languageSettingsPrivate.getTranslateTargetLanguage",
      LANGUAGESETTINGSPRIVATE_GETTRANSLATETARGETLANGUAGE)

 protected:
  ~LanguageSettingsPrivateGetTranslateTargetLanguageFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the languageSettingsPrivate.setTranslateTargetLanguage method.
class LanguageSettingsPrivateSetTranslateTargetLanguageFunction
    : public ExtensionFunction {
 public:
  LanguageSettingsPrivateSetTranslateTargetLanguageFunction();

  LanguageSettingsPrivateSetTranslateTargetLanguageFunction(
      const LanguageSettingsPrivateSetTranslateTargetLanguageFunction&) =
      delete;
  LanguageSettingsPrivateSetTranslateTargetLanguageFunction& operator=(
      const LanguageSettingsPrivateSetTranslateTargetLanguageFunction&) =
      delete;

  DECLARE_EXTENSION_FUNCTION(
      "languageSettingsPrivate.setTranslateTargetLanguage",
      LANGUAGESETTINGSPRIVATE_SETTRANSLATETARGETLANGUAGE)

 protected:
  ~LanguageSettingsPrivateSetTranslateTargetLanguageFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the languageSettingsPrivate.getInputMethodLists method.
class LanguageSettingsPrivateGetInputMethodListsFunction
    : public ExtensionFunction {
 public:
  LanguageSettingsPrivateGetInputMethodListsFunction();

  LanguageSettingsPrivateGetInputMethodListsFunction(
      const LanguageSettingsPrivateGetInputMethodListsFunction&) = delete;
  LanguageSettingsPrivateGetInputMethodListsFunction& operator=(
      const LanguageSettingsPrivateGetInputMethodListsFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("languageSettingsPrivate.getInputMethodLists",
                             LANGUAGESETTINGSPRIVATE_GETINPUTMETHODLISTS)

 protected:
  ~LanguageSettingsPrivateGetInputMethodListsFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the languageSettingsPrivate.addInputMethod method.
class LanguageSettingsPrivateAddInputMethodFunction : public ExtensionFunction {
 public:
  LanguageSettingsPrivateAddInputMethodFunction();

  LanguageSettingsPrivateAddInputMethodFunction(
      const LanguageSettingsPrivateAddInputMethodFunction&) = delete;
  LanguageSettingsPrivateAddInputMethodFunction& operator=(
      const LanguageSettingsPrivateAddInputMethodFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("languageSettingsPrivate.addInputMethod",
                             LANGUAGESETTINGSPRIVATE_ADDINPUTMETHOD)

 protected:
  ~LanguageSettingsPrivateAddInputMethodFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the languageSettingsPrivate.removeInputMethod method.
class LanguageSettingsPrivateRemoveInputMethodFunction
    : public ExtensionFunction {
 public:
  LanguageSettingsPrivateRemoveInputMethodFunction();

  LanguageSettingsPrivateRemoveInputMethodFunction(
      const LanguageSettingsPrivateRemoveInputMethodFunction&) = delete;
  LanguageSettingsPrivateRemoveInputMethodFunction& operator=(
      const LanguageSettingsPrivateRemoveInputMethodFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("languageSettingsPrivate.removeInputMethod",
                             LANGUAGESETTINGSPRIVATE_REMOVEINPUTMETHOD)

 protected:
  ~LanguageSettingsPrivateRemoveInputMethodFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

// Implements the languageSettingsPrivate.retryDownloadDictionary method.
class LanguageSettingsPrivateRetryDownloadDictionaryFunction
    : public ExtensionFunction {
 public:
  LanguageSettingsPrivateRetryDownloadDictionaryFunction();

  LanguageSettingsPrivateRetryDownloadDictionaryFunction(
      const LanguageSettingsPrivateRetryDownloadDictionaryFunction&) = delete;
  LanguageSettingsPrivateRetryDownloadDictionaryFunction& operator=(
      const LanguageSettingsPrivateRetryDownloadDictionaryFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("languageSettingsPrivate.retryDownloadDictionary",
                             LANGUAGESETTINGSPRIVATE_RETRYDOWNLOADDICTIONARY)

 protected:
  ~LanguageSettingsPrivateRetryDownloadDictionaryFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_LANGUAGE_SETTINGS_PRIVATE_LANGUAGE_SETTINGS_PRIVATE_API_H_
