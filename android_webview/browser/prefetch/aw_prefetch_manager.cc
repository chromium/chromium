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
#include "base/notimplemented.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/prefetch_deduplication_utils.h"
#include "content/public/browser/preload_pipeline_info.h"
#include "content/public/common/content_features.h"

// Has to come after all the FromJniType() / ToJniType() headers.
#include "android_webview/browser_jni_headers/AwPrefetchManager_jni.h"
#include "third_party/blink/public/common/navigation/preloading_headers.h"

using content::BrowserThread;

namespace android_webview {

BASE_FEATURE(kWebViewPrefetchDisableBlockUntilHeadTimeout,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Listens to the status of a prefetch request and propagates it to Java
// callbacks.
//
// Thread model:
//
// Instances of this class can be invoked on any thread. Since
// `JNIEnv` is thread-local, we use `AttachCurrentThread()` to safely acquire a
// valid `JNIEnv` and `ScopedJavaGlobalRef` to safely hold Java objects across
// different threads.
//
// TODO(crbug.com/496807663): Ideally the callback should be invoked as async
// task on a created thread environment as a guardrail not to invoke any
// navigation/prefetch reentrancy.
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
    : browser_context_(*browser_context),
      aw_pre_prefetch_service_(
          base::FeatureList::IsEnabled(
              features::kWebViewPrefetchOffTheMainThread)
              ? content::PrePrefetchService::Create(&browser_context_.get())
              : nullptr) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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
  return StartRequest(
      env, url, /*is_pre_prefetch=*/false,
      content::PreloadPipelineInfo::Create(
          /*planned_max_preloading_type=*/content::PreloadingType::kPrefetch),
      prefetch_params, callback, callback_executor);
}

AwPrefetchKey AwPrefetchManager::StartPrefetchRequestAheadOfPrerender(
    base::PassKey<AwContents>,
    JNIEnv* env,
    const std::string& url,
    const base::android::JavaRef<jobject>& prefetch_params,
    scoped_refptr<content::PreloadPipelineInfo> preload_pipeline_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT0("android_webview",
               "AwPrefetchManager::StartPrefetchRequestAheadOfPrerender");
  return StartRequest(
      env, url, /*is_pre_prefetch=*/false, std::move(preload_pipeline_info),
      prefetch_params,
      // Callbacks are not set for prefetch ahead of prerender. Callers are
      // expected to observe failures etc. via prerender.
      /*callback=*/base::android::JavaRef(),
      /*callback_executor=*/base::android::JavaRef());
}

int AwPrefetchManager::StartPrePrefetchRequest(
    JNIEnv* env,
    const std::string& url,
    const base::android::JavaRef<jobject>& prefetch_params,
    const base::android::JavaRef<jobject>& callback,
    const base::android::JavaRef<jobject>& callback_executor) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  TRACE_EVENT0("android_webview", "AwPrefetchManager::StartPrePrefetchRequest");
  CHECK(
      base::FeatureList::IsEnabled(features::kWebViewPrefetchOffTheMainThread));

  return StartRequest(env, url, /*is_pre_prefetch=*/true,
                      /*preload_pipeline_info=*/nullptr, prefetch_params,
                      callback, callback_executor);
}

int AwPrefetchManager::StartPrefetchFromPrePrefetch(JNIEnv* env,
                                                    int32_t prefetch_key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT0("android_webview",
               "AwPrefetchManager::StartPrefetchFromPrePrefetch");

  CHECK(
      base::FeatureList::IsEnabled(features::kWebViewPrefetchOffTheMainThread));

  std::unique_ptr<content::PrePrefetchHandle> pre_prefetch_handle =
      aw_prefetch_manager_data_.TakePrePrefetchHandleForConsume(prefetch_key);

  if (!pre_prefetch_handle) {
    return NO_PREFETCH_KEY;
  }

  std::unique_ptr<content::PrefetchHandle> prefetch_handle =
      browser_context_->StartPrefetchFromPrePrefetch(
          std::move(pre_prefetch_handle));

  if (prefetch_handle) {
    aw_prefetch_manager_data_.CommitPrefetchHandleAfterConsume(
        prefetch_key, std::move(prefetch_handle));
  } else {
    // If starting the prefetch fails, the wrapper is now left without a handle,
    // which should be cleaned up.
    // TODO(crbug.com/452406598): This manual cancellation should ideally be
    // removed by introducing a writer interface that grants write permission
    // to the wrapper and automatically handles rollback on failure.
    aw_prefetch_manager_data_.CancelPrefetch(prefetch_key);
    return NO_PREFETCH_KEY;
  }

  return prefetch_key;
}

