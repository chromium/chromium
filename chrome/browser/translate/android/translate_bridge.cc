// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/android/translate_bridge.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/TranslateBridge_jni.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/language/language_model_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/language_experiments.h"
#include "components/language/core/common/language_util.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_locale_settings.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "content/public/browser/web_contents.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ToJavaArrayOfStrings;

namespace {

PrefService* GetPrefService() {
  return ProfileManager::GetActiveUserProfile()
      ->GetOriginalProfile()
      ->GetPrefs();
}

}  // namespace

static ChromeTranslateClient* GetTranslateClient(
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  ChromeTranslateClient* client =
      ChromeTranslateClient::FromWebContents(web_contents);
  DCHECK(client);
  return client;
}

static void JNI_TranslateBridge_ManualTranslateWhenReady(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  ChromeTranslateClient* client =
      ChromeTranslateClient::FromWebContents(web_contents);
  DCHECK(client);
  client->ManualTranslateWhenReady();
}

static jboolean JNI_TranslateBridge_CanManuallyTranslate(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  ChromeTranslateClient* client = GetTranslateClient(j_web_contents);
  translate::TranslateManager* manager = client->GetTranslateManager();
  DCHECK(manager);
  return manager->CanManuallyTranslate();
}

static jboolean JNI_TranslateBridge_ShouldShowManualTranslateIPH(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  ChromeTranslateClient* client = GetTranslateClient(j_web_contents);
  translate::TranslateManager* manager = client->GetTranslateManager();
  DCHECK(manager);

  const std::string page_lang = manager->GetLanguageState().original_language();
  std::unique_ptr<translate::TranslatePrefs> translate_prefs(
      client->GetTranslatePrefs());

  return base::StartsWith(page_lang, "en",
                          base::CompareCase::INSENSITIVE_ASCII) &&
         !language::ShouldForceTriggerTranslateOnEnglishPages(
             translate_prefs->GetForceTriggerOnEnglishPagesCount()) &&
         !manager->GetLanguageState().translate_enabled();
}

static void JNI_TranslateBridge_SetPredefinedTargetLanguage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    const base::android::JavaParamRef<jstring>& j_translate_language) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  const std::string translate_language(
      ConvertJavaStringToUTF8(env, j_translate_language));

  ChromeTranslateClient* client =
      ChromeTranslateClient::FromWebContents(web_contents);
  DCHECK(client);
  client->SetPredefinedTargetLanguage(translate_language);
}

// Returns the preferred target language to translate into for this user.
static base::android::ScopedJavaLocalRef<jstring>
JNI_TranslateBridge_GetTargetLanguage(JNIEnv* env) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  language::LanguageModel* language_model =
      LanguageModelManagerFactory::GetForBrowserContext(profile)
          ->GetPrimaryModel();
  DCHECK(language_model);
  PrefService* pref_service = profile->GetPrefs();
  std::string target_language =
      TranslateService::GetTargetLanguage(pref_service, language_model);
  DCHECK(!target_language.empty());
  base::android::ScopedJavaLocalRef<jstring> j_target_language =
      base::android::ConvertUTF8ToJavaString(env, target_language);
  return j_target_language;
}

// Determines whether the given language is blocked for translation.
static jboolean JNI_TranslateBridge_IsBlockedLanguage(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& j_language_string) {
  std::string language_we_might_block =
      ConvertJavaStringToUTF8(env, j_language_string);
  Profile* profile = ProfileManager::GetActiveUserProfile();
  PrefService* pref_service = profile->GetPrefs();
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(pref_service);
  DCHECK(translate_prefs);
  return translate_prefs->IsBlockedLanguage(language_we_might_block);
}

// Gets all the model languages and calls back to the Java
// TranslateBridge#addModelLanguageToSet once for each language.
static void JNI_TranslateBridge_GetModelLanguages(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& set) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  language::LanguageModel* language_model =
      LanguageModelManagerFactory::GetForBrowserContext(profile)
          ->GetPrimaryModel();
  DCHECK(language_model);
  std::string model_languages;
  std::vector<language::LanguageModel::LanguageDetails> languageDetails =
      language_model->GetLanguages();
  DCHECK(!languageDetails.empty());
  for (const auto& details : languageDetails) {
    Java_TranslateBridge_addModelLanguageToSet(
        env, set,
        base::android::ConvertUTF8ToJavaString(env, details.lang_code));
  }
}

