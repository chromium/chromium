// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_quota_manager_bridge.h"

#include <set>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_content_browser_client.h"
#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwQuotaManagerBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using content::BrowserThread;
using content::StoragePartition;
using storage::QuotaManager;

namespace android_webview {

namespace {

// This object lives on UI and IO threads. Care need to be taken to make sure
// there are no concurrent accesses to instance variables. Also this object
// is refcounted in the various callbacks, and is destroyed when all callbacks
// are destroyed at the end of DoneOnUIThread.
class GetStorageKeysTask
    : public base::RefCountedThreadSafe<GetStorageKeysTask> {
 public:
  GetStorageKeysTask(AwQuotaManagerBridge::GetOriginsCallback callback,
                     QuotaManager* quota_manager);

  GetStorageKeysTask(const GetStorageKeysTask&) = delete;
  GetStorageKeysTask& operator=(const GetStorageKeysTask&) = delete;

  void Run();

 private:
  friend class base::RefCountedThreadSafe<GetStorageKeysTask>;
  ~GetStorageKeysTask();

  void OnStorageKeysObtained(blink::mojom::StorageType type,
                             const std::set<blink::StorageKey>& storage_keys);

  void OnUsageAndQuotaObtained(const blink::StorageKey& storage_key,
                               blink::mojom::QuotaStatusCode status_code,
                               int64_t usage,
                               int64_t quota);

  void CheckDone();
  void DoneOnUIThread();

  AwQuotaManagerBridge::GetOriginsCallback ui_callback_;
  scoped_refptr<QuotaManager> quota_manager_;

  std::vector<std::string> origin_;
  std::vector<int64_t> usage_;
  std::vector<int64_t> quota_;

  size_t num_callbacks_to_wait_;
  size_t num_callbacks_received_;
};

GetStorageKeysTask::GetStorageKeysTask(
    AwQuotaManagerBridge::GetOriginsCallback callback,
    QuotaManager* quota_manager)
    : ui_callback_(std::move(callback)), quota_manager_(quota_manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

GetStorageKeysTask::~GetStorageKeysTask() {}

void GetStorageKeysTask::Run() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &QuotaManager::GetStorageKeysForType, quota_manager_,
          blink::mojom::StorageType::kTemporary,
          base::BindOnce(&GetStorageKeysTask::OnStorageKeysObtained, this,
                         blink::mojom::StorageType::kTemporary)));
}

void GetStorageKeysTask::OnStorageKeysObtained(
    blink::mojom::StorageType type,
    const std::set<blink::StorageKey>& storage_keys) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  num_callbacks_to_wait_ = storage_keys.size();
  num_callbacks_received_ = 0u;

  for (const blink::StorageKey& storage_key : storage_keys) {
    quota_manager_->GetUsageAndQuota(
        storage_key, type,
        base::BindOnce(&GetStorageKeysTask::OnUsageAndQuotaObtained, this,
                       storage_key));
  }

  CheckDone();
}

void GetStorageKeysTask::OnUsageAndQuotaObtained(
    const blink::StorageKey& storage_key,
    blink::mojom::QuotaStatusCode status_code,
    int64_t usage,
    int64_t quota) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status_code == blink::mojom::QuotaStatusCode::kOk) {
    origin_.push_back(storage_key.origin().GetURL().spec());
    usage_.push_back(usage);
    quota_.push_back(quota);
  }

  ++num_callbacks_received_;
  CheckDone();
}

void GetStorageKeysTask::CheckDone() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (num_callbacks_received_ == num_callbacks_to_wait_) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&GetStorageKeysTask::DoneOnUIThread, this));
  } else if (num_callbacks_received_ > num_callbacks_to_wait_) {
    NOTREACHED();
  }
}

// This method is to avoid copying the 3 vector arguments into a bound callback.
void GetStorageKeysTask::DoneOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(ui_callback_).Run(origin_, usage_, quota_);
}

}  // namespace

// static
scoped_refptr<AwQuotaManagerBridge> AwQuotaManagerBridge::Create(
    AwBrowserContext* browser_context) {
  return new AwQuotaManagerBridge(browser_context);
}

AwQuotaManagerBridge::AwQuotaManagerBridge(AwBrowserContext* browser_context)
    : browser_context_(browser_context) {}

AwQuotaManagerBridge::~AwQuotaManagerBridge() {}

void AwQuotaManagerBridge::Init(JNIEnv* env,
                                const JavaParamRef<jobject>& object) {
  java_ref_ = JavaObjectWeakGlobalRef(env, object);
}

