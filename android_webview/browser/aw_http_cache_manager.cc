// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_http_cache_manager.h"

#include <algorithm>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/network_service/net_helpers.h"
#include "android_webview/common/aw_features.h"
#include "base/android/scoped_java_ref.h"
#include "base/byte_size.h"
#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/checked_math.h"
#include "components/prefs/pref_name_set.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/network_context.mojom.h"

// Has to come after all the FromJniType() / ToJniType() headers.
#include "android_webview/browser_jni_headers/AwHttpCacheManager_jni.h"

namespace android_webview {

namespace {

// Value used in per-profile prefs to indicate that the default HTTP Cache
// quota should be used.
const int64_t kUseDefaultHttpCacheQuota = -1;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(HttpCacheQuotaChangeHistory)
enum class HttpCacheQuotaChangeHistory {
  kFirstTime = 0,
  kRepeatedCallWithoutChange = 1,
  kRepeatedCallWithChange = 2,
  kMaxValue = kRepeatedCallWithChange,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/android/enums.xml:HttpCacheQuotaChangeHistory)

}  // namespace

namespace prefs {

const char kHttpCacheQuota[] = "aw_http_cache_quota";

}  // namespace prefs

AwHttpCacheManager::AwHttpCacheManager(AwBrowserContext* browser_context)
    : browser_context_(raw_ref<AwBrowserContext>::from_ptr(browser_context)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (base::FeatureList::IsEnabled(features::kWebViewHttpCacheQuotaApi)) {
    // 1 is a hard minimum because the backends use 0 to mean a default (that
    // WebView doesn't actually ever use).
    CHECK_GE(features::kWebViewHttpCacheQuotaApiMinimum.Get(), 1);
    CHECK_LE(features::kWebViewHttpCacheQuotaApiMinimum.Get(),
             features::kWebViewHttpCacheQuotaApiMaximum.Get());
  }
}

AwHttpCacheManager::~AwHttpCacheManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

base::android::ScopedJavaLocalRef<jobject>
AwHttpCacheManager::GetJavaHttpCacheManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!java_obj_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    java_obj_ =
        Java_AwHttpCacheManager_create(env, reinterpret_cast<intptr_t>(this));
  }
  return base::android::ScopedJavaLocalRef<jobject>(java_obj_);
}

// static
void AwHttpCacheManager::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // We don't know if we're dealing with a default or non-default profile here,
  // so we may register the pref even if the API is disabled for Default.
  if (!base::FeatureList::IsEnabled(features::kWebViewHttpCacheQuotaApi)) {
    return;
  }
  registry->RegisterInt64Pref(prefs::kHttpCacheQuota,
                              kUseDefaultHttpCacheQuota);
}

void AwHttpCacheManager::InsertPersistentPrefs(PrefNameSet* persistent_prefs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!IsQuotaApiEnabled()) {
    return;
  }
  persistent_prefs->insert(prefs::kHttpCacheQuota);
}

void AwHttpCacheManager::RecordInitialQuotaHistogram() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!IsQuotaApiEnabled()) {
    return;
  }
  PrefService* user_pref_service = browser_context_->GetPrefService();
  base::UmaHistogramBoolean(
      browser_context_->IsDefaultBrowserContext()
          ? "Android.WebView.HttpCacheQuotaApi.RestoredFromPrefs.Default"
          : "Android.WebView.HttpCacheQuotaApi.RestoredFromPrefs.NonDefault",
      user_pref_service->HasPrefPath(prefs::kHttpCacheQuota) &&
          user_pref_service->GetInt64(prefs::kHttpCacheQuota) !=
              kUseDefaultHttpCacheQuota);
}

int64_t AwHttpCacheManager::GetDefaultQuotaBytes(JNIEnv* env) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!IsQuotaApiEnabled()) {
    return GetDefaultHttpCacheSize();
  }
  return std::clamp(GetDefaultHttpCacheSize(),
                    features::kWebViewHttpCacheQuotaApiMinimum.Get(),
                    features::kWebViewHttpCacheQuotaApiMaximum.Get());
}

void AwHttpCacheManager::UseDefaultQuota(JNIEnv* env) {
  SetQuotaBytesInternal(kUseDefaultHttpCacheQuota);
}

bool AwHttpCacheManager::IsUsingDefaultQuota(JNIEnv* env) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!IsQuotaApiEnabled()) {
    return true;
  }
  return browser_context_->GetPrefService()->GetInt64(prefs::kHttpCacheQuota) ==
         kUseDefaultHttpCacheQuota;
}

