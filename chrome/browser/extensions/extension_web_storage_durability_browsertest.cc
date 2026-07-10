// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_model_delegate.h"
#include "chrome/browser/extensions/browsing_data_test_utils.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browsing_data/content/browsing_data_model.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

// This file contains tests that verify how web storage (local storage,
// IndexedDB, etc.) used on extension origins interacts with browsing data
// removal systems (storage quota management, user-initiated browsing data
// deletion, etc.).

namespace extensions {
namespace utils = extensions::browsing_data_test_utils;

namespace {

constexpr char kManifestWithoutUnlimitedStorage[] = R"({
  "name": "Test Extension Without unlimitedStorage",
  "manifest_version": 3,
  "version": "0.1",
  "background": {"service_worker": "background.js"}
})";

constexpr char kManifestWithUnlimitedStorage[] = R"({
  "name": "Test Extension With unlimitedStorage",
  "manifest_version": 3,
  "version": "0.1",
  "background": {"service_worker": "background.js"},
  "permissions": ["unlimitedStorage"]
})";

class ExtensionWebStorageDurabilityTest : public ExtensionBrowserTest {
 protected:
  // Loads a test extension with the provided manifest. The manifest may
  // optionally declare a background script using the path "background.js".
  const Extension* LoadTestExtension(const char* manifest) {
    test_dir_.WriteManifest(manifest);
    test_dir_.WriteFile(FILE_PATH_LITERAL("background.js"), "");
    const Extension* extension = LoadExtension(test_dir_.UnpackedPath());

    if (!extension) {
      ADD_FAILURE() << "Failed to load extension.";
      return nullptr;
    }

    return extension;
  }

  // Loads a test extension with `manifest`. The manifest must declare a
  // background script using the path "background.js".
  const Extension* LoadTestExtensionWithIdb(const char* manifest) {
    constexpr char kBackgroundInitIdb[] = R"(
      new Promise(resolve => {
        const req = indexedDB.open('test-db', 1);
        req.onsuccess = () => {
          req.result.close();
          resolve();
        };
        req.onerror = () => {
          chrome.test.sendMessage('error-idb-open');
        }
      }).then(() => chrome.test.sendMessage('idb-ready'));
    )";