int AwPrefetchManager::StartRequest(
    JNIEnv* env,
    const std::string& url,
    bool is_pre_prefetch,
    scoped_refptr<content::PreloadPipelineInfo> preload_pipeline_info,
    const base::android::JavaRef<jobject>& prefetch_params,
    const base::android::JavaRef<jobject>& callback,
    const base::android::JavaRef<jobject>& callback_executor) {
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

  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&AwPrefetchManager::SetOrClearExternalPrefetchExperiment,
                       variations_id));
  } else {
    AwPrefetchManager::SetOrClearExternalPrefetchExperiment(variations_id);
  }

  std::unique_ptr<content::PrefetchRequestStatusListener>
      request_status_listener;
  if (java_obj_ && callback && callback_executor) {
    request_status_listener = std::make_unique<AwPrefetchRequestStatusListener>(
        java_obj_, callback, callback_executor);
  }

  AwPrefetchKey key = NO_PREFETCH_KEY;

  // For WebView we will check if there is already a duplicate prefetch
  // request based on the URL and the No-Vary-Search hint. This is for the
  // purpose of deduping prefetch requests on the application's behalf.
  // TODO(crbug.com/393344309): Apply deduping to all prefetch requests (not
  // just WebView).
  if (base::FeatureList::IsEnabled(
          features::kWebViewPrefetchOffTheMainThread)) {
    key = aw_prefetch_manager_data_.ReservePrefetchHandleWrapper(
        pf_url, expected_no_vary_search);
    if (key == NO_PREFETCH_KEY) {
      if (request_status_listener) {
        request_status_listener->OnPrefetchStartFailedDuplicate();
      }
      return NO_PREFETCH_KEY;
    }
  } else {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (browser_context_->IsPrefetchDuplicate(pf_url,
                                              expected_no_vary_search)) {
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
    // canceling it before starting a next one reduces one PostTask, which is
    // good from a performance perspective. Please see
    // https://docs.google.com/document/d/1OylSDdS_RTOkG_E_PXJ0aPI1QrygMjGkgSs5JcTrFlE/edit?tab=t.0#bookmark=id.rcr0rfweiz90
    // for more information.
    // TODO(crbug.com/426404355): After parallel prefetching being enabled for
    // WV.prefetch, perhaps we no longer need this. Revisit and verify.
    aw_prefetch_manager_data_.MayEvictOldestPrefetchHandleForANewRequest();
  }

  std::unique_ptr<content::PrefetchHandle> prefetch_handle;
  std::unique_ptr<content::PrePrefetchHandle> pre_prefetch_handle;

  if (is_pre_prefetch) {
    DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
    CHECK(base::FeatureList::IsEnabled(
        features::kWebViewPrefetchOffTheMainThread));
    CHECK(aw_pre_prefetch_service_);
    pre_prefetch_handle = aw_pre_prefetch_service_->StartPrePrefetchRequest(
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
  } else {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    prefetch_handle = browser_context_->StartBrowserPrefetchRequest(
        pf_url, AW_PREFETCH_METRICS_SUFFIX,
        GetIsJavaScriptEnabledFromPrefetchParameters(env, prefetch_params),
        expected_no_vary_search,
        base::FeatureList::IsEnabled(
            ::features::kWebViewPrefetchHighestPrefetchPriority)
            ? std::optional(content::PrefetchPriority::kHighest)
            : std::nullopt,
        std::move(preload_pipeline_info), additional_headers,
        std::move(request_status_listener),
        base::Seconds(aw_prefetch_manager_data_.GetTtlInSec()),
        /*should_append_variations_header=*/false,
        base::FeatureList::IsEnabled(
            kWebViewPrefetchDisableBlockUntilHeadTimeout),
        should_bypass_http_cache);
  }

  if (base::FeatureList::IsEnabled(
          features::kWebViewPrefetchOffTheMainThread)) {
    if (pre_prefetch_handle) {
      aw_prefetch_manager_data_.CommitInitialPrePrefetchHandle(
          key, std::move(pre_prefetch_handle));
      return key;
    } else if (prefetch_handle) {
      aw_prefetch_manager_data_.CommitInitialPrefetchHandle(
          key, std::move(prefetch_handle));
      return key;
    } else {
      // TODO(crbug.com/452406598): This manual cancellation should ideally be
      // removed by introducing a writer interface that grants write
      // permission and automatically handles rollback on failure.
      aw_prefetch_manager_data_.CancelPrefetch(key);
      return NO_PREFETCH_KEY;
    }
  } else {
    CHECK(!pre_prefetch_handle);
    if (prefetch_handle) {
      return aw_prefetch_manager_data_.AddNewPrefetchHandleWrapper(
          std::make_unique<AwPrefetchHandleWrapper>(
              pf_url, expected_no_vary_search, std::move(prefetch_handle)));
    }
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

void AwPrefetchManager::SetTtlInSec(JNIEnv* env, int ttl_in_sec) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  CHECK_GT(ttl_in_sec, 0);

  aw_prefetch_manager_data_.SetTtlInSec(ttl_in_sec);
}

void AwPrefetchManager::SetMaxPrefetches(JNIEnv* env, int max_prefetches) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  CHECK_GT(max_prefetches, 0);
  size_t sanitized_max_prefetches = static_cast<size_t>(max_prefetches);

  aw_prefetch_manager_data_.SetMaxPrefetches(
      std::min(sanitized_max_prefetches, kAbsoluteMaxPrefetches));
}

int AwPrefetchManager::GetTtlInSec(JNIEnv* env) const {
  return aw_prefetch_manager_data_.GetTtlInSec();
}

size_t AwPrefetchManager::GetMaxPrefetches(JNIEnv* env) const {
  return aw_prefetch_manager_data_.GetMaxPrefetches();
}

void AwPrefetchManager::ClearTtl(JNIEnv* env) {
  aw_prefetch_manager_data_.SetTtlInSec(kDefaultTtlInSec);
}

void AwPrefetchManager::ClearMaxPrefetches(JNIEnv* env) {
  aw_prefetch_manager_data_.SetMaxPrefetches(kDefaultMaxPrefetches);
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
