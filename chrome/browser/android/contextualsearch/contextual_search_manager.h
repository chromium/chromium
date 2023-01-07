// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_MANAGER_H_
#define CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_MANAGER_H_

#include <jni.h>

#include <stddef.h>

#include "base/android/jni_android.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/contextual_search/core/browser/contextual_search_delegate.h"

// Manages the native extraction and request logic for Contextual Search,
// and interacts with the Java ContextualSearchManager for UX.
// Most of the work is done by the associated |ContextualSearchDelegate|.
class ContextualSearchManager {
 public:
  // Constructs a native manager associated with the Java manager.
  ContextualSearchManager(JNIEnv* env,
                          const base::android::JavaRef<jobject>& obj);

  ContextualSearchManager(const ContextualSearchManager&) = delete;
  ContextualSearchManager& operator=(const ContextualSearchManager&) = delete;

  ~ContextualSearchManager();

  // Called by the Java ContextualSearchManager when it is being destroyed.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Starts the request to get the search terms to use for the given selection,
  // by accessing our server with the content of the page (from the given
  // content view core object).
  // Any outstanding server requests are canceled.
  // When the server responds with the search term, the Java object is notified
  // by calling OnSearchTermResolutionResponse().
  void StartSearchTermResolutionRequest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_contextual_search_context,
      const base::android::JavaParamRef<jobject>& j_base_web_contents);

  // Gathers the surrounding text around the selection and saves it locally.
  // Does not send a search term resolution request to the server.
  void GatherSurroundingText(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_contextual_search_context,
      const base::android::JavaParamRef<jobject>& j_base_web_contents);

 private:
  void OnSearchTermResolutionResponse(
      const ResolvedSearchTerm& resolved_search_term);

  // Calls back to Java with notification when a sample of text surrounding the
  // selection is available.
  void OnTextSurroundingSelectionAvailable(
      const std::string& encoding,
      const std::u16string& surrounding_text,
      size_t start_offset,
      size_t end_offset);

  // Our global reference to the Java ContextualSearchManager.
  base::android::ScopedJavaGlobalRef<jobject> java_manager_;

  // The delegate we're using the do the real work.
  std::unique_ptr<ContextualSearchDelegate> delegate_;
};

#endif  // CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_MANAGER_H_
