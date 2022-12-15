// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/android/translate_bridge.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/containers/adapters.h"
#include "base/strings/string_split.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/language/android/jni_headers/TranslateBridge_jni.h"
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
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_metrics_logger.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "content/public/browser/web_contents.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
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

static void JNI_TranslateBridge_TranslateToLanguage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    const base::android::JavaParamRef<jstring>& j_target_language_code) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  ChromeTranslateClient* client =
      ChromeTranslateClient::FromWebContents(web_contents);
  DCHECK(client);
  const std::string target_language_code(
      ConvertJavaStringToUTF8(env, j_target_language_code));
  const std::string& source_language_code =
      client->GetLanguageState().source_language();
  if (source_language_code.empty()) {
    // TODO(crbug.com/1181400): Add support for specifying a target language for
    // ManualTranslateWhenReady().
    return;
  }

  translate::TranslateManager* manager = client->GetTranslateManager();
  DCHECK(manager);
  if (!translate::TranslateDownloadManager::IsSupportedLanguage(
          target_language_code)) {
    // If the requested target language isn't supported, show the infobar but
    // don't start translating. If the infobar is already visible, this will
    // leave it in its current state.
    manager->ShowTranslateUI(/*auto_translate=*/false,
                             /*triggered_from_menu=*/false);
  } else {
    // We don't check for source_language_code support because TranslatePage
    // handles that case already.
    manager->TranslatePage(
        source_language_code, target_language_code,
        /*triggered_from_menu=*/false,
        /*translation_type=*/
        manager->GetActiveTranslateMetricsLogger()
            ->GetNextManualTranslationType(
                /*is_context_menu_initiated_translation=*/false));
  }
}

static jboolean JNI_TranslateBridge_CanManuallyTranslate(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    jboolean menuLogging) {
  ChromeTranslateClient* client = GetTranslateClient(j_web_contents);
  translate::TranslateManager* manager = client->GetTranslateManager();
  DCHECK(manager);
  return manager->CanManuallyTranslate(menuLogging);
}

static jboolean JNI_TranslateBridge_ShouldShowManualTranslateIPH(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  ChromeTranslateClient* client = GetTranslateClient(j_web_contents);
  translate::TranslateManager* manager = client->GetTranslateManager();
  DCHECK(manager);

  const std::string page_lang = manager->GetLanguageState()->source_language();
  std::unique_ptr<translate::TranslatePrefs> translate_prefs(
      client->GetTranslatePrefs());

  return base::StartsWith(page_lang, "en",
                          base::CompareCase::INSENSITIVE_ASCII) &&
         !language::ShouldForceTriggerTranslateOnEnglishPages(
             translate_prefs->GetForceTriggerOnEnglishPagesCount()) &&
         !manager->GetLanguageState()->translate_enabled();
}

static void JNI_TranslateBridge_SetPredefinedTargetLanguage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    const base::android::JavaParamRef<jstring>& j_translate_language,
    jboolean j_should_auto_translate) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  const std::string translate_language(
      ConvertJavaStringToUTF8(env, j_translate_language));

  ChromeTranslateClient* client =
      ChromeTranslateClient::FromWebContents(web_contents);
  DCHECK(client);
  client->SetPredefinedTargetLanguage(translate_language,
                                      j_should_auto_translate);
}

static base::android::ScopedJavaLocalRef<jstring>
JNI_TranslateBridge_GetSourceLanguage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  ChromeTranslateClient* client =
      ChromeTranslateClient::FromWebContents(web_contents);
  DCHECK(client);
  const std::string& source_language_code =
      client->GetLanguageState().source_language();
  DCHECK(!source_language_code.empty());
  base::android::ScopedJavaLocalRef<jstring> j_source_language =
      base::android::ConvertUTF8ToJavaString(env, source_language_code);
  return j_source_language;
}

