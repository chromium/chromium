// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTINUOUS_SEARCH_INTERNAL_SEARCH_RESULT_EXTRACTOR_PRODUCER_INTERFACE_H_
#define CHROME_BROWSER_CONTINUOUS_SEARCH_INTERNAL_SEARCH_RESULT_EXTRACTOR_PRODUCER_INTERFACE_H_

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"

namespace continuous_search {

// Interface abstracting the JNI for SearchResultExtractorProducer
// CalledByNative methods.
class SearchResultExtractorProducerInterface {
 public:
  virtual ~SearchResultExtractorProducerInterface() = default;

  // Returns `status_code` to native. `obj` is an instance of
  // SearchResultExtractorProducer.
  virtual void OnError(JNIEnv* env,
                       const base::android::JavaRef<jobject>& obj,
                       jint status_code) = 0;

  // Returns the contents of a Java SearchResultMetadata from native data. `obj`
  // is an instance of SearchResultExtractorProducer.
  virtual void OnResultsAvailable(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& obj,
      const base::android::JavaRef<jobject>& url,
      const base::android::JavaRef<jstring>& query,
      jint result_type,
      const base::android::JavaRef<jobjectArray>& group_label,
      const base::android::JavaRef<jbooleanArray>& is_ad_group,
      const base::android::JavaRef<jintArray>& group_size,
      const base::android::JavaRef<jobjectArray>& titles,
      const base::android::JavaRef<jobjectArray>& urls) = 0;
};

}  // namespace continuous_search

#endif  // CHROME_BROWSER_CONTINUOUS_SEARCH_INTERNAL_SEARCH_RESULT_EXTRACTOR_PRODUCER_INTERFACE_H_
