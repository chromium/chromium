// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/test_support_jni_headers/PrefetchTestBridge_jni.h"
#include "chrome/browser/android/profile_key_util.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_service_factory.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/image_fetcher/core/cache/image_cache.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_prefs.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

// Below is the native implementation of PrefetchTestBridge.java.

namespace offline_pages {
namespace prefetch {

JNI_EXPORT void JNI_PrefetchTestBridge_EnableLimitlessPrefetching(
    JNIEnv* env,
    jboolean enable) {
  prefetch_prefs::SetLimitlessPrefetchingEnabled(
      ::android::GetLastUsedRegularProfileKey()->GetPrefs(), enable != 0);
}

JNI_EXPORT jboolean
JNI_PrefetchTestBridge_IsLimitlessPrefetchingEnabled(JNIEnv* env) {
  return static_cast<jboolean>(prefetch_prefs::IsLimitlessPrefetchingEnabled(
      ::android::GetLastUsedRegularProfileKey()->GetPrefs()));
}

JNI_EXPORT void JNI_PrefetchTestBridge_InsertIntoCachedImageFetcher(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_url,
    const JavaParamRef<jbyteArray>& j_image_data) {
  image_fetcher::ImageFetcherService* service =
      ImageFetcherServiceFactory::GetForKey(
          ::android::GetLastUsedRegularProfileKey());
  DCHECK(service);
  scoped_refptr<image_fetcher::ImageCache> cache =
      service->ImageCacheForTesting();
  std::string url = base::android::ConvertJavaStringToUTF8(env, j_url);
  std::string image_data;
  base::android::JavaByteArrayToString(env, j_image_data, &image_data);

  cache->SaveImage(url, image_data, false /* needs_transcoding */,
                   absl::nullopt /* expiration_interval */);
}

JNI_EXPORT void JNI_PrefetchTestBridge_AddCandidatePrefetchURL(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_url,
    const JavaParamRef<jstring>& j_title,
    const JavaParamRef<jstring>& j_thumbnail_url,
    const JavaParamRef<jstring>& j_favicon_url,
    const JavaParamRef<jstring>& j_snippet,
    const JavaParamRef<jstring>& j_attribution) {
  GURL url = GURL(base::android::ConvertJavaStringToUTF8(env, j_url));
  std::u16string title = base::android::ConvertJavaStringToUTF16(env, j_title);
  std::string thumbnail_url =
      base::android::ConvertJavaStringToUTF8(env, j_thumbnail_url);
  std::string favicon_url =
      base::android::ConvertJavaStringToUTF8(env, j_favicon_url);
  std::string snippet = base::android::ConvertJavaStringToUTF8(env, j_snippet);
  std::string attribution =
      base::android::ConvertJavaStringToUTF8(env, j_attribution);
  std::vector<PrefetchURL> new_candidate_urls = {
      PrefetchURL(url.spec(), url, title, GURL(thumbnail_url),
                  GURL(favicon_url), snippet, attribution)};

  PrefetchServiceFactory::GetForKey(::android::GetLastUsedRegularProfileKey())
      ->GetPrefetchDispatcher()
      ->AddCandidatePrefetchURLs(kSuggestedArticlesNamespace,
                                 new_candidate_urls);
}

}  // namespace prefetch
}  // namespace offline_pages
