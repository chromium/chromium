// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_HTTP_CACHE_MANAGER_H_
#define ANDROID_WEBVIEW_BROWSER_AW_HTTP_CACHE_MANAGER_H_

#include <jni.h>

#include <optional>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ref.h"
#include "components/prefs/pref_name_set.h"

class PrefRegistrySimple;

namespace android_webview {

namespace prefs {

extern const char kHttpCacheQuota[];

}  // namespace prefs

class AwBrowserContext;

// Manages the HTTP cache for Android WebView.
//
// Lifetime: Profile
class AwHttpCacheManager {
 public:
  // Create a new instance owned by (non-null) |browser_context|.
  explicit AwHttpCacheManager(AwBrowserContext* browser_context);
  ~AwHttpCacheManager();

  AwHttpCacheManager(const AwHttpCacheManager&) = delete;
  AwHttpCacheManager& operator=(const AwHttpCacheManager&) = delete;

  // Get the corresponding AwHttpCacheManager Java object.
  base::android::ScopedJavaLocalRef<jobject> GetJavaHttpCacheManager();

  // |PrefRegistrySimple| must be non-null.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  // |PrefNameSet| must be non-null.
  void InsertPersistentPrefs(PrefNameSet* persistent_prefs);
  void RecordInitialQuotaHistogram();

  // Get the HTTP cache quota that would be allocated in the absence of a
  // customized cache quota.
  // |env| is ignored and may be nullptr.
  int64_t GetDefaultQuotaBytes(JNIEnv* env) const;
  // Reset the HTTP cache quota to an automatic default value.
  // |env| is ignored and may be nullptr.
  void UseDefaultQuota(JNIEnv* env);
  // Get whether the HTTP cache quota is using an automatic default value.
  // |env| is ignored and may be nullptr.
  bool IsUsingDefaultQuota(JNIEnv* env) const;
  // Get the concrete HTTP cache quota that is currently in effect.
  // |env| is ignored and may be nullptr.
  int64_t GetQuotaBytes(JNIEnv* env) const;
  // Set the HTTP cache quota to a specific (non-default) value. This may
  // trigger expensive disk operations, including cache initialization and
  // evictions. |env| is ignored and may be nullptr. |quota_in_bytes| must be >=
  // 0.
  void SetQuotaBytes(JNIEnv* env, int64_t quota_in_bytes);

 private:
  // Updates a quota to a specific or default value.
  void SetQuotaBytesInternal(int64_t quota_in_bytes);

  // Records usage metrics for the HTTP cache quota API.
  void RecordQuotaSetterHistograms(int64_t quota_in_bytes);

  // Check if the HTTP cache quota API is enabled for this browser context.
  bool IsQuotaApiEnabled() const;

  // The owning browser context.
  const raw_ref<AwBrowserContext> browser_context_;

  // Corresponding AwHttpCacheManager Java object.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  // Stores the last value passed to SetQuotaBytes (or the default if
  // UseDefaultQuota was called). Used only for histograms.
  std::optional<int64_t> last_quota_set_for_metrics_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_HTTP_CACHE_MANAGER_H_
