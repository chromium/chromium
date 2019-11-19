// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_LANGUAGE_SETTINGS_PRIVATE_LANGUAGE_SETTINGS_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_LANGUAGE_SETTINGS_PRIVATE_LANGUAGE_SETTINGS_PRIVATE_API_H_

#include "base/macros.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/spellchecker/spellcheck_custom_dictionary.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// Implements the languageSettingsPrivate.getLanguageList method.
class LanguageSettingsPrivateGetLanguageListFunction
    : public ExtensionFunction {
 public:
  LanguageSettingsPrivateGetLanguageListFunction();
  DECLARE_EXTENSION_FUNCTION("languageSettingsPrivate.getLanguageList",
                             LANGUAGESETTINGSPRIVATE_GETLANGUAGELIST)

 protected:
  ~LanguageSettingsPrivateGetLanguageListFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ChromeExtensionFunctionDetails chrome_details_;

  DISALLOW_COPY_AND_ASSIGN(LanguageSettingsPrivateGetLanguageListFunction);
};

// Implements the languageSettingsPrivate.enableLanguage method.
class LanguageSettingsPrivateEnableLanguageFunction : public ExtensionFunction {
 public:
  LanguageSettingsPrivateEnableLanguageFunction();
  DECLARE_EXTENSION_FUNCTION("languageSettingsPrivate.enableLanguage",
                             LANGUAGESETTINGSPRIVATE_ENABLELANGUAGE)

 protected:
  ~LanguageSettingsPrivateEnableLanguageFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ChromeExtensionFunctionDetails chrome_details_;

  DISALLOW_COPY_AND_ASSIGN(LanguageSettingsPrivateEnableLanguageFunction);
};

// Implements the languageSettingsPrivate.disableLanguage method.
class LanguageSettingsPrivateDisableLanguageFunction
    : public ExtensionFunction {
 public:
  LanguageSettingsPrivateDisableLanguageFunction();
  DECLARE_EXTENSION_FUNCTION("languageSettingsPrivate.disableLanguage",
                             LANGUAGESETTINGSPRIVATE_DISABLELANGUAGE)

 protected:
  ~LanguageSettingsPrivateDisableLanguageFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ChromeExtensionFunctionDetails chrome_details_;

  DISALLOW_COPY_AND_ASSIGN(LanguageSettingsPrivateDisableLanguageFunction);
};

// Implements the languageSettingsPrivate.setEnableTranslationForLanguage
// method.
class LanguageSettingsPrivateSetEnableTranslationForLanguageFunction
    : public ExtensionFunction {
 public:
  LanguageSettingsPrivateSetEnableTranslationForLanguageFunction();
  DECLARE_EXTENSION_FUNCTION(
      "languageSettingsPrivate.setEnableTranslationForLanguage",
      LANGUAGESETTINGSPRIVATE_SETENABLETRANSLATIONFORLANGUAGE)

 protected:
  ~LanguageSettingsPrivateSetEnableTranslationForLanguageFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ChromeExtensionFunctionDetails chrome_details_;
  DISALLOW_COPY_AND_ASSIGN(
      LanguageSettingsPrivateSetEnableTranslationForLanguageFunction);
};

// Implements the languageSettingsPrivate.moveLanguage method.
class LanguageSettingsPrivateMoveLanguageFunction : public ExtensionFunction {
 public:
  LanguageSettingsPrivateMoveLanguageFunction();
  DECLARE_EXTENSION_FUNCTION("languageSettingsPrivate.moveLanguage",
                             LANGUAGESETTINGSPRIVATE_MOVELANGUAGE)

 protected:
  ~LanguageSettingsPrivateMoveLanguageFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ChromeExtensionFunctionDetails chrome_details_;
  DISALLOW_COPY_AND_ASSIGN(LanguageSettingsPrivateMoveLanguageFunction);
};

// Implements the languageSettingsPrivate.getSpellcheckDictionaryStatuses
// method.
class LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction
    : public ExtensionFunction {
 public:
  LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction();
  DECLARE_EXTENSION_FUNCTION(
      "languageSettingsPrivate.getSpellcheckDictionaryStatuses",
      LANGUAGESETTINGSPRIVATE_GETSPELLCHECKDICTIONARYSTATUS)

 protected:
  ~LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(
      LanguageSettingsPrivateGetSpellcheckDictionaryStatusesFunction);
};

// Implements the languageSettingsPrivate.getSpellcheckWords method.
class LanguageSettingsPrivateGetSpellcheckWordsFunction
    : public ExtensionFunction,
      public SpellcheckCustomDictionary::Observer {
 public:
  LanguageSettingsPrivateGetSpellcheckWordsFunction();
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
  std::unique_ptr<base::ListValue> GetSpellcheckWords() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(LanguageSettingsPrivateGetSpellcheckWordsFunction);
};