static base::android::ScopedJavaLocalRef<jstring>
JNI_TranslateBridge_GetCurrentLanguage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  ChromeTranslateClient* client =
      ChromeTranslateClient::FromWebContents(web_contents);
  DCHECK(client);
  const std::string& current_language_code =
      client->GetLanguageState().current_language();
  DCHECK(!current_language_code.empty());
  base::android::ScopedJavaLocalRef<jstring> j_current_language =
      base::android::ConvertUTF8ToJavaString(env, current_language_code);
  return j_current_language;
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

// Set the default target language to translate into for this user.
static void JNI_TranslateBridge_SetDefaultTargetLanguage(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_target_language) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService());
  std::string target_language(ConvertJavaStringToUTF8(env, j_target_language));
  translate_prefs->SetRecentTargetLanguage(target_language);
}

// Determines whether the given language is blocked for translation.
static jboolean JNI_TranslateBridge_IsBlockedLanguage(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& j_language_string) {
  std::string language_code(ConvertJavaStringToUTF8(env, j_language_string));
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService());
  DCHECK(translate_prefs);
  return translate_prefs->IsBlockedLanguage(language_code);
}

// Gets all languages that should always be translated as a Java List.
static ScopedJavaLocalRef<jobjectArray>
JNI_TranslateBridge_GetAlwaysTranslateLanguages(JNIEnv* env) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService());
  return ToJavaArrayOfStrings(env,
                              translate_prefs->GetAlwaysTranslateLanguages());
}

// Gets all languages for which translation should not be prompted.
static ScopedJavaLocalRef<jobjectArray>
JNI_TranslateBridge_GetNeverTranslateLanguages(JNIEnv* env) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService());
  return ToJavaArrayOfStrings(env,
                              translate_prefs->GetNeverTranslateLanguages());
}

// Sets the always translate state for a language.
// The always translate language list is actually a dict mapping
// source_language -> target_language.  We use the current target language when
// adding |language| to the dict.
static void JNI_TranslateBridge_SetLanguageAlwaysTranslateState(
    JNIEnv* env,
    const JavaParamRef<jstring>& language,
    jboolean alwaysTranslate) {
  std::string language_code(ConvertJavaStringToUTF8(env, language));
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService());

  translate_prefs->SetLanguageAlwaysTranslateState(language_code,
                                                   alwaysTranslate);
}

// static
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
  for (const auto& [language_code, country_code] :
       base::Reversed(unique_locale_list)) {
    if (seen_languages.find(language_code) == seen_languages.end()) {
      output_list.push_back(language_code);
      seen_languages.insert(language_code);
    }
    if (!country_code.empty())
      output_list.push_back(language_code + "-" + country_code);
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
  GetPrefService()->SetString(language::prefs::kSelectedLanguages,
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

  for (const auto& info : languages) {
    Java_TranslateBridge_addNewLanguageItemToList(
        env, list, ConvertUTF8ToJavaString(env, info.code),
        ConvertUTF8ToJavaString(env, info.display_name),
        ConvertUTF8ToJavaString(env, info.native_display_name),
        info.supports_translate);
  }
}

static ScopedJavaLocalRef<jobjectArray>
JNI_TranslateBridge_GetUserAcceptLanguages(JNIEnv* env) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService());

  std::vector<std::string> languages;
  translate_prefs->GetLanguageList(&languages);
  return ToJavaArrayOfStrings(env, languages);
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

static jboolean JNI_TranslateBridge_GetAppLanguagePromptShown(JNIEnv* env) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService());
  return translate_prefs->GetAppLanguagePromptShown();
}

static void JNI_TranslateBridge_SetAppLanguagePromptShown(JNIEnv* env) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService());
  translate_prefs->SetAppLanguagePromptShown();
}

static void JNI_TranslateBridge_SetIgnoreMissingKeyForTesting(  // IN-TEST
    JNIEnv* env,
    jboolean ignore) {
  translate::TranslateManager::SetIgnoreMissingKeyForTesting(  // IN-TEST
      ignore);
}