    test_dir_.WriteManifest(manifest);
    test_dir_.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundInitIdb);
    ExtensionTestMessageListener idb_listener("idb-ready");
    idb_listener.set_failure_message("error-idb-open");
    const Extension* extension = LoadExtension(test_dir_.UnpackedPath());

    if (!extension) {
      ADD_FAILURE() << "Failed to load extension.";
      return nullptr;
    }
    if (!idb_listener.WaitUntilSatisfied()) {
      ADD_FAILURE() << "Failed to initialize IDB.";
      return nullptr;
    }

    return extension;
  }

  // Loads an extension that can use the Browsing Data API to verify data
  // deletion flows initiated by extensions. This helper is meant to be used in
  // combination with `LoadBrowsingDataRemoverExtension`.
  const Extension* LoadBrowsingDataRemoverExtension() {
    data_remover_dir_.WriteManifest(R"({
      "name": "Browsing Data Remover",
      "manifest_version": 3,
      "version": "0.1",
      "background": {"service_worker": "background.js"},
      "permissions": ["browsingData"]
    })");
    data_remover_dir_.WriteFile(FILE_PATH_LITERAL("background.js"), "");
    const Extension* extension =
        LoadExtension(data_remover_dir_.UnpackedPath());

    if (!extension) {
      ADD_FAILURE() << "Failed to load extension.";
      return nullptr;
    }

    return extension;
  }

  // Allows tests to call `chrome.browsingData.remove()` custom `options_json`
  // and `data_types_json` provided.
  //
  // To use this helper, you must first call `LoadBrowsingDataRemoverExtension`
  // to load the data remover helper extension.
  bool BrowsingDataRemoverCallRemove(const Extension* extension,
                                     const std::string& options_json,
                                     const std::string& data_types_json) {
    static constexpr char kBrowsingDataRemoveScriptTemplate[] =
        "chrome.browsingData.remove(%s, %s).then(() => "
        "    chrome.test.sendScriptResult('done'));";
    std::string script =
        base::StringPrintf(kBrowsingDataRemoveScriptTemplate,
                           options_json.c_str(), data_types_json.c_str());

    base::Value value = BackgroundScriptExecutor::ExecuteScript(
        profile(), extension->id(), script,
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);

    return value == "done";
  }

  // Returns a JSON array containing the names of the IndexedDB databases
  // available in the `extension` extension's background context.
  base::Value TestExtensionCheckWebStorage(const Extension* extension) {
    static constexpr char kScript[] = R"(
        (async () => {
          const dbs = (await indexedDB.databases()).map(db => db.name);
          chrome.test.sendScriptResult(dbs);
        })();
    )";

    base::Value value = BackgroundScriptExecutor::ExecuteScript(
        profile(), extension->id(), kScript,
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
    return value;
  }

  // Retrieves the storage key of a fixed HTTPS origin for testing purposes.
  static blink::StorageKey GetWebStorageKey() {
    return blink::StorageKey::CreateFromStringForTesting("https://example.com");
  }

  // Retrieves the storage key of the `extension`.
  static blink::StorageKey GetExtensionStorageKey(const Extension* extension) {
    return blink::StorageKey::CreateFirstParty(extension->origin());
  }

  // Synchronously removes data types matching `remove_mask` on origins
  // specified in `origin_type_mask`.
  void RunBrowsingDataRemoval(uint64_t remove_mask, uint64_t origin_type_mask) {
    content::BrowsingDataRemover* remover = profile()->GetBrowsingDataRemover();
    content::BrowsingDataRemoverCompletionObserver observer(remover);
    remover->RemoveAndReply(base::Time(), base::Time::Max(), remove_mask,
                            origin_type_mask, &observer);
    observer.BlockUntilCompletion();
  }

  // Retrieve a set of buckets that `QuotaManagerImpl` can delete when under
  // storage pressure.
  std::set<storage::BucketLocator> GetEvictionCandidates() {
    scoped_refptr<storage::QuotaManager> quota_manager = base::WrapRefCounted(
        profile()->GetDefaultStoragePartition()->GetQuotaManager());
    std::set<storage::BucketLocator> result;
    base::RunLoop run_loop;

    scoped_refptr<base::SequencedTaskRunner> ui_runner =
        base::SequencedTaskRunner::GetCurrentDefault();

    auto on_got_eviction_buckets = base::BindOnce(
        [](std::set<storage::BucketLocator>* result, base::OnceClosure quit,
           const std::set<storage::BucketLocator>& buckets) {
          *result = buckets;
          std::move(quit).Run();
        },
        &result, run_loop.QuitClosure());

    auto get_eviction_buckets = base::BindOnce(
        [](scoped_refptr<storage::QuotaManager> quota_manager,
           scoped_refptr<base::SequencedTaskRunner> reply_runner,
           base::OnceCallback<void(const std::set<storage::BucketLocator>&)>
               on_got) {
          quota_manager->GetEvictionBuckets(
              INT64_MAX,
              base::BindPostTask(std::move(reply_runner), std::move(on_got)));
        },
        std::move(quota_manager), std::move(ui_runner),
        std::move(on_got_eviction_buckets));

    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, std::move(get_eviction_buckets));

    {
      SCOPED_TRACE("Retrieving buckets that QuotaManagerImpl will evict");

      run_loop.Run();
    }

    return result;
  }

  void ClearStorageKeyFromDefaultStoragePartition(
      const blink::StorageKey& key) {
    base::RunLoop run_loop;
    profile()->GetDefaultStoragePartition()->ClearData(
        content::StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE, key,
        base::Time(), base::Time::Max(), run_loop.QuitClosure());

    {
      SCOPED_TRACE("Clearing storage key in default storage partition");
      run_loop.Run();
    }
  }

 private:
  TestExtensionDir test_dir_;
  TestExtensionDir data_remover_dir_;
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Emulate storage quota eviction using `QuotaManagerImpl::GetEvictionBuckets`

