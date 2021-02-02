// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTINUOUS_SEARCH_INTERNAL_SEARCH_RESULT_EXTRACTOR_PRODUCER_H_
#define CHROME_BROWSER_CONTINUOUS_SEARCH_INTERNAL_SEARCH_RESULT_EXTRACTOR_PRODUCER_H_

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/continuous_search/browser/search_result_extractor_client.h"
#include "components/continuous_search/browser/search_result_extractor_client_status.h"
#include "components/continuous_search/common/public/mojom/continuous_search.mojom.h"

namespace continuous_search {
class SearchResultExtractorProducerInterface;

// C++ implementation of the Java SearchResultExtractorProducer.
class SearchResultExtractorProducer {
 public:
  SearchResultExtractorProducer(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_object,
      std::unique_ptr<SearchResultExtractorProducerInterface> interface);
  ~SearchResultExtractorProducer();

  SearchResultExtractorProducer(const SearchResultExtractorProducer&) = delete;
  SearchResultExtractorProducer& operator=(
      const SearchResultExtractorProducer&) = delete;

  // Fetches search metadata from the SRP in `j_web_contents`.
  void FetchResults(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& j_web_contents,
                    const base::android::JavaParamRef<jstring>& j_query);

  // Destroy `this` as it is owned by Java.
  void Destroy(JNIEnv* env);

 private:
  // Called when `FetchResults()` finishes.
  void OnResultsCallback(const std::string& query,
                         SearchResultExtractorClientStatus status,
                         mojom::CategoryResultsPtr results);

  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
  std::unique_ptr<SearchResultExtractorProducerInterface> java_interface_;
  SearchResultExtractorClient client_;

  base::WeakPtrFactory<SearchResultExtractorProducer> weak_ptr_factory_{this};
};

}  // namespace continuous_search

#endif  // CHROME_BROWSER_CONTINUOUS_SEARCH_INTERNAL_SEARCH_RESULT_EXTRACTOR_PRODUCER_H_
