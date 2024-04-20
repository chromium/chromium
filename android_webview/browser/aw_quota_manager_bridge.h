// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_QUOTA_MANAGER_BRIDGE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_QUOTA_MANAGER_BRIDGE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/android/jni_weak_ref.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"

namespace content {
class StoragePartition;
}

namespace storage {
class QuotaManager;
}  // namespace storage

namespace android_webview {

class AwBrowserContext;

// TODO(crbug.com/40184305): Change the functions in this class to reference
// StorageKey instead of Origin.
//
// This object is owned by the native AwBrowserContext, and the Java peer is
// owned by the Java AwBrowserContext.
//
// Lifetime: Profile
class AwQuotaManagerBridge
    : public base::RefCountedThreadSafe<AwQuotaManagerBridge> {
 public:
  AwQuotaManagerBridge(const AwQuotaManagerBridge&) = delete;
  AwQuotaManagerBridge& operator=(const AwQuotaManagerBridge&) = delete;

  static scoped_refptr<AwQuotaManagerBridge> Create(
      AwBrowserContext* browser_context);

  // Called by Java.
  void Init(JNIEnv* env, const base::android::JavaParamRef<jobject>& object);
  void DeleteAllData(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& object);
  void DeleteOrigin(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& object,
                    const base::android::JavaParamRef<jstring>& origin);
  void GetOrigins(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& object,
                  const base::android::JavaParamRef<jobject>& callback);
  void GetUsageAndQuotaForOrigin(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object,
      const base::android::JavaParamRef<jstring>& origin,
      const base::android::JavaParamRef<jobject>& callback,
      bool is_quota);

  using GetOriginsCallback =
      base::OnceCallback<void(const std::vector<std::string>& /* origin */,
                              const std::vector<int64_t>& /* usaoge */,
                              const std::vector<int64_t>& /* quota */)>;
  using QuotaUsageCallback =
      base::OnceCallback<void(int64_t /* usage */, int64_t /* quota */)>;

 private:
  friend class base::RefCountedThreadSafe<AwQuotaManagerBridge>;
  explicit AwQuotaManagerBridge(AwBrowserContext* browser_context);
  ~AwQuotaManagerBridge();

  content::StoragePartition* GetStoragePartition() const;

  storage::QuotaManager* GetQuotaManager() const;

  raw_ptr<AwBrowserContext> browser_context_;
  JavaObjectWeakGlobalRef java_ref_;

  base::WeakPtrFactory<AwQuotaManagerBridge> weak_factory_{this};
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_QUOTA_MANAGER_BRIDGE_H_