StoragePartition* AwQuotaManagerBridge::GetStoragePartition() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // AndroidWebview does not use per-site storage partitions.
  StoragePartition* storage_partition =
      browser_context_->GetDefaultStoragePartition();
  DCHECK(storage_partition);
  return storage_partition;
}

QuotaManager* AwQuotaManagerBridge::GetQuotaManager() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  QuotaManager* quota_manager = GetStoragePartition()->GetQuotaManager();
  DCHECK(quota_manager);
  return quota_manager;
}

void AwQuotaManagerBridge::DeleteAllData(JNIEnv* env,
                                         const JavaParamRef<jobject>& object) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetStoragePartition()->ClearData(
      // Clear all web storage data except cookies.
      StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
          StoragePartition::REMOVE_DATA_MASK_INDEXEDDB |
          StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE |
          StoragePartition::REMOVE_DATA_MASK_WEBSQL,
      StoragePartition::QUOTA_MANAGED_STORAGE_MASK_TEMPORARY,
      blink::StorageKey(), base::Time(), base::Time::Max(), base::DoNothing());
}

void AwQuotaManagerBridge::DeleteOrigin(JNIEnv* env,
                                        const JavaParamRef<jobject>& object,
                                        const JavaParamRef<jstring>& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::u16string origin_string(
      base::android::ConvertJavaStringToUTF16(env, origin));
  StoragePartition* storage_partition = GetStoragePartition();
  storage_partition->ClearDataForOrigin(
      // All (temporary) QuotaClient types.
      StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
          StoragePartition::REMOVE_DATA_MASK_INDEXEDDB |
          StoragePartition::REMOVE_DATA_MASK_WEBSQL,
      StoragePartition::QUOTA_MANAGED_STORAGE_MASK_TEMPORARY,
      GURL(origin_string), base::DoNothing());
}

void AwQuotaManagerBridge::GetOrigins(JNIEnv* env,
                                      const JavaParamRef<jobject>& object,
                                      const JavaParamRef<jobject>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetOriginsCallback ui_callback = base::BindOnce(
      [](const JavaRef<jobject>& obj, const JavaRef<jobject>& callback,
         const std::vector<std::string>& origin,
         const std::vector<int64_t>& usage, const std::vector<int64_t>& quota) {
        JNIEnv* env = AttachCurrentThread();
        Java_AwQuotaManagerBridge_onGetOriginsCallback(
            env, obj, callback,
            base::android::ToJavaArrayOfStrings(env, origin),
            base::android::ToJavaLongArray(env, usage),
            base::android::ToJavaLongArray(env, quota));
      },
      ScopedJavaGlobalRef<jobject>(env, object),
      ScopedJavaGlobalRef<jobject>(env, callback));
  base::MakeRefCounted<GetStorageKeysTask>(std::move(ui_callback),
                                           GetQuotaManager())
      ->Run();
}

namespace {

void OnUsageAndQuotaObtained(
    AwQuotaManagerBridge::QuotaUsageCallback ui_callback,
    blink::mojom::QuotaStatusCode status_code,
    int64_t usage,
    int64_t quota) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status_code != blink::mojom::QuotaStatusCode::kOk) {
    usage = 0;
    quota = 0;
  }
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(ui_callback), usage, quota));
}

}  // namespace

void AwQuotaManagerBridge::GetUsageAndQuotaForOrigin(
    JNIEnv* env,
    const JavaParamRef<jobject>& object,
    const JavaParamRef<jstring>& origin,
    const JavaParamRef<jobject>& callback,
    bool is_quota) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::u16string origin_string(
      base::android::ConvertJavaStringToUTF16(env, origin));

  QuotaUsageCallback ui_callback = base::BindOnce(
      [](const JavaRef<jobject>& callback, bool is_quota, int64_t usage,
         int64_t quota) {
        base::android::RunLongCallbackAndroid(callback,
                                              (is_quota ? quota : usage));
      },
      ScopedJavaGlobalRef<jobject>(env, callback), is_quota);

  // TODO(crbug.com/41417435): Use helper for url::Origin creation from string.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &QuotaManager::GetUsageAndQuota, GetQuotaManager(),
          blink::StorageKey::CreateFirstParty(
              url::Origin::Create(GURL(origin_string))),
          blink::mojom::StorageType::kTemporary,
          base::BindOnce(&OnUsageAndQuotaObtained, std::move(ui_callback))));
}

}  // namespace android_webview
