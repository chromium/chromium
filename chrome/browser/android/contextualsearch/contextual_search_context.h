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
// text.
struct ContextualSearchContext {
 public:
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
  // the surrounding text stored here in a base::string16 is implicitly encoded
  // in UTF-16 (see http://www.chromium.org/developers/chromium-string-usage).
  const std::string GetBasePageEncoding() const;
  void SetBasePageEncoding(const std::string& base_page_encoding);

  // Gets the country code of the home country of the user, or an empty string.
  const std::string GetHomeCountry() const;

  // Sets the selection and surroundings.
  void SetSelectionSurroundings(int start_offset,
                                int end_offset,
                                const base::string16& surrounding_text);

  // Gets the text surrounding the selection (including the selection).
  const base::string16 GetSurroundingText() const;

  // Gets the start offset of the selection within the surrounding text (in
  // characters).
  int GetStartOffset() const;
  // Gets the end offset of the selection within the surrounding text (in
  // characters).
  int GetEndOffset() const;

  int64_t GetPreviousEventId() const;
  int GetPreviousEventResults() const;

  // Causes the next resolve request to be for an exact match instead of an
  // expandable term.
  void RestrictResolve(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj);

  // Detects the language of the context using CLD from the translate utility.
  base::android::ScopedJavaLocalRef<jstring> DetectLanguage(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // Gets a WeakPtr to this instance.
  base::WeakPtr<ContextualSearchContext> GetWeakPtr();

 private:
  bool can_resolve;
  bool can_send_base_page_url;

  std::string home_country;
  GURL base_page_url;
  std::string base_page_encoding;
  base::string16 surrounding_text;
  int start_offset;
  int end_offset;
  int64_t previous_event_id;
  int previous_event_results;

  // The linked Java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  // Member variables should appear before the WeakPtrFactory, to ensure
  // that any WeakPtrs to this instance are invalidated before its members
  // variable's destructors are executed, rendering them invalid.
  base::WeakPtrFactory<ContextualSearchContext> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ContextualSearchContext);
};

#endif  // CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_CONTEXT_H_
