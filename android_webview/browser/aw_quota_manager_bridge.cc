// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_quota_manager_bridge.h"

#include <memory>
#include <optional>
#include <set>
#include <utility>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/common/aw_features.h"
#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/concurrent_callbacks.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/schemeful_site.h"
#include "storage/browser/quota/quota_manager_impl.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/jni_zero/default_conversions.h"
#include "url/gurl.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwQuotaManagerBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using content::BrowserThread;
using content::StoragePartition;

namespace android_webview {

namespace {

// Data removal mask for complete data removal.
constexpr content::BrowsingDataRemover::DataType kDataRemovalMask =
    content::BrowsingDataRemover::DATA_TYPE_ON_STORAGE_PARTITION |
    content::BrowsingDataRemover::DATA_TYPE_MEDIA_LICENSES;

// Origin type mask for complete data removal.
constexpr content::BrowsingDataRemover::OriginType
    kDataRemovalOriginProtectionTypes =
        content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB |
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB;

struct OriginUsageAndQuota {
  std::string origin;
  int64_t usage;
  int64_t quota;
};

using GetOriginsResultCallback =
    base::OnceCallback<void(std::vector<std::optional<OriginUsageAndQuota>>)>;

// This object manages its own lifetime. It deletes itself once the callback has
// been invoked.
class DeleteDataObserver : public content::BrowsingDataRemover::Observer {
 public:
  explicit DeleteDataObserver(content::BrowsingDataRemover* data_remover,
                              const base::android::JavaRef<jobject>& callback)
      : observation_(this), callback_(callback) {
    observation_.Observe(data_remover);
  }
  ~DeleteDataObserver() override = default;

  void OnBrowsingDataRemoverDone(uint64_t failed_data_types) override {
    bool success = failed_data_types == 0;
    // Post handling of callback to UI thread to avoid any long-running tasks
    // from blocking or modifications of the data_remover_ observer list from
    // interfering with iteration through observers.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&DeleteDataObserver::OnRemovalDone,
                                  base::Unretained(this), success));
  }

  // Callback to be executed on the UI thread after removal is done.
  // Will call the Java callback and delete itself.
  void OnRemovalDone(bool success) {
    base::android::RunBooleanCallbackAndroid(callback_, success);
    delete this;
  }

 private:
  base::ScopedObservation<content::BrowsingDataRemover,
                          content::BrowsingDataRemover::Observer>
      observation_;
  base::android::ScopedJavaGlobalRef<jobject> callback_;
};

std::string GetRegisterableDomain(const std::string& host) {
  std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
      host, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  // The method above returns empty string for IP addresses and "localhost".
  return domain.empty() ? host : domain;
}

}  // namespace

// static
scoped_refptr<AwQuotaManagerBridge> AwQuotaManagerBridge::Create(
    AwBrowserContext* browser_context) {
  return new AwQuotaManagerBridge(browser_context);
}

AwQuotaManagerBridge::AwQuotaManagerBridge(AwBrowserContext* browser_context)
    : browser_context_(browser_context) {}

AwQuotaManagerBridge::~AwQuotaManagerBridge() = default;

StoragePartition* AwQuotaManagerBridge::GetStoragePartition() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // AndroidWebview does not use per-site storage partitions.
  StoragePartition* storage_partition =
      browser_context_->GetDefaultStoragePartition();
  DCHECK(storage_partition);
  return storage_partition;
}

scoped_refptr<storage::QuotaManagerProxy>
AwQuotaManagerBridge::GetQuotaManagerProxy() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy =
      GetStoragePartition()->GetQuotaManagerProxy();
  DCHECK(quota_manager_proxy);
  return quota_manager_proxy;
}

void AwQuotaManagerBridge::DeleteBrowsingData(
    const base::android::JavaRef<jobject>& jcallback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::BrowsingDataRemover* data_remover =
      browser_context_->GetBrowsingDataRemover();
  // DeleteDataObserver manages its own lifetime.
  DeleteDataObserver* observer =
      new DeleteDataObserver(data_remover, jcallback);
  data_remover->RemoveAndReply(base::Time(), base::Time::Max(),
                               kDataRemovalMask,
                               kDataRemovalOriginProtectionTypes, observer);
}

std::string AwQuotaManagerBridge::DeleteBrowsingDataForSite(
    const std::string& domain,
    const base::android::JavaRef<jobject>& jcallback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder =
      content::BrowsingDataFilterBuilder::Create(
          content::BrowsingDataFilterBuilder::Mode::kDelete,
          content::BrowsingDataFilterBuilder::OriginMatchingMode::
              kOriginAndThirdParty);
  std::string site = GetRegisterableDomain(domain);
  filter_builder->AddRegisterableDomain(site);

  content::BrowsingDataRemover* data_remover =
      browser_context_->GetBrowsingDataRemover();
  // DeleteDataObserver manages its own lifetime.
  DeleteDataObserver* observer =
      new DeleteDataObserver(data_remover, jcallback);
  data_remover->RemoveWithFilterAndReply(
      base::Time(), base::Time::Max(), kDataRemovalMask,
      kDataRemovalOriginProtectionTypes, std::move(filter_builder), observer);

  return site;
}

