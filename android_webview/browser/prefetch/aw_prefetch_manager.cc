// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "android_webview/browser/prefetch/aw_prefetch_manager.h"

#include <jni.h>

#include <optional>

#include "android_webview/browser/metrics/aw_metrics_service_accessor.h"
#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "android_webview/browser/prefetch/aw_prefetch_manager_data.h"
#include "android_webview/browser/prefetch/aw_preloading_utils.h"
#include "android_webview/common/aw_features.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check_is_test.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_features.h"

// Has to come after all the FromJniType() / ToJniType() headers.
#include "android_webview/browser_jni_headers/AwPrefetchManager_jni.h"
#include "third_party/blink/public/common/navigation/preloading_headers.h"

using content::BrowserThread;

namespace android_webview {

BASE_FEATURE(kWebViewPrefetchDisableBlockUntilHeadTimeout,
             base::FEATURE_ENABLED_BY_DEFAULT);

class AwPrefetchRequestStatusListener
    : public content::PrefetchRequestStatusListener {
 public:
  AwPrefetchRequestStatusListener(
      const base::android::ScopedJavaGlobalRef<jobject>
          prefetch_manager_java_object,
      const base::android::JavaRef<jobject>& callback,
      const base::android::JavaRef<jobject>& callback_executor)
      : prefetch_manager_java_object_(prefetch_manager_java_object),
        prefetch_java_callback_(callback),
        prefetch_java_callback_executor_(callback_executor) {
    CHECK(prefetch_manager_java_object_);
    CHECK(prefetch_java_callback_);
    CHECK(prefetch_java_callback_executor_);
  }
  ~AwPrefetchRequestStatusListener() override = default;

  void OnPrefetchStartFailedGeneric() override {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AwPrefetchManager_onPrefetchStartFailedGeneric(
        env, prefetch_manager_java_object_, prefetch_java_callback_,
        prefetch_java_callback_executor_);
  }

  void OnPrefetchStartFailedDuplicate() override {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AwPrefetchManager_onPrefetchStartFailedDuplicate(
        env, prefetch_manager_java_object_, prefetch_java_callback_,
        prefetch_java_callback_executor_);
  }

  void OnPrefetchResponseCompleted() override {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AwPrefetchManager_onPrefetchResponseCompleted(
        env, prefetch_manager_java_object_, prefetch_java_callback_,
        prefetch_java_callback_executor_);
  }

  void OnPrefetchResponseError() override {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AwPrefetchManager_onPrefetchResponseError(
        env, prefetch_manager_java_object_, prefetch_java_callback_,
        prefetch_java_callback_executor_);
  }

  void OnPrefetchResponseServerError(int response_code) override {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_AwPrefetchManager_onPrefetchResponseServerError(
        env, prefetch_manager_java_object_, prefetch_java_callback_,
        prefetch_java_callback_executor_, response_code);
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> prefetch_manager_java_object_;
  base::android::ScopedJavaGlobalRef<jobject> prefetch_java_callback_;
  base::android::ScopedJavaGlobalRef<jobject> prefetch_java_callback_executor_;
};

AwPrefetchManager::AwPrefetchManager(content::BrowserContext* browser_context)
    : browser_context_(*browser_context) {
  TRACE_EVENT_INSTANT("android_webview",
                      "AwPrefetchManager::AwPrefetchManager");
}

AwPrefetchManager::~AwPrefetchManager() = default;

// static
bool AwPrefetchManager::IsPrefetchRequest(
    const network::ResourceRequest& resource_request) {
  return AwPrefetchManager::IsSecPurposeForPrefetch(
      resource_request.headers.GetHeader(blink::kSecPurposeHeaderName));
}

// static
bool AwPrefetchManager::IsPrerenderRequest(
    const network::ResourceRequest& resource_request) {
  return blink::IsSecPurposeForPrerender(
      resource_request.headers.GetHeader(blink::kSecPurposeHeaderName));
}

// static
bool AwPrefetchManager::IsSecPurposeForPrefetch(
    std::optional<std::string> sec_purpose_header_value) {
  return blink::IsSecPurposeForPrefetch(sec_purpose_header_value);
}

// static
void AwPrefetchManager::SetOrClearExternalPrefetchExperiment(
    std::optional<int> variations_id) {
  std::vector<int> experiment_ids;
  if (variations_id.has_value()) {
    experiment_ids.push_back(variations_id.value());
  }

  // Always invoke registration to ensure the metrics state is synchronized
  // with the current request. Providing an empty ID list is necessary to
  // clear state from previous requests if the current one lacks a
  // Variations ID.
  AwMetricsServiceAccessor::RegisterExternalExperiment(experiment_ids);
}

AwPrefetchKey AwPrefetchManager::StartPrefetchRequest(
    JNIEnv* env,
    const std::string& url,
    const base::android::JavaRef<jobject>& prefetch_params,
    const base::android::JavaRef<jobject>& callback,
    const base::android::JavaRef<jobject>& callback_executor) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT0("android_webview", "AwPrefetchManager::StartPrefetchRequest");

  GURL pf_url = GURL(url);
  net::HttpRequestHeaders additional_headers =
      GetAdditionalHeadersFromPrefetchParameters(env, prefetch_params);

  // TODO(crbug.com/455296998): Remove this code for M145.
  bool should_bypass_http_cache = false;
  if (base::FeatureList::IsEnabled(
          android_webview::features::
              kWebViewBypassHttpCacheForPrefetchFromHeader)) {
    should_bypass_http_cache = GetShouldBypassHttpCacheFromHeaders(
        additional_headers, /*remove_header=*/true);
  }
  std::optional<net::HttpNoVarySearchData> expected_no_vary_search =
      GetExpectedNoVarySearchFromPrefetchParameters(env, prefetch_params);

  std::optional<int> variations_id =
      GetVariationsIdFromPrefetchParameters(env, prefetch_params);
  AwPrefetchManager::SetOrClearExternalPrefetchExperiment(variations_id);

  std::unique_ptr<content::PrefetchRequestStatusListener>
      request_status_listener;
  if (java_obj_ && callback && callback_executor) {
    request_status_listener = std::make_unique<AwPrefetchRequestStatusListener>(
        java_obj_, callback, callback_executor);
  } else {
    CHECK_IS_TEST();
  }

  // For WebView we will check if there is already a duplicate
  // prefetch request based on the URL and the No-Vary-Search hint. This is for
  // the purpose of deduping prefetch requests on the application's behalf.
  // TODO(crbug.com/393344309): Apply deduping to all prefetch requests (not
  // just WebView).
  bool is_prefetch_duplicate = [&]() {
    if (!base::FeatureList::IsEnabled(
            features::kWebViewPrefetchOffTheMainThread)) {
      DCHECK_CURRENTLY_ON(BrowserThread::UI);
      return browser_context_->IsPrefetchDuplicate(pf_url,
                                                   expected_no_vary_search);
    } else {
      return aw_prefetch_manager_data_.IsPrefetchDuplicate(
          pf_url, expected_no_vary_search);
    }
  }();

  if (is_prefetch_duplicate) {
    if (request_status_listener) {
      request_status_listener->OnPrefetchStartFailedDuplicate();
    }
    return NO_PREFETCH_KEY;
  }

  // Make room for the new prefetch request by evicting the older ones to
  // respect the `max_prefetches_` limit.
  //
  // We intentionally do this **before** starting prefetch instead of after.
  // Due to current //content `PrefetchScheduler` restrictions of its
  // sequential async scheduling, if an evicted prefetch is still running,
  // canceling it before starting a next one reduces one PostTask, which is good
  // from performance perspective. Please see
  // https://docs.google.com/document/d/1OylSDdS_RTOkG_E_PXJ0aPI1QrygMjGkgSs5JcTrFlE/edit?tab=t.0#bookmark=id.rcr0rfweiz90
  // for more information.
  // TODO(crbug.com/426404355): After parallel prefetching being enabled for
  // WV.prefetch, perhaps we no longer need this. Revisit and verify.
  aw_prefetch_manager_data_.MayEvictOldestPrefetchHandleForANewRequest();

  std::unique_ptr<content::PrefetchHandle> prefetch_handle =
      browser_context_->StartBrowserPrefetchRequest(
          pf_url, AW_PREFETCH_METRICS_SUFFIX,
          GetIsJavaScriptEnabledFromPrefetchParameters(env, prefetch_params),
          expected_no_vary_search,
          base::FeatureList::IsEnabled(
              ::features::kWebViewPrefetchHighestPrefetchPriority)
              ? std::optional(content::PrefetchPriority::kHighest)
              : std::nullopt,
          additional_headers, std::move(request_status_listener),
          base::Seconds(aw_prefetch_manager_data_.GetTtlInSec()),
          /*should_append_variations_header=*/false,
          base::FeatureList::IsEnabled(
              kWebViewPrefetchDisableBlockUntilHeadTimeout),
          should_bypass_http_cache);

  if (prefetch_handle) {
    return aw_prefetch_manager_data_.AddPrefetchHandle(
        std::make_unique<AwPrefetchHandleWrapper>(
            pf_url, std::move(expected_no_vary_search),
            std::move(prefetch_handle)));
  } else {
    return NO_PREFETCH_KEY;
  }
}

void AwPrefetchManager::CancelPrefetch(JNIEnv* env,
                                       AwPrefetchKey prefetch_key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT0("android_webview", "AwPrefetchManager::CancelPrefetch");
  if (prefetch_key == NO_PREFETCH_KEY) {
    // no-op.
    return;
  }
  aw_prefetch_manager_data_.CancelPrefetch(prefetch_key);
}

void AwPrefetchManager::SetTtlInSec(JNIEnv* env,
                                    std::optional<int> ttl_in_sec) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  int sanitized_ttl_in_sec = ttl_in_sec.value_or(kDefaultTtlInSec);
  CHECK_GT(sanitized_ttl_in_sec, 0);

