// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/android/translate_bridge.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/adapters.h"
#include "base/strings/string_split.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/language/language_model_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
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
#include "components/translate/core/browser/translate_metrics_logger.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "content/public/browser/web_contents.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "ui/base/l10n/l10n_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/language/android/jni_headers/TranslateBridge_jni.h"
#include "chrome/browser/language/android/jni_headers/TranslationObserver_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;

namespace {

PrefService* GetPrefService(const base::android::JavaRef<jobject>& j_profile) {
  return Profile::FromJavaObject(j_profile)->GetPrefs();
}

class TranslationObserver
    : public translate::ContentTranslateDriver::TranslationObserver {
 public:
  TranslationObserver(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& j_observer)
      : env_(env), j_observer_(j_observer) {}

  void OnIsPageTranslatedChanged(content::WebContents* source) override {
    ScopedJavaLocalRef<jobject> jsource_contents;
    if (source) {
      jsource_contents = source->GetJavaWebContents();
    }
    Java_TranslationObserver_onIsPageTranslatedChanged(env_, j_observer_,
                                                       jsource_contents);
  }

  void OnPageTranslated(const std::string& source_lang,
                        const std::string& translated_lang,
                        translate::TranslateErrors error_type) override {
    Java_TranslationObserver_onPageTranslated(
        env_, j_observer_,
        base::android::ConvertUTF8ToJavaString(env_, source_lang),
        base::android::ConvertUTF8ToJavaString(env_, translated_lang),
        static_cast<int>(error_type));
  }

 private:
  raw_ptr<JNIEnv> env_;
  ScopedJavaGlobalRef<jobject> j_observer_;
};

}  // namespace

static ChromeTranslateClient* GetTranslateClient(
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  ChromeTranslateClient* client =
      ChromeTranslateClient::FromWebContents(web_contents);
  CHECK(client);
  return client;
}

static void JNI_TranslateBridge_ManualTranslateWhenReady(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  ChromeTranslateClient* client =
      ChromeTranslateClient::FromWebContents(web_contents);
  CHECK(client);
  client->ManualTranslateWhenReady();
}

static jboolean JNI_TranslateBridge_CanManuallyTranslate(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    jboolean menuLogging) {
  ChromeTranslateClient* client = GetTranslateClient(j_web_contents);
  translate::TranslateManager* manager = client->GetTranslateManager();
  CHECK(manager);
  return manager->CanManuallyTranslate(menuLogging);
}

static jboolean JNI_TranslateBridge_ShouldShowManualTranslateIPH(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  ChromeTranslateClient* client = GetTranslateClient(j_web_contents);
  translate::TranslateManager* manager = client->GetTranslateManager();
  CHECK(manager);

  const std::string page_lang = manager->GetLanguageState()->source_language();
  std::unique_ptr<translate::TranslatePrefs> translate_prefs(
      client->GetTranslatePrefs());

  return base::StartsWith(page_lang, "en",
                          base::CompareCase::INSENSITIVE_ASCII) &&
         !translate_prefs->ShouldForceTriggerTranslateOnEnglishPages() &&
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
  CHECK(client);
  client->SetPredefinedTargetLanguage(translate_language,
                                      j_should_auto_translate);
}

// Returns the preferred target language to translate into for this user.
static base::android::ScopedJavaLocalRef<jstring>
JNI_TranslateBridge_GetTargetLanguage(JNIEnv* env,
                                      const JavaParamRef<jobject>& j_profile) {
  Profile* profile = Profile::FromJavaObject(j_profile);
  language::LanguageModel* language_model =
      LanguageModelManagerFactory::GetForBrowserContext(profile)
          ->GetPrimaryModel();
  CHECK(language_model);
  PrefService* pref_service = profile->GetPrefs();
  std::string target_language =
      TranslateService::GetTargetLanguage(pref_service, language_model);
  CHECK(!target_language.empty());
  base::android::ScopedJavaLocalRef<jstring> j_target_language =
      base::android::ConvertUTF8ToJavaString(env, target_language);
  return j_target_language;
}

// Set the default target language to translate into for this user.
static void JNI_TranslateBridge_SetDefaultTargetLanguage(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jstring>& j_target_language) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService(j_profile));
  std::string target_language(ConvertJavaStringToUTF8(env, j_target_language));
  translate_prefs->SetRecentTargetLanguage(target_language);
}

// Determines whether the given language is blocked for translation.
static jboolean JNI_TranslateBridge_IsBlockedLanguage(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    const base::android::JavaParamRef<jstring>& j_language_string) {
  std::string language_code(ConvertJavaStringToUTF8(env, j_language_string));
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService(j_profile));
  CHECK(translate_prefs);
  return translate_prefs->IsBlockedLanguage(language_code);
}

// Gets all languages that should always be translated as a Java List.
static ScopedJavaLocalRef<jobjectArray>
JNI_TranslateBridge_GetAlwaysTranslateLanguages(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService(j_profile));
  return ToJavaArrayOfStrings(env,
                              translate_prefs->GetAlwaysTranslateLanguages());
}

// Gets all languages for which translation should not be prompted.
static ScopedJavaLocalRef<jobjectArray>
JNI_TranslateBridge_GetNeverTranslateLanguages(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService(j_profile));
  return ToJavaArrayOfStrings(env,
                              translate_prefs->GetNeverTranslateLanguages());
}

