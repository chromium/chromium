// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_QUOTA_MANAGER_BRIDGE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_QUOTA_MANAGER_BRIDGE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/android/jni_weak_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"

namespace content {
class StoragePartition;
}

namespace storage {
class QuotaManager;
}  // namespace storage

namespace android_webview {

class AwBrowserContext;

class AwQuotaManagerBridge
    : public base::RefCountedThreadSafe<AwQuotaManagerBridge> {
 public:
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
                  jint callback_id);
  void GetUsageAndQuotaForOrigin(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& object,
      const base::android::JavaParamRef<jstring>& origin,
      jint callback_id,
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

  void DeleteAllDataOnUiThread();
  void DeleteOriginOnUiThread(const base::string16& origin);
  void GetOriginsOnUiThread(jint callback_id);
  void GetUsageAndQuotaForOriginOnUiThread(const base::string16& origin,
                                           jint callback_id,
                                           bool is_quota);

  void GetOriginsCallbackImpl(int jcallback_id,
                              const std::vector<std::string>& origin,
                              const std::vector<int64_t>& usage,
                              const std::vector<int64_t>& quota);
  void QuotaUsageCallbackImpl(int jcallback_id,
                              bool is_quota,
                              int64_t usage,
                              int64_t quota);

  AwBrowserContext* browser_context_;
  JavaObjectWeakGlobalRef java_ref_;

  base::WeakPtrFactory<AwQuotaManagerBridge> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AwQuotaManagerBridge);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_QUOTA_MANAGER_BRIDGE_H_
