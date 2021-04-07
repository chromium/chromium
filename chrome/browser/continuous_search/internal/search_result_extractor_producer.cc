// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/continuous_search/internal/search_result_extractor_producer.h"

#include <memory>
#include <utility>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/containers/span.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/continuous_search/internal/jni_headers/SearchResultExtractorProducer_jni.h"
#include "chrome/browser/continuous_search/internal/search_result_extractor_producer_interface.h"
#include "chrome/browser/continuous_search/internal/search_url_helper.h"
#include "chrome/browser/continuous_search/page_category.h"
#include "content/public/browser/web_contents.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

namespace continuous_search {

class SearchResultExtractorProducerJavaInterface
    : public SearchResultExtractorProducerInterface {
 public:
  SearchResultExtractorProducerJavaInterface() = default;
  ~SearchResultExtractorProducerJavaInterface() override = default;

  SearchResultExtractorProducerJavaInterface(
      const SearchResultExtractorProducerJavaInterface&) = delete;
  SearchResultExtractorProducerJavaInterface& operator=(
      const SearchResultExtractorProducerJavaInterface&) = delete;

  void OnError(JNIEnv* env,
               const base::android::JavaRef<jobject>& obj,
               jint status_code) override {
    Java_SearchResultExtractorProducer_onError(env, obj, status_code);
  }

  void OnResultsAvailable(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& obj,
      const base::android::JavaRef<jobject>& url,
      const base::android::JavaRef<jstring>& query,
      jint result_type,
      const base::android::JavaRef<jobjectArray>& group_label,
      const base::android::JavaRef<jbooleanArray>& is_ad_group,
      const base::android::JavaRef<jintArray>& group_size,
      const base::android::JavaRef<jobjectArray>& titles,
      const base::android::JavaRef<jobjectArray>& urls) override {
    Java_SearchResultExtractorProducer_onResultsAvailable(
        env, obj, url, query, result_type, group_label, is_ad_group, group_size,
        titles, urls);
  }
};

jlong JNI_SearchResultExtractorProducer_Create(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_object) {
  SearchResultExtractorProducer* producer = new SearchResultExtractorProducer(
      env, j_object,
      std::make_unique<SearchResultExtractorProducerJavaInterface>());
  return reinterpret_cast<intptr_t>(producer);
}

SearchResultExtractorProducer::SearchResultExtractorProducer(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_object,
    std::unique_ptr<SearchResultExtractorProducerInterface> interface)
    : java_interface_(std::move(interface)) {
  java_ref_.Reset(j_object);
}

SearchResultExtractorProducer::~SearchResultExtractorProducer() = default;

void SearchResultExtractorProducer::FetchResults(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    const base::android::JavaParamRef<jstring>& j_query) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  client_.RequestData(
      web_contents,
      base::BindOnce(&SearchResultExtractorProducer::OnResultsCallback,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::android::ConvertJavaStringToUTF8(env, j_query)));
}

void SearchResultExtractorProducer::Destroy(JNIEnv* env) {
  delete this;
}

void SearchResultExtractorProducer::OnResultsCallback(
    const std::string& query,
    SearchResultExtractorClientStatus status,
    mojom::CategoryResultsPtr results) {
  base::UmaHistogramEnumeration(
      "Browser.ContinuousSearch.SearchResultExtractionStatus", status,
      SearchResultExtractorClientStatus::kMaxValue);

  JNIEnv* env = base::android::AttachCurrentThread();
  if (status != SearchResultExtractorClientStatus::kSuccess) {
    java_interface_->OnError(env, java_ref_, status);
    return;
  }

  size_t result_count = 0;
  for (const mojom::ResultGroupPtr& group : results->groups) {
    result_count += group->results.size();
  }
  base::UmaHistogramCounts100(
      "Browser.ContinuousSearch.NumberOfSearchResultsExtracted", result_count);

  std::vector<std::string> labels;
  std::vector<int> group_sizes;
  // std::vector<bool> doesn't provide data() due to its unique packing
  // implementation and there is no JNI method for returning a Java array of
  // booleans from a base::span<bool> or std::vector<bool>. This is the next
  // best option.
  std::unique_ptr<bool[]> groups_are_ad_type(new bool[results->groups.size()]);
  labels.reserve(results->groups.size());
  group_sizes.reserve(results->groups.size());

  std::vector<std::u16string> titles;
  std::vector<base::android::ScopedJavaLocalRef<jobject>> urls;
  titles.reserve(result_count);
  urls.reserve(result_count);
  for (size_t i = 0; i < results->groups.size(); ++i) {
    const mojom::ResultGroupPtr& group = results->groups[i];
    labels.push_back(group->label);
    groups_are_ad_type[i] = group->is_ad_group;
    group_sizes.push_back(group->results.size());

    for (const mojom::SearchResultPtr& result : group->results) {
      titles.push_back(result->title);
      urls.push_back(url::GURLAndroid::FromNativeGURL(env, result->link));
    }
  }

  // The SearchResultExtractorClient has already verified `document_url` matches
  // the last committed URL of the web contents when the request returned.
  // Document URL must also be a SRP url.
  DCHECK(GetSrpPageCategoryForUrl(results->document_url) !=
         PageCategory::kNone);
  java_interface_->OnResultsAvailable(
      env, java_ref_,
      url::GURLAndroid::FromNativeGURL(env, results->document_url),
      base::android::ConvertUTF8ToJavaString(env, query),
      static_cast<jint>(GetSrpPageCategoryForUrl(results->document_url)),
      base::android::ToJavaArrayOfStrings(env, labels),
      base::android::ToJavaBooleanArray(env, groups_are_ad_type.get(),
                                        results->groups.size()),
      base::android::ToJavaIntArray(env, group_sizes),
      base::android::ToJavaArrayOfStrings(env, titles),
      url::GURLAndroid::ToJavaArrayOfGURLs(env, urls));
}

}  // namespace continuous_search