// Sets the always translate state for a language.
// The always translate language list is actually a dict mapping
// source_language -> target_language.  We use the current target language when
// adding |language| to the dict.
static void JNI_TranslateBridge_SetLanguageAlwaysTranslateState(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jstring>& language,
    jboolean alwaysTranslate) {
  std::string language_code(ConvertJavaStringToUTF8(env, language));
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService(j_profile));

  if (alwaysTranslate) {
    Profile* profile = Profile::FromJavaObject(j_profile);
    language::LanguageModel* language_model =
        LanguageModelManagerFactory::GetForBrowserContext(profile)
            ->GetPrimaryModel();
    CHECK(language_model);
    PrefService* pref_service = profile->GetPrefs();
    std::string target_language =
        TranslateService::GetTargetLanguage(pref_service, language_model);
    CHECK(!target_language.empty());
    translate_prefs->AddLanguagePairToAlwaysTranslateList(language_code,
                                                          target_language);
  } else {
    translate_prefs->RemoveLanguagePairFromAlwaysTranslateList(language_code);
  }
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
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jstring>& default_locale) {
  std::string accept_languages(l10n_util::GetStringUTF8(IDS_ACCEPT_LANGUAGES));
  std::string locale_string(ConvertJavaStringToUTF8(env, default_locale));

  TranslateBridge::PrependToAcceptLanguagesIfNecessary(locale_string,
                                                       &accept_languages);
  GetPrefService(j_profile)->SetString(language::prefs::kSelectedLanguages,
                                       accept_languages);
}

static void JNI_TranslateBridge_GetChromeAcceptLanguages(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jobject>& list) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService(j_profile));

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
JNI_TranslateBridge_GetUserAcceptLanguages(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService(j_profile));

  std::vector<std::string> languages;
  translate_prefs->GetLanguageList(&languages);
  return ToJavaArrayOfStrings(env, languages);
}

static void JNI_TranslateBridge_SetLanguageOrder(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jobjectArray>& j_order) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService(j_profile));
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
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jstring>& language,
    jboolean is_add) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService(j_profile));
  std::string language_code(ConvertJavaStringToUTF8(env, language));

  if (is_add) {
    translate_prefs->AddToLanguageList(language_code, false /*force_blocked=*/);
  } else {
    translate_prefs->RemoveFromLanguageList(language_code);
  }
}

static void JNI_TranslateBridge_MoveAcceptLanguage(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jstring>& language,
    jint offset) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService(j_profile));

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
    const JavaParamRef<jobject>& j_profile,
    const JavaParamRef<jstring>& language,
    jboolean blocked) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService(j_profile));
  std::string language_code(ConvertJavaStringToUTF8(env, language));

  if (blocked) {
    translate_prefs->BlockLanguage(language_code);
  } else {
    translate_prefs->UnblockLanguage(language_code);
  }
}

static jboolean JNI_TranslateBridge_GetAppLanguagePromptShown(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService(j_profile));
  return translate_prefs->GetAppLanguagePromptShown();
}

static void JNI_TranslateBridge_SetAppLanguagePromptShown(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile) {
  std::unique_ptr<translate::TranslatePrefs> translate_prefs =
      ChromeTranslateClient::CreateTranslatePrefs(GetPrefService(j_profile));
  translate_prefs->SetAppLanguagePromptShown();
}

static base::android::ScopedJavaLocalRef<jstring>
JNI_TranslateBridge_GetCurrentLanguage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  ChromeTranslateClient* client =
      ChromeTranslateClient::FromWebContents(web_contents);
  CHECK(client);
  const std::string& current_language_code =
      client->GetLanguageState().current_language();
  base::android::ScopedJavaLocalRef<jstring> j_current_language =
      base::android::ConvertUTF8ToJavaString(env, current_language_code);
  return j_current_language;
}

static jboolean JNI_TranslateBridge_IsPageTranslated(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  ChromeTranslateClient* client =
      ChromeTranslateClient::FromWebContents(web_contents);
  CHECK(client);
  return client->GetLanguageState().IsPageTranslated();
}

static jlong JNI_TranslateBridge_AddTranslationObserver(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    const base::android::JavaParamRef<jobject>& j_observer) {
  auto* observer = new TranslationObserver(env, j_observer);
  GetTranslateClient(j_web_contents)
      ->translate_driver()
      ->AddTranslationObserver(observer);
  return reinterpret_cast<jlong>(observer);
}

static void JNI_TranslateBridge_RemoveTranslationObserver(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    jlong j_observer_native_ptr) {
  TranslationObserver* observer =
      reinterpret_cast<TranslationObserver*>(j_observer_native_ptr);
  GetTranslateClient(j_web_contents)
      ->translate_driver()
      ->RemoveTranslationObserver(observer);
  delete observer;
}

static void JNI_TranslateBridge_SetIgnoreMissingKeyForTesting(  // IN-TEST
    JNIEnv* env,
    jboolean ignore) {
  translate::TranslateManager::SetIgnoreMissingKeyForTesting(  // IN-TEST
      ignore);
}
