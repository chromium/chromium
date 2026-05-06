// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "android_webview/browser/prefetch/aw_prefetch_manager.h"

#include <jni.h>

#include <optional>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_origin_matched_header.h"
#include "android_webview/browser/metrics/aw_metrics_service_accessor.h"
#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "android_webview/browser/network_service/aw_proxying_url_loader_factory.h"
#include "android_webview/browser/prefetch/aw_prefetch_manager_data.h"
#include "android_webview/browser/prefetch/aw_prefetch_prefs.h"
#include "android_webview/browser/prefetch/aw_preloading_utils.h"
#include "android_webview/common/aw_features.h"
#include "base/android/apk_info.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check_is_test.h"
#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "base/trace_event/trace_event.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/prefetch_deduplication_utils.h"
#include "content/public/browser/preload_pipeline_info.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"

// Has to come after all the FromJniType() / ToJniType() headers.
#include "android_webview/browser_jni_headers/AwPrefetchManager_jni.h"
#include "third_party/blink/public/common/navigation/preloading_headers.h"

using content::BrowserThread;

namespace android_webview {

namespace {

void NotifyStartFailedDuplicate(
    std::unique_ptr<content::PrefetchRequestStatusListener>
        request_status_listener) {
  if (!request_status_listener) {
    return;
  }

  if (base::FeatureList::IsEnabled(
          ::features::kPrefetchRequestStatusListenerAsync)) {
    // We always post this task to the UI thread even if this is called from
    // non-UI thread (i.e. off-the-main-thread preprefetch), because:
    // - Chromium taskrunner might be unavailable on the non-UI Java thread.
    // - The caller `AwPrefetchRequestStatusListener` doesn't care on which
    //   thread it is called.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&content::PrefetchRequestStatusListener::
                                      OnPrefetchStartFailedDuplicate,
                                  std::move(request_status_listener)));
  } else {
    request_status_listener->OnPrefetchStartFailedDuplicate();
  }
}

}  // namespace

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

namespace {

static PrefService* g_pref_service_for_testing = nullptr;

// A part of header modification for PrePrefetch, which would be added via
// `AwProxyingURLLoaderFactory` (by
// `ContentBrowserClient::WillCreateURLLoaderFactory`) normally on non UI
// thread.
content::PrefetchUpdateHeadersParams GetAwPrefetchHeadersOnNonUIThread(
    const network::ResourceRequest& request) {
  content::PrefetchUpdateHeadersParams headers;
  // We can safely ignore any processing handled in
  // `shouldInterceptRequest`, because prefetch intentionally bypasses it.

  AwProxyingURLLoaderFactory::SetRequestedWithHeader(
      request, headers.modified_cors_exempt_headers);

  // TODO(crbug.com/452389538): Apply custom headers configured by the
  // application via the AndroidX Profile.setCustomHeaders() API
  // (AwOriginMatchedHeader).

  return headers;
}

std::unique_ptr<content::PrePrefetchService> CreatePrePrefetchService(
    content::BrowserContext* browser_context,
    std::optional<AwPrefetchLatestInfoPref> initial_prefetch_hints,
    AwPrefetchManagerData& prefetch_manager_data) {
  if (initial_prefetch_hints.has_value()) {
    prefetch_manager_data.UpdateLatestPrefetchInfo(
        initial_prefetch_hints.value());
    // Read the latest prefetch info from persisted prefs, and pass
    // these values to `PrePrefetchService::Create()` as a hint for
    // the likely initial PrePrefetch request for pre-calculating UI
    // thread dependent parts of the PrePrefetch `ResourceRequest`.
    return content::PrePrefetchService::Create(
        browser_context,
        {base::BindRepeating(&GetAwPrefetchHeadersOnNonUIThread)},
        initial_prefetch_hints.value().origin,
        initial_prefetch_hints.value().javascript_enabled,
        // All prefetches from `AwPrefetchManager` will have false
        // `initial_should_append_variations_header_hint`.
        /*initial_should_append_variations_header_hint=*/false);
  }
  return content::PrePrefetchService::Create(
      browser_context,
      {base::BindRepeating(&GetAwPrefetchHeadersOnNonUIThread)});
}

}  // namespace