// Verify that `QuotaManagerImpl` DOES identify an extension _without_ the
// "unlimitedStorage" as a candidate for eviction.
IN_PROC_BROWSER_TEST_F(ExtensionWebStorageDurabilityTest,
                       Quota_Eviction_WithoutUnlimitedStorage) {
  const Extension* extension =
      LoadTestExtensionWithIdb(kManifestWithoutUnlimitedStorage);
  ASSERT_TRUE(extension);

  blink::StorageKey ext_key = GetExtensionStorageKey(extension);

  std::set<storage::BucketLocator> candidates = GetEvictionCandidates();

  bool found = std::ranges::any_of(
      candidates, [&ext_key](const storage::BucketLocator& loc) {
        return loc.storage_key == ext_key;
      });

  EXPECT_TRUE(found);
}

// Verify that `QuotaManagerImpl` DOES NOT identify an extension _with_ the
// "unlimitedStorage" as a candidate for eviction.
IN_PROC_BROWSER_TEST_F(ExtensionWebStorageDurabilityTest,
                       Quota_Eviction_WithUnlimitedStorage) {
  const Extension* extension =
      LoadTestExtensionWithIdb(kManifestWithUnlimitedStorage);
  ASSERT_TRUE(extension);

  blink::StorageKey ext_key = GetExtensionStorageKey(extension);

  std::set<storage::BucketLocator> candidates = GetEvictionCandidates();

  bool found = std::ranges::any_of(
      candidates, [&ext_key](const storage::BucketLocator& loc) {
        return loc.storage_key == ext_key;
      });

  EXPECT_FALSE(found);
}
////////////////////////////////////////////////////////////////////////////////
// chrome.browsingData.remove() API flows

// Verify that an extension's indexedDB is retained when another extension calls
// `chrome.browsingData.remove()` with default `originTypes`.
IN_PROC_BROWSER_TEST_F(ExtensionWebStorageDurabilityTest,
                       ChromeBrowsingData_DefaultOriginTypes) {
  const Extension* extension =
      LoadTestExtensionWithIdb(kManifestWithoutUnlimitedStorage);
  ASSERT_TRUE(extension);
  EXPECT_THAT(TestExtensionCheckWebStorage(extension),
              base::test::IsJson(R"(["test-db"])"));

  const Extension* data_remover = LoadBrowsingDataRemoverExtension();
  ASSERT_TRUE(data_remover);

  bool data_removed = BrowsingDataRemoverCallRemove(
      data_remover, R"({"since": 0})", R"({"indexedDB": true})");
  ASSERT_TRUE(data_removed);
  EXPECT_THAT(TestExtensionCheckWebStorage(extension),
              base::test::IsJson(R"(["test-db"])"));
}

// Verify that an extension's indexedDB is retained when another extension calls
// `chrome.browsingData.remove()` and includes "unprotectedWeb" in
// `originTypes`.
IN_PROC_BROWSER_TEST_F(ExtensionWebStorageDurabilityTest,
                       ChromeBrowsingData_UnprotectedWebOnly) {
  const Extension* extension =
      LoadTestExtensionWithIdb(kManifestWithoutUnlimitedStorage);
  ASSERT_TRUE(extension);
  EXPECT_THAT(TestExtensionCheckWebStorage(extension),
              base::test::IsJson(R"(["test-db"])"));

  const Extension* data_remover = LoadBrowsingDataRemoverExtension();
  ASSERT_TRUE(data_remover);

  bool data_removed = BrowsingDataRemoverCallRemove(
      data_remover, R"({"since": 0, "originTypes": {"unprotectedWeb": true}})",
      R"({"indexedDB": true})");
  ASSERT_TRUE(data_removed);
  EXPECT_THAT(TestExtensionCheckWebStorage(extension),
              base::test::IsJson(R"(["test-db"])"));
}