// static
// This logic should be kept in sync with prependToAcceptLanguagesIfNecessary in
// chrome/android/java/src/org/chromium/chrome/browser/
//     physicalweb/PwsClientImpl.java
// Input |locales| is a comma separated locale representation that consists of
// language tags (BCP47 compliant format). Each language tag contains a language
// code and a country code or a language code only.
void TranslateBridge::PrependToAcceptLanguagesIfNecessary(
    const std::string& locales,
    std::string* accept_languages) {
  std::vector<std::string> locale_list =
      base::SplitString(locales + "," + *accept_languages, ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  std::set<std::string> seen_tags;
  std::vector<std::pair<std::string, std::string>> unique_locale_list;
  for (const std::string& locale_str : locale_list) {
    char locale_ID[ULOC_FULLNAME_CAPACITY] = {};
    char language_code_buffer[ULOC_LANG_CAPACITY] = {};
    char country_code_buffer[ULOC_COUNTRY_CAPACITY] = {};

    UErrorCode error = U_ZERO_ERROR;
    uloc_forLanguageTag(locale_str.c_str(), locale_ID, ULOC_FULLNAME_CAPACITY,
                        nullptr, &error);
    if (U_FAILURE(error)) {
      LOG(ERROR) << "Ignoring invalid locale representation " << locale_str;
      continue;
    }

    error = U_ZERO_ERROR;
    uloc_getLanguage(locale_ID, language_code_buffer, ULOC_LANG_CAPACITY,
                     &error);
    if (U_FAILURE(error)) {
      LOG(ERROR) << "Ignoring invalid locale representation " << locale_str;
      continue;
    }

    error = U_ZERO_ERROR;
    uloc_getCountry(locale_ID, country_code_buffer, ULOC_COUNTRY_CAPACITY,
                    &error);
    if (U_FAILURE(error)) {
      LOG(ERROR) << "Ignoring invalid locale representation " << locale_str;
      continue;
    }

    std::string language_code(language_code_buffer);
    std::string country_code(country_code_buffer);
    std::string language_tag(language_code + "-" + country_code);

    if (seen_tags.find(language_tag) != seen_tags.end())
      continue;

    seen_tags.insert(language_tag);
    unique_locale_list.push_back(std::make_pair(language_code, country_code));
  }

  // If language is not in the accept languages list, also add language
  // code. A language code should only be inserted after the last
  // languageTag that contains that language.
  // This will work with the IDS_ACCEPT_LANGUAGE localized strings bundled
  // with Chrome but may fail on arbitrary lists of language tags due to
  // differences in case and whitespace.
  std::set<std::string> seen_languages;
  std::vector<std::string> output_list;
  for (auto it = unique_locale_list.rbegin(); it != unique_locale_list.rend();
       ++it) {
    if (seen_languages.find(it->first) == seen_languages.end()) {
      output_list.push_back(it->first);
      seen_languages.insert(it->first);
    }
    if (!it->second.empty())
      output_list.push_back(it->first + "-" + it->second);
  }

  std::reverse(output_list.begin(), output_list.end());
  *accept_languages = base::JoinString(output_list, ",");
}

static void JNI_TranslateBridge_ResetAcceptLanguages(
    JNIEnv* env,
    const JavaParamRef<jstring>& default_locale) {
  std::string accept_languages(l10n_util::GetStringUTF8(IDS_ACCEPT_LANGUAGES));
  std::string locale_string(ConvertJavaStringToUTF8(env, default_locale));

  TranslateBridge::PrependToAcceptLanguagesIfNecessary(locale_string,
                                                       &accept_languages);
  GetPrefService()->SetString(language::prefs::kAcceptLanguages,
                              accept_languages);
}

static void JNI_TranslateBridge_GetChromeAcceptLanguages(
    JNIEnv* env,
    const JavaParamRef<jobject>& list) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService());

  std::vector<translate::TranslateLanguageInfo> languages;
  std::string app_locale = g_browser_process->GetApplicationLocale();
  translate_prefs->GetLanguageInfoList(
      app_locale, translate_prefs->IsTranslateAllowedByPolicy(), &languages);

  language::ToTranslateLanguageSynonym(&app_locale);
  for (const auto& info : languages) {
    // If the language comes from the same language family as the app locale,
    // translate for this language won't be supported on this device.
    std::string lang_code = info.code;
    language::ToTranslateLanguageSynonym(&lang_code);
    bool supports_translate =
        info.supports_translate && lang_code != app_locale;

    Java_TranslateBridge_addNewLanguageItemToList(
        env, list, ConvertUTF8ToJavaString(env, info.code),
        ConvertUTF8ToJavaString(env, info.display_name),
        ConvertUTF8ToJavaString(env, info.native_display_name),
        supports_translate);
  }
}