// Implements the languageSettingsPrivate.addSpellcheckWord method.
class LanguageSettingsPrivateAddSpellcheckWordFunction
    : public ExtensionFunction {
 public:
  LanguageSettingsPrivateAddSpellcheckWordFunction();
  DECLARE_EXTENSION_FUNCTION("languageSettingsPrivate.addSpellcheckWord",
                             LANGUAGESETTINGSPRIVATE_ADDSPELLCHECKWORD)

 protected:
  ~LanguageSettingsPrivateAddSpellcheckWordFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LanguageSettingsPrivateAddSpellcheckWordFunction);
};

// Implements the languageSettingsPrivate.removeSpellcheckWord method.
class LanguageSettingsPrivateRemoveSpellcheckWordFunction
    : public ExtensionFunction {
 public:
  LanguageSettingsPrivateRemoveSpellcheckWordFunction();
  DECLARE_EXTENSION_FUNCTION("languageSettingsPrivate.removeSpellcheckWord",
                             LANGUAGESETTINGSPRIVATE_REMOVESPELLCHECKWORD)

 protected:
  ~LanguageSettingsPrivateRemoveSpellcheckWordFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LanguageSettingsPrivateRemoveSpellcheckWordFunction);
};

// Implements the languageSettingsPrivate.getTranslateTargetLanguage method.
class LanguageSettingsPrivateGetTranslateTargetLanguageFunction
    : public ExtensionFunction {
 public:
  LanguageSettingsPrivateGetTranslateTargetLanguageFunction();
  DECLARE_EXTENSION_FUNCTION(
      "languageSettingsPrivate.getTranslateTargetLanguage",
      LANGUAGESETTINGSPRIVATE_GETTRANSLATETARGETLANGUAGE)

 protected:
  ~LanguageSettingsPrivateGetTranslateTargetLanguageFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ChromeExtensionFunctionDetails chrome_details_;

  DISALLOW_COPY_AND_ASSIGN(
      LanguageSettingsPrivateGetTranslateTargetLanguageFunction);
};

// Implements the languageSettingsPrivate.getInputMethodLists method.
class LanguageSettingsPrivateGetInputMethodListsFunction
    : public ExtensionFunction {
 public:
  LanguageSettingsPrivateGetInputMethodListsFunction();
  DECLARE_EXTENSION_FUNCTION("languageSettingsPrivate.getInputMethodLists",
                             LANGUAGESETTINGSPRIVATE_GETINPUTMETHODLISTS)

 protected:
  ~LanguageSettingsPrivateGetInputMethodListsFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LanguageSettingsPrivateGetInputMethodListsFunction);
};

// Implements the languageSettingsPrivate.addInputMethod method.
class LanguageSettingsPrivateAddInputMethodFunction : public ExtensionFunction {
 public:
  LanguageSettingsPrivateAddInputMethodFunction();
  DECLARE_EXTENSION_FUNCTION("languageSettingsPrivate.addInputMethod",
                             LANGUAGESETTINGSPRIVATE_ADDINPUTMETHOD)

 protected:
  ~LanguageSettingsPrivateAddInputMethodFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ChromeExtensionFunctionDetails chrome_details_;

  DISALLOW_COPY_AND_ASSIGN(LanguageSettingsPrivateAddInputMethodFunction);
};

// Implements the languageSettingsPrivate.removeInputMethod method.
class LanguageSettingsPrivateRemoveInputMethodFunction
    : public ExtensionFunction {
 public:
  LanguageSettingsPrivateRemoveInputMethodFunction();
  DECLARE_EXTENSION_FUNCTION("languageSettingsPrivate.removeInputMethod",
                             LANGUAGESETTINGSPRIVATE_REMOVEINPUTMETHOD)

 protected:
  ~LanguageSettingsPrivateRemoveInputMethodFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ChromeExtensionFunctionDetails chrome_details_;

  DISALLOW_COPY_AND_ASSIGN(LanguageSettingsPrivateRemoveInputMethodFunction);
};

// Implements the languageSettingsPrivate.retryDownloadDictionary method.
class LanguageSettingsPrivateRetryDownloadDictionaryFunction
    : public ExtensionFunction {
 public:
  LanguageSettingsPrivateRetryDownloadDictionaryFunction();
  DECLARE_EXTENSION_FUNCTION("languageSettingsPrivate.retryDownloadDictionary",
                             LANGUAGESETTINGSPRIVATE_RETRYDOWNLOADDICTIONARY)

 protected:
  ~LanguageSettingsPrivateRetryDownloadDictionaryFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(
      LanguageSettingsPrivateRetryDownloadDictionaryFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_LANGUAGE_SETTINGS_PRIVATE_LANGUAGE_SETTINGS_PRIVATE_API_H_
