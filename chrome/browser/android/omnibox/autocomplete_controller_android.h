// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OMNIBOX_AUTOCOMPLETE_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_OMNIBOX_AUTOCOMPLETE_CONTROLLER_ANDROID_H_

#include <memory>
#include <string>

#include "base/android/jni_weak_ref.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

class AutocompleteController;
struct AutocompleteMatch;
class AutocompleteResult;
class ChromeAutocompleteProviderClient;
class Profile;

// The native part of the Java AutocompleteController class.
class AutocompleteControllerAndroid : public AutocompleteController::Observer,
                                      public KeyedService {
 public:
  explicit AutocompleteControllerAndroid(Profile* profile);

  // Methods that forward to AutocompleteController:
  void Start(JNIEnv* env,
             const base::android::JavaRef<jobject>& obj,
             const base::android::JavaRef<jstring>& j_text,
             jint j_cursor_pos,
             const base::android::JavaRef<jstring>& j_desired_tld,
             const base::android::JavaRef<jstring>& j_current_url,
             jint j_page_classification,
             bool prevent_inline_autocomplete,
             bool prefer_keyword,
             bool allow_exact_keyword_match,
             bool want_asynchronous_matches,
             const base::android::JavaRef<jstring>& j_query_tile_id,
             bool is_query_started_from_tiles);
  base::android::ScopedJavaLocalRef<jobject> Classify(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& j_text,
      bool focused_from_fakebox);
  void OnOmniboxFocused(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& j_omnibox_text,
      const base::android::JavaParamRef<jstring>& j_current_url,
      jint j_page_classification,
      const base::android::JavaParamRef<jstring>& j_current_title);
  void Stop(JNIEnv* env,
            const base::android::JavaParamRef<jobject>& obj,
            bool clear_result);
  void ResetSession(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj);
  void OnSuggestionSelected(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint selected_index,
      const jint j_window_open_disposition,
      jint hash_code,
      const base::android::JavaParamRef<jstring>& j_current_url,
      jint j_page_classification,
      jlong elapsed_time_since_first_modified,
      jint completed_length,
      const base::android::JavaParamRef<jobject>& j_web_contents);
  void DeleteSuggestion(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        jint selected_index,
                        jint hash_code);
  base::android::ScopedJavaLocalRef<jobject>
  UpdateMatchDestinationURLWithQueryFormulationTime(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint selected_index,
      jint hash_code,
      jlong elapsed_time_since_input_change,
      const base::android::JavaParamRef<jstring>& jnew_query_text,
      const base::android::JavaParamRef<jobjectArray>& jnew_query_params);
  base::android::ScopedJavaLocalRef<jobject> FindMatchingTabWithUrl(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_gurl);

  // Break the association between the AutocompleteController java and
  // native instances.
  void ReleaseJavaObject(JNIEnv* env);

  // KeyedService:
  void Shutdown() override;

  class Factory : public BrowserContextKeyedServiceFactory {
   public:
    static AutocompleteControllerAndroid* GetForProfile(Profile* profile,
                                             JNIEnv* env,
                                             jobject obj);

    static Factory* GetInstance();

   protected:
    content::BrowserContext* GetBrowserContextToUse(
        content::BrowserContext* context) const override;

   private:
    friend struct base::DefaultSingletonTraits<Factory>;

    Factory();
    ~Factory() override;

    // BrowserContextKeyedServiceFactory
    KeyedService* BuildServiceInstanceFor(
        content::BrowserContext* profile) const override;
  };

  // Pass detected voice matches down to VoiceSuggestionsProvider.
  void SetVoiceMatches(
      JNIEnv* env,
      const base::android::JavaParamRef<jobjectArray>& j_voice_matches,
      const base::android::JavaParamRef<jfloatArray>& j_confidence_scores);

 private:
  ~AutocompleteControllerAndroid() override;
  void InitJNI(JNIEnv* env, jobject obj);

  // AutocompleteController::Observer implementation.
  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override;

  // Notifies the Java AutocompleteController that suggestions were received
  // based on the text the user typed in last.
  void NotifySuggestionsReceived(
      const AutocompleteResult& autocomplete_result);

  bool IsValidMatch(JNIEnv* env, jint selected_index, jint hash_code);

  std::unique_ptr<AutocompleteController> autocomplete_controller_;

  // Last input we sent to the autocomplete controller.
  AutocompleteInput input_;

  // Whether we're currently inside a call to Start() that's called
  // from Classify().
  bool inside_synchronous_start_;

  JavaObjectWeakGlobalRef weak_java_autocomplete_controller_android_;
  Profile* profile_;
  ChromeAutocompleteProviderClient* provider_client_;

  // Whether the omnibox input is a query that starts building
  // by clicking on an image tile.
  bool is_query_started_from_tiles_ = false;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteControllerAndroid);
};

#endif  // CHROME_BROWSER_ANDROID_OMNIBOX_AUTOCOMPLETE_CONTROLLER_ANDROID_H_