  aw_prefetch_manager_data_.SetTtlInSec(sanitized_ttl_in_sec);
}

void AwPrefetchManager::SetMaxPrefetches(JNIEnv* env,
                                         std::optional<int> max_prefetches) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  size_t sanitized_max_prefetches = kDefaultMaxPrefetches;
  if (max_prefetches) {
    CHECK_GT(max_prefetches.value(), 0);
    sanitized_max_prefetches = static_cast<size_t>(max_prefetches.value());
  }

  aw_prefetch_manager_data_.SetMaxPrefetches(
      std::min(sanitized_max_prefetches, kAbsoluteMaxPrefetches));
}

int AwPrefetchManager::GetTtlInSecForTesting(JNIEnv* env) const {
  return aw_prefetch_manager_data_.GetTtlInSec();
}

size_t AwPrefetchManager::GetMaxPrefetchesForTesting(JNIEnv* env) const {
  return aw_prefetch_manager_data_.GetMaxPrefetchesForTesting();  // IN-TEST
}

std::vector<AwPrefetchKey>
AwPrefetchManager::GetAllPrefetchKeysForTesting()  // IN-TEST
    const {
  return aw_prefetch_manager_data_.GetAllPrefetchKeysForTesting();  // IN-TEST
}

AwPrefetchKey AwPrefetchManager::GetLastPrefetchKeyForTesting() const {
  return aw_prefetch_manager_data_.GetLastPrefetchKeyForTesting();  // IN-TEST
}

bool AwPrefetchManager::GetIsPrefetchInCacheForTesting(  // IN-TEST
    JNIEnv* env,
    AwPrefetchKey prefetch_key) const {
  return aw_prefetch_manager_data_.GetIsPrefetchInCacheForTesting(  // IN-TEST
      prefetch_key);
}

static AwPrefetchKey JNI_AwPrefetchManager_GetNoPrefetchKey(JNIEnv* env) {
  return NO_PREFETCH_KEY;
}

static bool JNI_AwPrefetchManager_IsSecPurposeForPrefetch(
    JNIEnv* env,
    const std::string& sec_purpose_header_value) {
  return AwPrefetchManager::IsSecPurposeForPrefetch(sec_purpose_header_value);
}

base::android::ScopedJavaLocalRef<jobject>
AwPrefetchManager::GetJavaPrefetchManager() {
  if (!java_obj_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    java_obj_ =
        Java_AwPrefetchManager_create(env, reinterpret_cast<intptr_t>(this));
  }
  return base::android::ScopedJavaLocalRef<jobject>(java_obj_);
}

}  // namespace android_webview

DEFINE_JNI(AwPrefetchManager)