// Verify that an extension's indexedDB is retained when another extension calls
// `chrome.browsingData.remove()` and includes "protectedWeb" in `originTypes`.
IN_PROC_BROWSER_TEST_F(ExtensionWebStorageDurabilityTest,
                       ChromeBrowsingData_ProtectedWebOnly) {
  const Extension* extension =
      LoadTestExtensionWithIdb(kManifestWithoutUnlimitedStorage);
  ASSERT_TRUE(extension);
  EXPECT_THAT(TestExtensionCheckWebStorage(extension),
              base::test::IsJson(R"(["test-db"])"));

  const Extension* data_remover = LoadBrowsingDataRemoverExtension();
  ASSERT_TRUE(data_remover);

  bool data_removed = BrowsingDataRemoverCallRemove(
      data_remover, R"({"since": 0, "originTypes": {"protectedWeb": true}})",
      R"({"indexedDB": true})");
  ASSERT_TRUE(data_removed);
  EXPECT_THAT(TestExtensionCheckWebStorage(extension),
              base::test::IsJson(R"(["test-db"])"));
}

// Verify that an extension's indexedDB is REMOVED when another extension calls
// `chrome.browsingData.remove()` and includes "extensions" in `originTypes`.
IN_PROC_BROWSER_TEST_F(ExtensionWebStorageDurabilityTest,
                       ChromeBrowsingData_ExtensionsOnly) {
  const Extension* extension =
      LoadTestExtensionWithIdb(kManifestWithoutUnlimitedStorage);
  ASSERT_TRUE(extension);
  EXPECT_THAT(TestExtensionCheckWebStorage(extension),
              base::test::IsJson(R"(["test-db"])"));

  const Extension* data_remover = LoadBrowsingDataRemoverExtension();
  ASSERT_TRUE(data_remover);

  bool data_removed = BrowsingDataRemoverCallRemove(
      data_remover, R"({"since": 0, "originTypes": {"extension": true}})",
      R"({"indexedDB": true})");
  ASSERT_TRUE(data_removed);
  EXPECT_THAT(TestExtensionCheckWebStorage(extension),
              base::test::IsJson(R"([])"));
}

////////////////////////////////////////////////////////////////////////////////
// Emulate deletion via chrome://settings/clearBrowsingData using
// BrowsingDataRemover::RemoveAndReply
//
// See also content/browser/browsing_data/browsing_data_remover_impl_unittest.cc
// for tests that include `chrome-extension://` in their origins.

// Verify that deleting browsing data on unprotected websites:
// - Deletes a normal website's web storage
// - Does not delete an extension's web storage
IN_PROC_BROWSER_TEST_F(ExtensionWebStorageDurabilityTest,
                       BrowsingDataRemover_UnprotectedWeb) {
  const Extension* extension =
      LoadTestExtension(kManifestWithoutUnlimitedStorage);
  ASSERT_TRUE(extension);

  blink::StorageKey ext_key = GetExtensionStorageKey(extension);
  blink::StorageKey web_key = GetWebStorageKey();

  utils::CreateLocalStorageForKey(profile(), ext_key);
  utils::CreateLocalStorageForKey(profile(), web_key);

  RunBrowsingDataRemoval(
      content::BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB);

  auto usage_info = utils::GetLocalStorageInfo(profile());
  EXPECT_TRUE(
      utils::UsageInfosHasStorageKey(usage_info, ext_key));  // ext is preserved
  EXPECT_FALSE(
      utils::UsageInfosHasStorageKey(usage_info, web_key));  // web is cleared
}

// Verify that deleting browsing data on protected websites:
// - Does not delete a normal website's web storage
// - Does not delete an extension's web storage
IN_PROC_BROWSER_TEST_F(ExtensionWebStorageDurabilityTest,
                       BrowsingDataRemover_ProtectedWeb) {
  const Extension* extension =
      LoadTestExtension(kManifestWithoutUnlimitedStorage);
  ASSERT_TRUE(extension);

  blink::StorageKey ext_key = GetExtensionStorageKey(extension);
  blink::StorageKey web_key = GetWebStorageKey();

  utils::CreateLocalStorageForKey(profile(), ext_key);
  utils::CreateLocalStorageForKey(profile(), web_key);

  RunBrowsingDataRemoval(
      content::BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE,
      content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB);

  auto usage_info = utils::GetLocalStorageInfo(profile());

  // Only protected storage was targeted, so both the normal website and
  // extension should be retained.
  EXPECT_TRUE(
      utils::UsageInfosHasStorageKey(usage_info, ext_key));  // ext preserved
  EXPECT_TRUE(
      utils::UsageInfosHasStorageKey(usage_info, web_key));  // web preserved
}