AwPrefetchManager::AwPrefetchManager(content::BrowserContext* browser_context)
    : browser_context_(*browser_context),
      aw_pre_prefetch_service_(
          IsWebViewPrefetchOffTheMainThreadEnabled()
              ? CreatePrePrefetchService(browser_context,
                                         ReadLatestPrefetchInfoFromPref(),
                                         aw_prefetch_manager_data_)
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

// static
void AwPrefetchManager::SetPrefServiceForTesting(PrefService* pref_service) {
  g_pref_service_for_testing = pref_service;
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
  CHECK(IsWebViewPrefetchOffTheMainThreadEnabled());

  return StartRequest(env, url, /*is_pre_prefetch=*/true,
                      /*preload_pipeline_info=*/nullptr, prefetch_params,
                      callback, callback_executor);
}

int AwPrefetchManager::StartPrefetchFromPrePrefetch(JNIEnv* env,
                                                    int32_t prefetch_key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT0("android_webview",
               "AwPrefetchManager::StartPrefetchFromPrePrefetch");

  CHECK(IsWebViewPrefetchOffTheMainThreadEnabled());

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
  if (IsWebViewPrefetchOffTheMainThreadEnabled()) {
    key = aw_prefetch_manager_data_.ReservePrefetchHandleWrapper(
        pf_url, expected_no_vary_search);
    if (key == NO_PREFETCH_KEY) {
      NotifyStartFailedDuplicate(std::move(request_status_listener));
      return NO_PREFETCH_KEY;
    }
  } else {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (browser_context_->IsPrefetchDuplicate(pf_url,
                                              expected_no_vary_search)) {
      NotifyStartFailedDuplicate(std::move(request_status_listener));
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

  bool javascript_enabled =
      GetIsJavaScriptEnabledFromPrefetchParameters(env, prefetch_params);

  if (is_pre_prefetch) {
    DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
    CHECK(IsWebViewPrefetchOffTheMainThreadEnabled());
    CHECK(aw_pre_prefetch_service_);
    pre_prefetch_handle = aw_pre_prefetch_service_->StartPrePrefetchRequest(
        pf_url, AW_PREFETCH_METRICS_SUFFIX, javascript_enabled,
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
        pf_url, AW_PREFETCH_METRICS_SUFFIX, javascript_enabled,
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

  if (IsWebViewPrefetchOffTheMainThreadEnabled()) {
    url::Origin pf_origin = url::Origin::Create(pf_url);
    if (pf_origin.opaque()) {
      // This is theoretically possible where
      // `AwPrefetchManager#getStartPrefetchErrorOrNull()`'s
      // `Uri.parse(url).getScheme()` returns HTTPS but still `pf_url` has
      // an invalid GURL and thus `pf_origin` is opaque.
      // TODO(crbug.com/452406598): This should be prevented orthogonal to
      // pref's origin. We can parse GURL in Java and catch any errors there.
      NOTREACHED();
    }

    // Updates the latest prefetch info and persist it to prefs if the
    // settings are actually changed.
    AwPrefetchLatestInfoPref hints = {pf_origin, javascript_enabled};
    if (aw_prefetch_manager_data_.UpdateLatestPrefetchInfo(hints)) {
      if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
        // Unretained is safe here because currently `AwPrefetchManager` will
        // never be destructed. Please see the comment on
        // `aw_prefetch_manager.h`'s Thread model.
        content::GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE,
            base::BindOnce(&AwPrefetchManager::WriteLatestPrefetchInfoToPref,
                           base::Unretained(this), std::move(hints)));
      } else {
        WriteLatestPrefetchInfoToPref(std::move(hints));
      }
    }
  }

  if (IsWebViewPrefetchOffTheMainThreadEnabled()) {
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

PrefService* AwPrefetchManager::GetPrefService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(IsWebViewPrefetchOffTheMainThreadEnabled());

  if (g_pref_service_for_testing) {
    return g_pref_service_for_testing;
  }
  auto* aw_browser_context =
      static_cast<AwBrowserContext*>(&browser_context_.get());
  return aw_browser_context->GetPrefService();
}

std::optional<AwPrefetchLatestInfoPref>
AwPrefetchManager::ReadLatestPrefetchInfoFromPref() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("android_webview",
              "AwPrefetchManager::ReadLatestPrefetchInfoFromPref");

  CHECK(IsWebViewPrefetchOffTheMainThreadEnabled());

  PrefService* pref_service = GetPrefService();
  if (!pref_service) {
    return std::nullopt;
  }
  std::string origin_str =
      pref_service->GetString(prefs::kAwPrefetchLatestOrigin);
  if (origin_str.empty()) {
    return std::nullopt;
  }
  url::Origin origin = url::Origin::Create(GURL(origin_str));
  if (origin.opaque()) {
    return std::nullopt;
  }
  bool javascript_enabled =
      pref_service->GetBoolean(prefs::kAwPrefetchLatestJavascriptEnabled);
  return AwPrefetchLatestInfoPref{origin, javascript_enabled};
}

void AwPrefetchManager::WriteLatestPrefetchInfoToPref(
    AwPrefetchLatestInfoPref pref) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT("android_webview",
              "AwPrefetchManager::WriteLatestPrefetchInfoToPref");

  CHECK(IsWebViewPrefetchOffTheMainThreadEnabled());

  // This should always be true. the origin started for prefetch
  // is already checked to be non-opaque.
  CHECK(!pref.origin.opaque());
  if (PrefService* pref_service = GetPrefService()) {
    pref_service->SetString(prefs::kAwPrefetchLatestOrigin,
                            pref.origin.Serialize());
    pref_service->SetBoolean(prefs::kAwPrefetchLatestJavascriptEnabled,
                             pref.javascript_enabled);
  }
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
