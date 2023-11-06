// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OMNIBOX_AUTOCOMPLETE_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_OMNIBOX_AUTOCOMPLETE_CONTROLLER_ANDROID_H_

#include <memory>

#include "base/android/jni_android.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_input.h"

class AutocompleteResult;
class ChromeAutocompleteProviderClient;
class Profile;

// The native part of the Java AutocompleteController class.
class AutocompleteControllerAndroid : public AutocompleteController::Observer {
 public:
  AutocompleteControllerAndroid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcontroller,
      Profile* profile,
      std::unique_ptr<ChromeAutocompleteProviderClient> client);

  AutocompleteControllerAndroid(const AutocompleteControllerAndroid&) = delete;
  AutocompleteControllerAndroid& operator=(
      const AutocompleteControllerAndroid&) = delete;

  // Methods that forward to AutocompleteController:
  void Start(JNIEnv* env,
             const base::android::JavaRef<jstring>& j_text,
             jint j_cursor_pos,
             const base::android::JavaRef<jstring>& j_desired_tld,
             const base::android::JavaRef<jstring>& j_current_url,
             jint j_page_classification,
             bool prevent_inline_autocomplete,
             bool prefer_keyword,
             bool allow_exact_keyword_match,
             bool want_asynchronous_matches);
  void StartPrefetch(JNIEnv* env,
                     const base::android::JavaRef<jstring>& j_current_url,
                     jint j_page_classification);
  base::android::ScopedJavaLocalRef<jobject> Classify(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& j_text);
  void OnOmniboxFocused(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& j_omnibox_text,
      const base::android::JavaParamRef<jstring>& j_current_url,
      jint j_page_classification,
      const base::android::JavaParamRef<jstring>& j_current_title);
  void Stop(JNIEnv* env, bool clear_result);
  void ResetSession(JNIEnv* env);

  void OnSuggestionSelected(
      JNIEnv* env,
      uintptr_t match_ptr,
      int suggestion_line,
      const jint j_window_open_disposition,
      const base::android::JavaParamRef<jstring>& j_current_url,
      jint j_page_classification,
      jlong elapsed_time_since_first_modified,
      jint completed_length,
      const base::android::JavaParamRef<jobject>& j_web_contents);
  jboolean OnSuggestionTouchDown(
      JNIEnv* env,
      uintptr_t match_ptr,
      int match_index,
      const base::android::JavaParamRef<jobject>& j_web_contents);
  void DeleteMatch(JNIEnv* env, uintptr_t match_ptr);
  void DeleteMatchElement(JNIEnv* env, uintptr_t match_ptr, jint element_index);
  base::android::ScopedJavaLocalRef<jobject>
  UpdateMatchDestinationURLWithAdditionalAssistedQueryStats(
      JNIEnv* env,
      uintptr_t match_ptr,
      jlong elapsed_time_since_input_change);
  base::android::ScopedJavaLocalRef<jobject> GetMatchingTabForSuggestion(
      JNIEnv* env,
      uintptr_t match_ptr);

  // Pass detected voice matches down to VoiceSuggestionsProvider.
  void SetVoiceMatches(
      JNIEnv* env,
      const base::android::JavaParamRef<jobjectArray>& j_voice_matches,
      const base::android::JavaParamRef<jfloatArray>& j_confidence_scores);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() const;

  // Called by Java to destroy this instance.
  void Destroy(JNIEnv*);

 private:
  ~AutocompleteControllerAndroid() override;

  // AutocompleteController::Observer implementation.
  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override;

  // Notifies the Java AutocompleteController that suggestions were received
  // based on the text the user typed in last.
  void NotifySuggestionsReceived(
      const AutocompleteResult& autocomplete_result);

  // Prepare renderer process. Called in zero-prefix context.
  // This call may get triggered multiple time during User interaction with the
  // Omnibox - these requests are deduplicated down the call chain.
  void WarmUpRenderProcess() const;

  // Last input we sent to the autocomplete controller.
  AutocompleteInput input_{};

  // Whether we're currently inside a call to Start() that's called
  // from Classify().
  bool inside_synchronous_start_{false};

  // The Profile associated with this instance of AutocompleteControllerAndroid.
  // There should be only one instance of AutocompleteControllerAndroid per
  // Profile. This is orchestrated by AutocompleteControllerFactory java class.
  // Guaranteed to be non-null.
  const raw_ptr<Profile> profile_;

  // Direct reference to AutocompleteController java class. Kept for as long as
  // this instance of AutocompleteControllerAndroid lives: until corresponding
  // Profile gets destroyed.
  // Destruction of Profile triggers destruction of both
  // C++ AutocompleteControllerAndroid and Java AutocompleteController objects.
  // Guaranteed to be non-null.
  const base::android::ScopedJavaGlobalRef<jobject> java_controller_;

  // AutocompleteController associated with this client. As this is directly
  // associated with the |provider_client_| and indirectly with |profile_|
  // there is exactly one instance per class.
  // Retained throughout the lifetime of the AutocompleteControllerAndroid.
  // Invalidated only immediately before the AutocompleteControllerAndroid is
  // destroyed.
  std::unique_ptr<AutocompleteController> autocomplete_controller_;

  // Factory used to create asynchronously invoked callbacks.
  // Retained throughout the lifetime of the AutocompleteControllerAndroid.
  const base::WeakPtrFactory<AutocompleteControllerAndroid> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_ANDROID_OMNIBOX_AUTOCOMPLETE_CONTROLLER_ANDROID_H_