////////////////////////////////////////////////////////////////////////////////
// Emulate web storage deletion via chrome://settings/content/all
// BrowsingDataModel

// Verify that BrowsingDataModel (used by the chrome://settings/cookies UI)
// contains websites, but does not contain extension origins.
IN_PROC_BROWSER_TEST_F(ExtensionWebStorageDurabilityTest,
                       BrowsingDataModel_NoExtensionOrigins) {
  const Extension* extension =
      LoadTestExtension(kManifestWithoutUnlimitedStorage);
  ASSERT_TRUE(extension);

  url::Origin ext_origin = extension->origin();
  blink::StorageKey ext_key = GetExtensionStorageKey(extension);
  blink::StorageKey web_key = GetWebStorageKey();

  utils::CreateLocalStorageForKey(profile(), ext_key);
  utils::CreateLocalStorageForKey(profile(), web_key);

  ASSERT_TRUE(utils::UsageInfosHasStorageKey(
      browsing_data_test_utils::GetLocalStorageInfo(profile()), ext_key));

  base::test::TestFuture<std::unique_ptr<BrowsingDataModel>> future;
  BrowsingDataModel::BuildFromDisk(
      profile(), ChromeBrowsingDataModelDelegate::CreateForProfile(profile()),
      future.GetCallback());

  std::unique_ptr<BrowsingDataModel> model = future.Take();
  ASSERT_TRUE(model);

  // Verify that the extension's origin is not matched.
  ASSERT_EQ(1u, model->size());
  const auto& entry = *model->begin();
  ASSERT_EQ(BrowsingDataModel::GetOriginForDataKey(*entry.data_key),
            BrowsingDataModel::GetOriginForDataKey(web_key));
}

////////////////////////////////////////////////////////////////////////////////
// Emulate the DevTools deletion flow using StoragePartition::ClearData

// Verify that "Clear site data" in DevTools works on an extension without the
// "unlimitedStorage" permission.
IN_PROC_BROWSER_TEST_F(
    ExtensionWebStorageDurabilityTest,
    StoragePartitionClearData_ExtensionWithoutUnlimitedStorage) {
  const Extension* extension =
      LoadTestExtension(kManifestWithoutUnlimitedStorage);
  ASSERT_TRUE(extension);

  blink::StorageKey ext_key = GetExtensionStorageKey(extension);

  utils::CreateLocalStorageForKey(profile(), ext_key);
  ASSERT_TRUE(utils::UsageInfosHasStorageKey(
      browsing_data_test_utils::GetLocalStorageInfo(profile()), ext_key));

  ClearStorageKeyFromDefaultStoragePartition(ext_key);

  EXPECT_FALSE(utils::UsageInfosHasStorageKey(
      browsing_data_test_utils::GetLocalStorageInfo(profile()), ext_key));
}

// Verify that "Clear site data" in DevTools works on an extension with the
// "unlimitedStorage" permission.
IN_PROC_BROWSER_TEST_F(
    ExtensionWebStorageDurabilityTest,
    StoragePartitionClearData_ExtensionWithUnlimitedStorage) {
  const Extension* extension = LoadTestExtension(kManifestWithUnlimitedStorage);
  ASSERT_TRUE(extension);

  blink::StorageKey ext_key = GetExtensionStorageKey(extension);

  utils::CreateLocalStorageForKey(profile(), ext_key);
  ASSERT_TRUE(utils::UsageInfosHasStorageKey(
      browsing_data_test_utils::GetLocalStorageInfo(profile()), ext_key));

  ClearStorageKeyFromDefaultStoragePartition(ext_key);

  EXPECT_FALSE(utils::UsageInfosHasStorageKey(
      browsing_data_test_utils::GetLocalStorageInfo(profile()), ext_key));
}

}  // namespace extensions
