// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_SIMPLE_SEARCH_TERM_RESOLVER_H_
#define CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_SIMPLE_SEARCH_TERM_RESOLVER_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/android/contextualsearch/contextual_search_context.h"
#include "chrome/browser/android/contextualsearch/contextual_search_delegate.h"
#include "components/contextual_search/content/browser/contextual_search_js_api_handler.h"
#include "components/contextual_search/content/common/mojom/contextual_search_js_api_service.mojom.h"

class SimpleSearchTermResolver {
 public:
  // Constructs a native resolver associated with the Java instance.
  SimpleSearchTermResolver(JNIEnv* env,
                           const base::android::JavaRef<jobject>& obj);
  ~SimpleSearchTermResolver();

  // Called by the Java owner when it is being destroyed.
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

 private:
  void OnSearchTermResolutionResponse(
      const ResolvedSearchTerm& resolved_search_term);

  void OnTextSurroundingSelectionAvailable(
      const std::string& encoding,
      const base::string16& surrounding_text,
      size_t start_offset,
      size_t end_offset);

  // Our global reference to the Java instance.
  base::android::ScopedJavaGlobalRef<jobject> java_instance_;

  // The delegate we're using the do the real work.
  std::unique_ptr<ContextualSearchDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(SimpleSearchTermResolver);
};

#endif  // CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_SIMPLE_SEARCH_TERM_RESOLVER_H_