int64_t AwHttpCacheManager::GetQuotaBytes(JNIEnv* env) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!IsQuotaApiEnabled()) {
    return GetDefaultHttpCacheSize();
  }
  int64_t quota_in_bytes =
      browser_context_->GetPrefService()->GetInt64(prefs::kHttpCacheQuota);
  if (quota_in_bytes == kUseDefaultHttpCacheQuota) {
    quota_in_bytes = GetDefaultHttpCacheSize();
  } else {
    if (!features::kWebViewHttpCacheQuotaApiAllowShrinking.Get()) {
      quota_in_bytes =
          std::max(quota_in_bytes, int64_t{GetDefaultHttpCacheSize()});
    }
  }
  return std::clamp(quota_in_bytes,
                    int64_t{features::kWebViewHttpCacheQuotaApiMinimum.Get()},
                    int64_t{features::kWebViewHttpCacheQuotaApiMaximum.Get()});
}

void AwHttpCacheManager::SetQuotaBytes(JNIEnv* env, int64_t quota_in_bytes) {
  CHECK_NE(quota_in_bytes, kUseDefaultHttpCacheQuota);
  CHECK_GE(quota_in_bytes, 0);
  SetQuotaBytesInternal(quota_in_bytes);
}

void AwHttpCacheManager::SetQuotaBytesInternal(int64_t quota_in_bytes) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!IsQuotaApiEnabled()) {
    return;
  }
  RecordQuotaSetterHistograms(quota_in_bytes);
  // Note that we store the exact value supplied to the API without adjustment.
  // Adjustments are only performed when reading the value back out, allowing
  // adjustment-related feature parameters to always take effect.
  browser_context_->GetPrefService()->SetInt64(prefs::kHttpCacheQuota,
                                               quota_in_bytes);
  if (features::kWebViewHttpCacheQuotaApiRuntimeUpdate.Get()) {
    content::StoragePartition* partition =
        browser_context_->GetDefaultStoragePartition();
    if (partition && partition->GetNetworkContext()) {
      // Use GetQuotaBytes instead of quota_in_bytes, because it applies checks
      // and adjustments.
      partition->GetNetworkContext()->SetHttpCacheMaxSize(
          base::ByteSize(
              base::checked_cast<uint64_t>(GetQuotaBytes(/*env=*/nullptr))),
          features::kWebViewHttpCacheQuotaApiForceBackendInit.Get());
    }
  }
}

void AwHttpCacheManager::RecordQuotaSetterHistograms(int64_t quota_in_bytes) {
  const bool is_default_profile = browser_context_->IsDefaultBrowserContext();

  // Change history.
  HttpCacheQuotaChangeHistory change_history;
  if (!last_quota_set_for_metrics_.has_value()) {
    change_history = HttpCacheQuotaChangeHistory::kFirstTime;
  } else if (*last_quota_set_for_metrics_ == quota_in_bytes) {
    change_history = HttpCacheQuotaChangeHistory::kRepeatedCallWithoutChange;
  } else {
    change_history = HttpCacheQuotaChangeHistory::kRepeatedCallWithChange;
  }
  base::UmaHistogramEnumeration(
      is_default_profile
          ? "Android.WebView.HttpCacheQuotaApi.ChangeHistory.Default"
          : "Android.WebView.HttpCacheQuotaApi.ChangeHistory.NonDefault",
      change_history);
  last_quota_set_for_metrics_ = quota_in_bytes;

  if (quota_in_bytes == kUseDefaultHttpCacheQuota) {
    base::UmaHistogramBoolean(
        is_default_profile
            ? "Android.WebView.HttpCacheQuotaApi.UseDefaultQuota.Default"
            : "Android.WebView.HttpCacheQuotaApi.UseDefaultQuota.NonDefault",
        true);
    return;
  }

  // Absolute value in KiB.
  base::UmaHistogramCounts1M(
      is_default_profile
          ? "Android.WebView.HttpCacheQuotaApi.AbsoluteValue.Default"
          : "Android.WebView.HttpCacheQuotaApi.AbsoluteValue.NonDefault",
      base::saturated_cast<int>(quota_in_bytes / 1024));

  // Relative value in %.
  int64_t default_quota_in_bytes = GetDefaultQuotaBytes(/*env=*/nullptr);
  CHECK_GT(default_quota_in_bytes, 0);
  base::UmaHistogramCounts10000(
      is_default_profile
          ? "Android.WebView.HttpCacheQuotaApi.RelativeValue.Default"
          : "Android.WebView.HttpCacheQuotaApi.RelativeValue.NonDefault",
      base::saturated_cast<int>(quota_in_bytes * 100.0f /
                                default_quota_in_bytes));
}

bool AwHttpCacheManager::IsQuotaApiEnabled() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!base::FeatureList::IsEnabled(features::kWebViewHttpCacheQuotaApi)) {
    return false;
  }
  if (browser_context_->IsDefaultBrowserContext() &&
      !features::kWebViewHttpCacheQuotaApiAllowForDefaultProfile.Get()) {
    return false;
  }
  return true;
}

}  // namespace android_webview

DEFINE_JNI(AwHttpCacheManager)
