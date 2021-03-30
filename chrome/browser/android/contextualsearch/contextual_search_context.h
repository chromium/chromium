// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_CONTEXT_H_
#define CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_CONTEXT_H_

#include <string>

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "url/gurl.h"

// Encapsulates key parts of a Contextual Search Context, including surrounding
// text. This is the native implementation of the Java ContextualSearchContext.
struct ContextualSearchContext {
 public:
  // Languages used for translation.
  struct TranslationLanguages {
    std::string detected_language;
    std::string target_language;
    std::string fluent_languages;
  };

  ContextualSearchContext(JNIEnv* env, jobject obj);
  // Constructor for tests.
  ContextualSearchContext(const std::string& home_country,
                          const GURL& page_url,
                          const std::string& encoding);
  ~ContextualSearchContext();

  // Calls the destructor.  Should be called when this native object is no
  // longer needed.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Returns the native |ContextualSearchContext| given the Java object.
  static base::WeakPtr<ContextualSearchContext> FromJavaContextualSearchContext(
      const base::android::JavaRef<jobject>& j_contextual_search_context);

  // Returns whether this context can be resolved.
  // The context can be resolved only after calling SetResolveProperites.
  bool CanResolve() const;

  // Returns whether the base page URL may be sent (according to the Java
  // policy).
  bool CanSendBasePageUrl() const;

  // Sets the properties needed to resolve a context.
  void SetResolveProperties(
      JNIEnv* env,
      jobject obj,
      const base::android::JavaParamRef<jstring>& j_home_country,
      jboolean j_may_send_base_page_url,
      jlong j_previous_event_id,
      jint j_previous_event_results);

  // Adjust the current selection offsets by the given signed amounts.
  void AdjustSelection(JNIEnv* env,
                       jobject obj,
                       jint j_start_adjust,
                       jint j_end_adjust);

  void SetContent(JNIEnv* env,
                  jobject obj,
                  const base::android::JavaParamRef<jstring>& j_content,
                  jint j_selection_start,
                  jint j_selection_end);

  // Gets the URL of the base page.
  const GURL GetBasePageUrl() const;
  // Sets the URL of the base page.
  void SetBasePageUrl(const GURL& base_page_url);

  // Gets the encoding of the base page.  This is not very important, since
  // the surrounding text stored here in a std::u16string is implicitly encoded
  // in UTF-16 (see http://www.chromium.org/developers/chromium-string-usage).
  const std::string GetBasePageEncoding() const;
  void SetBasePageEncoding(const std::string& base_page_encoding);

  // Gets the country code of the home country of the user, or an empty string.
  const std::string GetHomeCountry() const;

  // Sets the selection and surroundings.
  void SetSelectionSurroundings(int start_offset,
                                int end_offset,
                                const std::u16string& surrounding_text);

  // Gets the text surrounding the selection (including the selection).
  const std::u16string GetSurroundingText() const;

  // Gets the start offset of the selection within the surrounding text (in
  // characters).
  int GetStartOffset() const;
  // Gets the end offset of the selection within the surrounding text (in
  // characters).
  int GetEndOffset() const;

  int64_t GetPreviousEventId() const;
  int GetPreviousEventResults() const;

  // Prepares the context to be used in a resolve request by supplying last
  // minute parameters.
  // |j_is_exact_resolve| indicates if the resolved term should be an exact
  // match for the selection range instead of an expandable selection.
  // |j_related_searches_stamp| is a value to stamp onto search URLs to
  // identify related searches. If the string is empty then Related Searches
  // are not being requested.
  void PrepareToResolve(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean j_is_exact_resolve,
      const base::android::JavaParamRef<jstring>& j_related_searches_stamp);

  // Returns whether the resolve request is for an exact match instead of an
  // expandable term.
  bool GetExactResolve() const;

  // Detects the language of the context using CLD from the translate utility.
  base::android::ScopedJavaLocalRef<jstring> DetectLanguage(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj) const;

  // Sets the languages to remember for use in translation.
  // See |GetTranslationLanguages|.
  void SetTranslationLanguages(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& j_detected_language,
      const base::android::JavaParamRef<jstring>& j_target_language,
      const base::android::JavaParamRef<jstring>& j_fluent_languages);

  // Returns the languages to use for translation, as set by
  // |SetTranslationLanguages|.
  const TranslationLanguages& GetTranslationLanguages() const;

  // Returns whether this request should include Related Searches in the
  // response.
  bool GetRelatedSearches() const;

  // Get the logging information stamp for Related Searches requests or the
  // empty string if the feature is not enabled.
  const std::string GetRelatedSearchesStamp() const;

  // Gets a WeakPtr to this instance.
  base::WeakPtr<ContextualSearchContext> GetWeakPtr();

 private:
  // Gets the reliable language of the given |contents| using CLD, or an empty
  // string if none can reliably be determined.
  std::string GetReliableLanguage(const std::u16string& contents) const;

  // Gets the selection, or an empty string if none.
  std::u16string GetSelection() const;

  bool can_resolve_ = false;
  bool can_send_base_page_url_ = false;
  std::string home_country_;
  GURL base_page_url_;
  std::string base_page_encoding_;
  std::u16string surrounding_text_;
  int start_offset_ = 0;
  int end_offset_ = 0;
  int64_t previous_event_id_ = 0L;
  int previous_event_results_ = 0;
  bool is_exact_resolve_ = false;
  TranslationLanguages translation_languages_;
  bool do_related_searches_ = false;
  std::string related_searches_stamp_;

  // The linked Java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  // Member variables should appear before the WeakPtrFactory, to ensure
  // that any WeakPtrs to this instance are invalidated before its members
  // variable's destructors are executed, rendering them invalid.
  base::WeakPtrFactory<ContextualSearchContext> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ContextualSearchContext);
};

#endif  // CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_CONTEXT_H_