void AwQuotaManagerBridge::DeleteAllDataFramework() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // (Legacy) Clear all web storage data except cookies.
  uint32_t remove_mask = StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                         StoragePartition::REMOVE_DATA_MASK_INDEXEDDB |
                         StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE;
  GetStoragePartition()->ClearData(remove_mask, blink::StorageKey(),
                                   base::Time(), base::Time::Max(),
                                   base::DoNothing());
}

void AwQuotaManagerBridge::DeleteOriginFramework(const std::u16string& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  StoragePartition* storage_partition = GetStoragePartition();
  // All (temporary) QuotaClient types.
  uint32_t remove_mask = StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
                         StoragePartition::REMOVE_DATA_MASK_INDEXEDDB;
  storage_partition->ClearDataForOrigin(remove_mask, GURL(origin),
                                        base::DoNothing());
}

void AwQuotaManagerBridge::GetOrigins(JNIEnv* env,
                                      const JavaRef<jobject>& object,
                                      const JavaRef<jobject>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetOriginsResultCallback ui_callback = base::BindOnce(
      [](const JavaRef<jobject>& obj, const JavaRef<jobject>& callback,
         std::vector<std::optional<OriginUsageAndQuota>> results) {
        std::vector<std::string> origins;
        std::vector<int64_t> usages;
        std::vector<int64_t> quotas;
        for (std::optional<OriginUsageAndQuota>& result : results) {
          if (result) {
            origins.push_back(std::move(result->origin));
            usages.push_back(result->usage);
            quotas.push_back(result->quota);
          }
        }
        JNIEnv* env = AttachCurrentThread();
        Java_AwQuotaManagerBridge_onGetOriginsCallback(env, obj, callback,
                                                       origins, usages, quotas);
      },
      ScopedJavaGlobalRef<jobject>(env, object),
      ScopedJavaGlobalRef<jobject>(env, callback));
  GetQuotaManagerProxy()->PostTaskWithQuotaManager(base::BindOnce(
      [](GetOriginsResultCallback callback,
         base::WeakPtr<storage::QuotaManagerImpl> quota_manager) {
        if (!quota_manager) {
          std::move(callback).Run({});
          return;
        }
        quota_manager->GetAllStorageKeys(base::BindOnce(
            [](base::WeakPtr<storage::QuotaManagerImpl> quota_manager,
               GetOriginsResultCallback callback,
               const std::set<blink::StorageKey>& storage_keys) {
              base::ConcurrentCallbacks<std::optional<OriginUsageAndQuota>>
                  aggregator;
              for (const blink::StorageKey& storage_key : storage_keys) {
                quota_manager->GetUsageAndQuota(
                    storage_key,
                    base::BindOnce(
                        [](const blink::StorageKey& storage_key,
                           blink::mojom::QuotaStatusCode status, int64_t usage,
                           int64_t quota)
                            -> std::optional<OriginUsageAndQuota> {
                          if (status != blink::mojom::QuotaStatusCode::kOk) {
                            return std::nullopt;
                          }
                          return OriginUsageAndQuota{
                              storage_key.origin().GetURL().spec(), usage,
                              quota};
                        },
                        storage_key)
                        .Then(aggregator.CreateCallback()));
              }
              std::move(aggregator).Done(std::move(callback));
            },
            quota_manager, std::move(callback)));
      },
      base::BindPostTaskToCurrentDefault(std::move(ui_callback))));
}

void AwQuotaManagerBridge::GetUsageAndQuotaForOrigin(
    const std::u16string& origin,
    const JavaRef<jobject>& callback,
    bool is_quota) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(crbug.com/41417435): Use helper for url::Origin creation from string.
  GetQuotaManagerProxy()->GetUsageAndQuota(
      blink::StorageKey::CreateFirstParty(url::Origin::Create(GURL(origin))),
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(
          [](bool is_quota, blink::mojom::QuotaStatusCode status, int64_t usage,
             int64_t quota) -> int64_t {
            return status == blink::mojom::QuotaStatusCode::kOk
                       ? (is_quota ? quota : usage)
                       : 0;
          },
          is_quota)
          .Then(base::BindOnce(&base::android::RunLongCallbackAndroid,
                               ScopedJavaGlobalRef<jobject>(callback))));
}

}  // namespace android_webview

DEFINE_JNI(AwQuotaManagerBridge)