static void JNI_TranslateBridge_GetUserAcceptLanguages(
    JNIEnv* env,
    const JavaParamRef<jobject>& list) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService());

  std::vector<std::string> languages;
  translate_prefs->GetLanguageList(&languages);
  Java_TranslateBridge_copyStringArrayToList(
      env, list, ToJavaArrayOfStrings(env, languages));
}

static void JNI_TranslateBridge_SetLanguageOrder(
    JNIEnv* env,
    const JavaParamRef<jobjectArray>& j_order) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService());
  std::vector<std::string> order;
  const int num_langs = (*env).GetArrayLength(j_order);
  for (int i = 0; i < num_langs; i++) {
    jstring string = (jstring)(*env).GetObjectArrayElement(j_order, i);
    order.push_back((*env).GetStringUTFChars(string, nullptr));
  }
  translate_prefs->SetLanguageOrder(order);
}

static void JNI_TranslateBridge_UpdateUserAcceptLanguages(
    JNIEnv* env,
    const JavaParamRef<jstring>& language,
    jboolean is_add) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService());
  std::string language_code(ConvertJavaStringToUTF8(env, language));

  if (is_add) {
    translate_prefs->AddToLanguageList(language_code, false /*force_blocked=*/);
  } else {
    translate_prefs->RemoveFromLanguageList(language_code);
  }
}

static void JNI_TranslateBridge_MoveAcceptLanguage(
    JNIEnv* env,
    const JavaParamRef<jstring>& language,
    jint offset) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService());

  std::vector<std::string> languages;
  translate_prefs->GetLanguageList(&languages);

  std::string language_code(ConvertJavaStringToUTF8(env, language));

  translate::TranslatePrefs::RearrangeSpecifier where =
      translate::TranslatePrefs::kNone;

  if (offset > 0) {
    where = translate::TranslatePrefs::kDown;
  } else {
    offset = -offset;
    where = translate::TranslatePrefs::kUp;
  }

  translate_prefs->RearrangeLanguage(language_code, where, offset, languages);
}

static jboolean JNI_TranslateBridge_IsBlockedLanguage2(
    JNIEnv* env,
    const JavaParamRef<jstring>& language) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService());

  std::string language_code(ConvertJavaStringToUTF8(env, language));
  language::ToTranslateLanguageSynonym(&language_code);

  // Application language is always blocked.
  std::string app_locale = g_browser_process->GetApplicationLocale();
  language::ToTranslateLanguageSynonym(&app_locale);
  if (app_locale == language_code)
    return true;

  return translate_prefs->IsBlockedLanguage(language_code);
}

static void JNI_TranslateBridge_SetLanguageBlockedState(
    JNIEnv* env,
    const JavaParamRef<jstring>& language,
    jboolean blocked) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService());
  std::string language_code(ConvertJavaStringToUTF8(env, language));

  if (blocked) {
    translate_prefs->BlockLanguage(language_code);
  } else {
    translate_prefs->UnblockLanguage(language_code);
  }
}

static jboolean JNI_TranslateBridge_GetExplicitLanguageAskPromptShown(
    JNIEnv* env) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService());
  return translate_prefs->GetExplicitLanguageAskPromptShown();
}

static void JNI_TranslateBridge_SetExplicitLanguageAskPromptShown(
    JNIEnv* env,
    jboolean shown) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService());
  translate_prefs->SetExplicitLanguageAskPromptShown(shown);
}
