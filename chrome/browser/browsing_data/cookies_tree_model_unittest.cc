// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/cookies_tree_model.h"

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browsing_data/mock_browsing_data_appcache_helper.h"
#include "chrome/browser/browsing_data/mock_browsing_data_cache_storage_helper.h"
#include "chrome/browser/browsing_data/mock_browsing_data_cookie_helper.h"
#include "chrome/browser/browsing_data/mock_browsing_data_database_helper.h"
#include "chrome/browser/browsing_data/mock_browsing_data_file_system_helper.h"
#include "chrome/browser/browsing_data/mock_browsing_data_flash_lso_helper.h"
#include "chrome/browser/browsing_data/mock_browsing_data_indexed_db_helper.h"
#include "chrome/browser/browsing_data/mock_browsing_data_local_storage_helper.h"
#include "chrome/browser/browsing_data/mock_browsing_data_media_license_helper.h"
#include "chrome/browser/browsing_data/mock_browsing_data_quota_helper.h"
#include "chrome/browser/browsing_data/mock_browsing_data_service_worker_helper.h"
#include "chrome/browser/browsing_data/mock_browsing_data_shared_worker_helper.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/mock_settings_observer.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/buildflags/buildflags.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#endif

using ::testing::_;
using content::BrowserThread;

namespace {

class CookiesTreeModelTest : public testing::Test {
 public:
  ~CookiesTreeModelTest() override {
    // Avoid memory leaks.
#if BUILDFLAG(ENABLE_EXTENSIONS)
    special_storage_policy_ = nullptr;
#endif
    profile_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void SetUp() override {
    profile_.reset(new TestingProfile());
    mock_browsing_data_cookie_helper_ =
        new MockBrowsingDataCookieHelper(profile_.get());
    mock_browsing_data_database_helper_ =
        new MockBrowsingDataDatabaseHelper(profile_.get());
    mock_browsing_data_local_storage_helper_ =
        new MockBrowsingDataLocalStorageHelper(profile_.get());
    mock_browsing_data_session_storage_helper_ =
        new MockBrowsingDataLocalStorageHelper(profile_.get());
    mock_browsing_data_appcache_helper_ =
        new MockBrowsingDataAppCacheHelper(profile_.get());
    mock_browsing_data_indexed_db_helper_ =
        new MockBrowsingDataIndexedDBHelper(profile_.get());
    mock_browsing_data_file_system_helper_ =
        new MockBrowsingDataFileSystemHelper(profile_.get());
    mock_browsing_data_quota_helper_ =
        new MockBrowsingDataQuotaHelper(profile_.get());
    mock_browsing_data_service_worker_helper_ =
        new MockBrowsingDataServiceWorkerHelper(profile_.get());
    mock_browsing_data_shared_worker_helper_ =
        new MockBrowsingDataSharedWorkerHelper(profile_.get());
    mock_browsing_data_cache_storage_helper_ =
        new MockBrowsingDataCacheStorageHelper(profile_.get());
    mock_browsing_data_flash_lso_helper_ =
        new MockBrowsingDataFlashLSOHelper(profile_.get());
    mock_browsing_data_media_license_helper_ =
        new MockBrowsingDataMediaLicenseHelper(profile_.get());

    const char kExtensionScheme[] = "extensionscheme";
    scoped_refptr<content_settings::CookieSettings> cookie_settings =
        new content_settings::CookieSettings(
            HostContentSettingsMapFactory::GetForProfile(profile_.get()),
            profile_->GetPrefs(), profile_->IsIncognitoProfile(),
            kExtensionScheme);
#if BUILDFLAG(ENABLE_EXTENSIONS)
    special_storage_policy_ =
        new ExtensionSpecialStoragePolicy(cookie_settings.get());
#endif
  }

  void TearDown() override {
    mock_browsing_data_service_worker_helper_ = nullptr;
    mock_browsing_data_shared_worker_helper_ = nullptr;
    mock_browsing_data_cache_storage_helper_ = nullptr;
    mock_browsing_data_quota_helper_ = nullptr;
    mock_browsing_data_file_system_helper_ = nullptr;
    mock_browsing_data_indexed_db_helper_ = nullptr;
    mock_browsing_data_appcache_helper_ = nullptr;
    mock_browsing_data_session_storage_helper_ = nullptr;
    mock_browsing_data_local_storage_helper_ = nullptr;
    mock_browsing_data_database_helper_ = nullptr;
    mock_browsing_data_flash_lso_helper_ = nullptr;
    mock_browsing_data_media_license_helper_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }
  std::unique_ptr<CookiesTreeModel> CreateCookiesTreeModelWithInitialSample() {
    auto container = std::make_unique<LocalDataContainer>(
        mock_browsing_data_cookie_helper_, mock_browsing_data_database_helper_,
        mock_browsing_data_local_storage_helper_,
        mock_browsing_data_session_storage_helper_,
        mock_browsing_data_appcache_helper_,
        mock_browsing_data_indexed_db_helper_,
        mock_browsing_data_file_system_helper_,
        mock_browsing_data_quota_helper_,
        mock_browsing_data_service_worker_helper_,
        mock_browsing_data_shared_worker_helper_,
        mock_browsing_data_cache_storage_helper_,
        mock_browsing_data_flash_lso_helper_,
        mock_browsing_data_media_license_helper_);

    auto cookies_model = std::make_unique<CookiesTreeModel>(
        std::move(container), special_storage_policy());
    mock_browsing_data_cookie_helper_->
        AddCookieSamples(GURL("http://foo1"), "A=1");
    mock_browsing_data_cookie_helper_->
        AddCookieSamples(GURL("http://foo2"), "B=1");
    mock_browsing_data_cookie_helper_->
        AddCookieSamples(GURL("http://foo3"), "C=1");
    mock_browsing_data_cookie_helper_->Notify();
    mock_browsing_data_database_helper_->AddDatabaseSamples();
    mock_browsing_data_database_helper_->Notify();
    mock_browsing_data_local_storage_helper_->AddLocalStorageSamples();
    mock_browsing_data_local_storage_helper_->Notify();
    mock_browsing_data_session_storage_helper_->AddLocalStorageSamples();
    mock_browsing_data_session_storage_helper_->Notify();
    mock_browsing_data_indexed_db_helper_->AddIndexedDBSamples();
    mock_browsing_data_indexed_db_helper_->Notify();
    mock_browsing_data_file_system_helper_->AddFileSystemSamples();
    mock_browsing_data_file_system_helper_->Notify();
    mock_browsing_data_quota_helper_->AddQuotaSamples();
    mock_browsing_data_quota_helper_->Notify();
    mock_browsing_data_service_worker_helper_->AddServiceWorkerSamples();
    mock_browsing_data_service_worker_helper_->Notify();
    mock_browsing_data_shared_worker_helper_->AddSharedWorkerSamples();
    mock_browsing_data_shared_worker_helper_->Notify();
    mock_browsing_data_cache_storage_helper_->AddCacheStorageSamples();
    mock_browsing_data_cache_storage_helper_->Notify();
    mock_browsing_data_flash_lso_helper_->AddFlashLSODomain("xyz.com");
    mock_browsing_data_flash_lso_helper_->Notify();
    mock_browsing_data_media_license_helper_->AddMediaLicenseSamples();
    mock_browsing_data_media_license_helper_->Notify();

    {
      SCOPED_TRACE(
          "Initial State 3 cookies, 2 databases, 2 local storages, "
          "2 session storages, 2 indexed DBs, 3 filesystems, "
          "2 quotas, 2 service workers, 2 shared workers,"
          "2 cache storages, 1 Flash LSO, 2 media licenses");
      // 71 because there's the root, then
      // cshost1 -> cache storage -> https://cshost1:1/
      // cshost2 -> cache storage -> https://cshost2:2/
      // foo1 -> cookies -> a,
      // foo2 -> cookies -> b,
      // foo3 -> cookies -> c,
      // gdbhost1 -> database -> http://gdbhost1:1/,
      // gdbhost2 -> database -> http://gdbhost2:2/,
      // host1 -> localstorage -> http://host1:1/,
      //       -> sessionstorage -> http://host1:1/,
      // host2 -> localstorage -> http://host2:2/.
      //       -> sessionstorage -> http://host2:2/,
      // idbhost1 -> indexeddb -> http://idbhost1:1/,
      // idbhost2 -> indexeddb -> http://idbhost2:2/,
      // fshost1 -> filesystem -> http://fshost1:1/,
      // fshost2 -> filesystem -> http://fshost2:1/,
      // fshost3 -> filesystem -> http://fshost3:1/,
      // media1 -> media_licenses -> media_license,
      // media2 -> media_licenses -> media_license,
      // quotahost1 -> quotahost1,
      // quotahost2 -> quotahost2,
      // swhost1 -> service worker -> https://swhost1:1
      // swhost2 -> service worker -> https://swhost1:2
      // sharedworkerhost1 -> shared worker -> https://sharedworkerhost1:1,
      // sharedworkerhost2 -> shared worker -> https://sharedworkerhost2:2,
      // xyz.com -> flash_lsos
      EXPECT_EQ(71, cookies_model->GetRoot()->GetTotalNodeCount());
      EXPECT_EQ("A,B,C", GetDisplayedCookies(cookies_model.get()));
      EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
                GetDisplayedDatabases(cookies_model.get()));
      EXPECT_EQ("http://host1:1/,http://host2:2/",
                GetDisplayedLocalStorages(cookies_model.get()));
      EXPECT_EQ("http://host1:1/,http://host2:2/",
                GetDisplayedSessionStorages(cookies_model.get()));
      EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
                GetDisplayedIndexedDBs(cookies_model.get()));
      EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
                GetDisplayedFileSystems(cookies_model.get()));
      EXPECT_EQ("quotahost1,quotahost2",
                GetDisplayedQuotas(cookies_model.get()));
      EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
                GetDisplayedServiceWorkers(cookies_model.get()));
      EXPECT_EQ(
          "https://sharedworkerhost1:1/app/worker.js,"
          "https://sharedworkerhost2:2/worker.js",
          GetDisplayedSharedWorkers(cookies_model.get()));
      EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
                GetDisplayedCacheStorages(cookies_model.get()));
      EXPECT_EQ("xyz.com", GetDisplayedFlashLSOs(cookies_model.get()));
      EXPECT_EQ("https://media1/,https://media2/",
                GetDisplayedMediaLicenses(cookies_model.get()));
    }
    return cookies_model;
  }

  // Checks that, when setting content settings for host nodes in the
  // cookie tree, the content settings are applied to the expected URL.
  void CheckContentSettingsUrlForHostNodes(
      const CookieTreeNode* node,
      CookieTreeNode::DetailedInfo::NodeType node_type,
      content_settings::CookieSettings* cookie_settings,
      const GURL& expected_url) {
    for (const auto& child : node->children()) {
      CheckContentSettingsUrlForHostNodes(child.get(),
                                          child->GetDetailedInfo().node_type,
                                          cookie_settings, expected_url);
    }

    ASSERT_EQ(node_type, node->GetDetailedInfo().node_type);

    if (node_type == CookieTreeNode::DetailedInfo::TYPE_HOST) {
      const CookieTreeHostNode* host =
          static_cast<const CookieTreeHostNode*>(node);

      if (expected_url.SchemeIsFile()) {
        EXPECT_FALSE(host->CanCreateContentException());
      } else {
        cookie_settings->ResetCookieSetting(expected_url);
        EXPECT_FALSE(cookie_settings->IsCookieSessionOnly(expected_url));

        host->CreateContentException(cookie_settings,
                                     CONTENT_SETTING_SESSION_ONLY);
        EXPECT_TRUE(cookie_settings->IsCookieSessionOnly(expected_url));
      }
    }
  }

  std::string GetNodesOfChildren(
      const CookieTreeNode* node,
      CookieTreeNode::DetailedInfo::NodeType node_type) {
    if (!node->children().empty()) {
      std::string retval;
      for (const auto& child : node->children())
        retval += GetNodesOfChildren(child.get(), node_type);
      return retval;
    }

    if (node->GetDetailedInfo().node_type != node_type)
      return std::string();

    // TODO: GetURL().spec() is used instead of Serialize() for backwards
    // compatibility with tests. The tests should be updated once all
    // appropriate parts have been migrated to url::Origin.
    switch (node_type) {
      case CookieTreeNode::DetailedInfo::TYPE_COOKIE:
        return node->GetDetailedInfo().cookie->Name() + ",";
      case CookieTreeNode::DetailedInfo::TYPE_APPCACHE:
      case CookieTreeNode::DetailedInfo::TYPE_CACHE_STORAGE:
      case CookieTreeNode::DetailedInfo::TYPE_DATABASE:
      case CookieTreeNode::DetailedInfo::TYPE_INDEXED_DB:
      case CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGE:
      case CookieTreeNode::DetailedInfo::TYPE_SERVICE_WORKER:
      case CookieTreeNode::DetailedInfo::TYPE_SESSION_STORAGE:
        return node->GetDetailedInfo().usage_info->origin.GetURL().spec() + ",";
      case CookieTreeNode::DetailedInfo::TYPE_FILE_SYSTEM:
        return node->GetDetailedInfo()
                   .file_system_info->origin.GetURL()
                   .spec() +
               ",";
      case CookieTreeNode::DetailedInfo::TYPE_QUOTA:
        return node->GetDetailedInfo().quota_info->host + ",";
      case CookieTreeNode::DetailedInfo::TYPE_SHARED_WORKER:
        return node->GetDetailedInfo().shared_worker_info->worker.spec() + ",";
      case CookieTreeNode::DetailedInfo::TYPE_FLASH_LSO:
        return node->GetDetailedInfo().flash_lso_domain + ",";
      case CookieTreeNode::DetailedInfo::TYPE_MEDIA_LICENSE:
        return node->GetDetailedInfo().media_license_info->origin.spec() + ",";
      default:
        return std::string();
    }
  }

  // Get the nodes names displayed in the view (if we had one) in the order
  // they are displayed, as a comma seperated string.
  // Ex: EXPECT_STREQ("X,Y", GetDisplayedNodes(cookies_view, type).c_str());
  std::string GetDisplayedNodes(CookiesTreeModel* cookies_model,
                                CookieTreeNode::DetailedInfo::NodeType type) {
    CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(
        cookies_model->GetRoot());
    std::string retval = GetNodesOfChildren(root, type);
    if (!retval.empty() && retval.back() == ',')
      retval.erase(retval.length() - 1);
    return retval;
  }

  std::string GetDisplayedCookies(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_COOKIE);
  }

  std::string GetDisplayedDatabases(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_DATABASE);
  }

  std::string GetDisplayedLocalStorages(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGE);
  }

  std::string GetDisplayedSessionStorages(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(
        cookies_model, CookieTreeNode::DetailedInfo::TYPE_SESSION_STORAGE);
  }

  std::string GetDisplayedAppCaches(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_APPCACHE);
  }

  std::string GetDisplayedIndexedDBs(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_INDEXED_DB);
  }

  std::string GetDisplayedFileSystems(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_FILE_SYSTEM);
  }

  std::string GetDisplayedQuotas(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_QUOTA);
  }

  std::string GetDisplayedServiceWorkers(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_SERVICE_WORKER);
  }

  std::string GetDisplayedSharedWorkers(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_SHARED_WORKER);
  }

  std::string GetDisplayedCacheStorages(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_CACHE_STORAGE);
  }

  std::string GetDisplayedFlashLSOs(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(
        cookies_model, CookieTreeNode::DetailedInfo::TYPE_FLASH_LSO);
  }

  std::string GetDisplayedMediaLicenses(CookiesTreeModel* cookies_model) {
    return GetDisplayedNodes(cookies_model,
                             CookieTreeNode::DetailedInfo::TYPE_MEDIA_LICENSE);
  }

  // Do not call on the root.
  void DeleteStoredObjects(CookieTreeNode* node) {
    node->DeleteStoredObjects();
    CookieTreeNode* parent_node = node->parent();
    DCHECK(parent_node);
    parent_node->GetModel()->Remove(parent_node, node);
  }

 protected:
  ExtensionSpecialStoragePolicy* special_storage_policy() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    return special_storage_policy_.get();
#else
    return nullptr;
#endif
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  scoped_refptr<MockBrowsingDataCookieHelper>
      mock_browsing_data_cookie_helper_;
  scoped_refptr<MockBrowsingDataDatabaseHelper>
      mock_browsing_data_database_helper_;
  scoped_refptr<MockBrowsingDataLocalStorageHelper>
      mock_browsing_data_local_storage_helper_;
  scoped_refptr<MockBrowsingDataLocalStorageHelper>
      mock_browsing_data_session_storage_helper_;
  scoped_refptr<MockBrowsingDataAppCacheHelper>
      mock_browsing_data_appcache_helper_;
  scoped_refptr<MockBrowsingDataIndexedDBHelper>
      mock_browsing_data_indexed_db_helper_;
  scoped_refptr<MockBrowsingDataFileSystemHelper>
      mock_browsing_data_file_system_helper_;
  scoped_refptr<MockBrowsingDataQuotaHelper>
      mock_browsing_data_quota_helper_;
  scoped_refptr<MockBrowsingDataServiceWorkerHelper>
      mock_browsing_data_service_worker_helper_;
  scoped_refptr<MockBrowsingDataSharedWorkerHelper>
      mock_browsing_data_shared_worker_helper_;
  scoped_refptr<MockBrowsingDataCacheStorageHelper>
      mock_browsing_data_cache_storage_helper_;
  scoped_refptr<MockBrowsingDataFlashLSOHelper>
      mock_browsing_data_flash_lso_helper_;
  scoped_refptr<MockBrowsingDataMediaLicenseHelper>
      mock_browsing_data_media_license_helper_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  scoped_refptr<ExtensionSpecialStoragePolicy> special_storage_policy_;
#endif
};

TEST_F(CookiesTreeModelTest, RemoveAll) {
  std::unique_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());

  // Reset the selection of the first row.
  {
    SCOPED_TRACE("Before removing");
    EXPECT_EQ("A,B,C",
              GetDisplayedCookies(cookies_model.get()));
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2",
              GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(
        "https://sharedworkerhost1:1/app/worker.js,"
        "https://sharedworkerhost2:2/worker.js",
        GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("xyz.com",
              GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("https://media1/,https://media2/",
              GetDisplayedMediaLicenses(cookies_model.get()));
  }

  mock_browsing_data_cookie_helper_->Reset();
  mock_browsing_data_database_helper_->Reset();
  mock_browsing_data_local_storage_helper_->Reset();
  mock_browsing_data_session_storage_helper_->Reset();
  mock_browsing_data_indexed_db_helper_->Reset();
  mock_browsing_data_service_worker_helper_->Reset();
  mock_browsing_data_shared_worker_helper_->Reset();
  mock_browsing_data_cache_storage_helper_->Reset();
  mock_browsing_data_file_system_helper_->Reset();

  cookies_model->DeleteAllStoredObjects();

  // Make sure the nodes are also deleted from the model's cache.
  // http://crbug.com/43249
  cookies_model->UpdateSearchResults(base::string16());

  {
    // 2 nodes - root and app
    SCOPED_TRACE("After removing");
    EXPECT_EQ(1, cookies_model->GetRoot()->GetTotalNodeCount());
    EXPECT_EQ(0u, cookies_model->GetRoot()->children().size());
    EXPECT_EQ(std::string(), GetDisplayedCookies(cookies_model.get()));
    EXPECT_TRUE(mock_browsing_data_cookie_helper_->AllDeleted());
    EXPECT_TRUE(mock_browsing_data_database_helper_->AllDeleted());
    EXPECT_TRUE(mock_browsing_data_local_storage_helper_->AllDeleted());
    EXPECT_FALSE(mock_browsing_data_session_storage_helper_->AllDeleted());
    EXPECT_TRUE(mock_browsing_data_indexed_db_helper_->AllDeleted());
    EXPECT_TRUE(mock_browsing_data_file_system_helper_->AllDeleted());
    EXPECT_TRUE(mock_browsing_data_service_worker_helper_->AllDeleted());
    EXPECT_TRUE(mock_browsing_data_shared_worker_helper_->AllDeleted());
    EXPECT_TRUE(mock_browsing_data_cache_storage_helper_->AllDeleted());
    EXPECT_TRUE(mock_browsing_data_flash_lso_helper_->AllDeleted());
    EXPECT_TRUE(mock_browsing_data_media_license_helper_->AllDeleted());
  }
}

TEST_F(CookiesTreeModelTest, Remove) {
  std::unique_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());

  // Children start out arranged as follows:
  //
  // 0. `cshost1`
  // 1. `cshost2`
  // 2. `foo1`
  // 3. `foo2`
  // 4. `foo3`
  // 5. `fshost1`
  // 6. `fshost2`
  // 7. `fshost3`
  // 8. `gdbhost1`
  // 9. `gdbhost2`
  // 10. `host1`
  // 11. `host2`
  // 12. `idbhost1`
  // 13. `idbhost2`
  // 14. `media1`
  // 15. `media2`
  // 16. `quotahost1`
  // 17. `quotahost2`
  // 18. `sharedworkerhost1`
  // 19. `sharedworkerhost2`
  // 20. `swhost1`
  // 21. `swhost2`
  // 22. `xyz.com`
  //
  // Here, we'll remove them one by one, starting from the end, and
  // check that the state makes sense. Initially there are 71 total nodes.

  // xyz.com -> flash_lsos (2 nodes)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[22].get());
  {
    SCOPED_TRACE("`xyz.com` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2",
              GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(
        "https://sharedworkerhost1:1/app/worker.js,"
        "https://sharedworkerhost2:2/worker.js",
        GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("https://media1/,https://media2/",
              GetDisplayedMediaLicenses(cookies_model.get()));
    EXPECT_EQ(69, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // swhost2 -> service worker -> https://swhost1:2 (3 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[21].get());
  {
    SCOPED_TRACE("`swhost2` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("https://swhost1:1/",
              GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(
        "https://sharedworkerhost1:1/app/worker.js,"
        "https://sharedworkerhost2:2/worker.js",
        GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("https://media1/,https://media2/",
              GetDisplayedMediaLicenses(cookies_model.get()));
    EXPECT_EQ(66, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // swhost1 -> service worker -> https://swhost1:1 (3 nodes)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[20].get());
  {
    SCOPED_TRACE("`swhost1` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(
        "https://sharedworkerhost1:1/app/worker.js,"
        "https://sharedworkerhost2:2/worker.js",
        GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("https://media1/,https://media2/",
              GetDisplayedMediaLicenses(cookies_model.get()));
    EXPECT_EQ(63, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // sharedworkerhost2 -> shared worker -> https://sharedworkerhost2:2 (3
  // objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[19].get());
  {
    SCOPED_TRACE("`sharedworkerhost2` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("https://sharedworkerhost1:1/app/worker.js",
              GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("https://media1/,https://media2/",
              GetDisplayedMediaLicenses(cookies_model.get()));
    EXPECT_EQ(60, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // sharedworkerhost1 -> shared worker -> https://sharedworkerhost1:1 (3 nodes)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[18].get());
  {
    SCOPED_TRACE("`sharedworkerhost1` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("https://media1/,https://media2/",
              GetDisplayedMediaLicenses(cookies_model.get()));
    EXPECT_EQ(57, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // quotahost2 -> quotahost2 (2 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[17].get());
  {
    SCOPED_TRACE("`quotahost2` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("quotahost1",
              GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("https://media1/,https://media2/",
              GetDisplayedMediaLicenses(cookies_model.get()));
    EXPECT_EQ(55, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // quotahost1 -> quotahost1 (2 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[16].get());
  {
    SCOPED_TRACE("`quotahost1` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("https://media1/,https://media2/",
              GetDisplayedMediaLicenses(cookies_model.get()));
    EXPECT_EQ(53, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // media2 -> media_licenses -> media_license (3 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[15].get());
  {
    SCOPED_TRACE("`media2` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("https://media1/",
              GetDisplayedMediaLicenses(cookies_model.get()));
    EXPECT_EQ(50, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // media1 -> media_licenses -> media_license (3 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[14].get());
  {
    SCOPED_TRACE("`media1` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedMediaLicenses(cookies_model.get()));
    EXPECT_EQ(47, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // idbhost2 -> indexeddb -> http://idbhost2:2/ (3 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[13].get());
  {
    SCOPED_TRACE("`idbhost2` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedMediaLicenses(cookies_model.get()));
    EXPECT_EQ(44, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // idbhost1 -> indexeddb -> http://idbhost1:1/ (3 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[12].get());
  {
    SCOPED_TRACE("`idbhost1` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedMediaLicenses(cookies_model.get()));
    EXPECT_EQ(41, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // host2 -> localstorage -> http://host2:2/,
  //       -> sessionstorage -> http://host2:2/ (5 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[11].get());
  {
    SCOPED_TRACE("`host2` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedMediaLicenses(cookies_model.get()));
    EXPECT_EQ(36, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // host1 -> localstorage -> http://host1:1/,
  //       -> sessionstorage -> http://host1:1/ (5 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[10].get());
  {
    SCOPED_TRACE("`host1` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedMediaLicenses(cookies_model.get()));
    EXPECT_EQ(31, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // gdbhost2 -> database -> http://gdbhost2:2/ (3 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[9].get());
  {
    SCOPED_TRACE("`gdbhost2` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost1:1/", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedMediaLicenses(cookies_model.get()));
    EXPECT_EQ(28, cookies_model->GetRoot()->GetTotalNodeCount());
  }
  // gdbhost1 -> database -> http://gdbhost1:1/ (3 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[8].get());
  {
    SCOPED_TRACE("`gdbhost1` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedMediaLicenses(cookies_model.get()));
    EXPECT_EQ(25, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // fshost3 -> filesystem -> http://fshost3:1/ (3 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[7].get());
  {
    SCOPED_TRACE("`fshost3` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedMediaLicenses(cookies_model.get()));
    EXPECT_EQ(22, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // fshost2 -> filesystem -> http://fshost2:1/ (3 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[6].get());
  {
    SCOPED_TRACE("`fshost2` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedMediaLicenses(cookies_model.get()));
    EXPECT_EQ(19, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // fshost1 -> filesystem -> http://fshost1:1/ (3 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[5].get());
  {
    SCOPED_TRACE("`fshost1` removed.");
    EXPECT_STREQ("A,B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedMediaLicenses(cookies_model.get()));
    EXPECT_EQ(16, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // foo3 -> cookies -> c (3 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[4].get());
  {
    SCOPED_TRACE("`foo3` removed.");
    EXPECT_STREQ("A,B", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedMediaLicenses(cookies_model.get()));
    EXPECT_EQ(13, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // foo2 -> cookies -> b (3 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[3].get());
  {
    SCOPED_TRACE("`foo2` removed.");
    EXPECT_STREQ("A", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedMediaLicenses(cookies_model.get()));
    EXPECT_EQ(10, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // foo1 -> cookies -> a (3 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[2].get());
  {
    SCOPED_TRACE("`foo1` removed.");
    EXPECT_STREQ("", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedMediaLicenses(cookies_model.get()));
    EXPECT_EQ(7, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // cshost2 -> cache storage -> https://cshost2:2/ (3 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[1].get());
  {
    SCOPED_TRACE("`cshost2` removed.");
    EXPECT_STREQ("", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedMediaLicenses(cookies_model.get()));
    EXPECT_EQ(4, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  // cshost1 -> cache storage -> https://cshost1:1/ (3 objects)
  DeleteStoredObjects(cookies_model->GetRoot()->children()[0].get());
  {
    SCOPED_TRACE("`cshost1` removed.");
    EXPECT_STREQ("", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedMediaLicenses(cookies_model.get()));
    EXPECT_EQ(1, cookies_model->GetRoot()->GetTotalNodeCount());
  }
}

TEST_F(CookiesTreeModelTest, RemoveCookiesNode) {
  std::unique_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());

  DeleteStoredObjects(
      cookies_model->GetRoot()->children()[2]->children()[0].get());
  {
    SCOPED_TRACE("First cookies origin removed");
    EXPECT_STREQ("B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    // 69 because in this case, the origin remains, although the COOKIES
    // node beneath it has been deleted.
    EXPECT_EQ(69, cookies_model->GetRoot()->GetTotalNodeCount());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(
        "https://sharedworkerhost1:1/app/worker.js,"
        "https://sharedworkerhost2:2/worker.js",
        GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("xyz.com", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("https://media1/,https://media2/",
              GetDisplayedMediaLicenses(cookies_model.get()));
  }

  DeleteStoredObjects(
      cookies_model->GetRoot()->children()[8]->children()[0].get());
  {
    SCOPED_TRACE("First database origin removed");
    EXPECT_STREQ("B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost2:2/", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(
        "https://sharedworkerhost1:1/app/worker.js,"
        "https://sharedworkerhost2:2/worker.js",
        GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("xyz.com", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("https://media1/,https://media2/",
              GetDisplayedMediaLicenses(cookies_model.get()));
    EXPECT_EQ(67, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  DeleteStoredObjects(
      cookies_model->GetRoot()->children()[10]->children()[0].get());
  {
    SCOPED_TRACE("First local storage origin removed");
    EXPECT_STREQ("B,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost2:2/", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(
        "https://sharedworkerhost1:1/app/worker.js,"
        "https://sharedworkerhost2:2/worker.js",
        GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("xyz.com", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("https://media1/,https://media2/",
              GetDisplayedMediaLicenses(cookies_model.get()));
    EXPECT_EQ(65, cookies_model->GetRoot()->GetTotalNodeCount());
  }
}

TEST_F(CookiesTreeModelTest, RemoveCookieNode) {
  std::unique_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());

  DeleteStoredObjects(
      cookies_model->GetRoot()->children()[3]->children()[0].get());
  {
    SCOPED_TRACE("Second origin COOKIES node removed");
    EXPECT_STREQ("A,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(
        "https://sharedworkerhost1:1/app/worker.js,"
        "https://sharedworkerhost2:2/worker.js",
        GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("xyz.com", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("https://media1/,https://media2/",
              GetDisplayedMediaLicenses(cookies_model.get()));
    // 69 because in this case, the origin remains, although the COOKIES
    // node beneath it has been deleted.
    EXPECT_EQ(69, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  DeleteStoredObjects(
      cookies_model->GetRoot()->children()[8]->children()[0].get());
  {
    SCOPED_TRACE("First database origin removed");
    EXPECT_STREQ("A,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost2:2/", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(
        "https://sharedworkerhost1:1/app/worker.js,"
        "https://sharedworkerhost2:2/worker.js",
        GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("xyz.com", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("https://media1/,https://media2/",
              GetDisplayedMediaLicenses(cookies_model.get()));
    EXPECT_EQ(67, cookies_model->GetRoot()->GetTotalNodeCount());
  }

  DeleteStoredObjects(
      cookies_model->GetRoot()->children()[10]->children()[0].get());
  {
    SCOPED_TRACE("First local storage origin removed");
    EXPECT_STREQ("A,C", GetDisplayedCookies(cookies_model.get()).c_str());
    EXPECT_EQ("http://gdbhost2:2/", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(
        "https://sharedworkerhost1:1/app/worker.js,"
        "https://sharedworkerhost2:2/worker.js",
        GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("xyz.com", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("https://media1/,https://media2/",
              GetDisplayedMediaLicenses(cookies_model.get()));
    EXPECT_EQ(65, cookies_model->GetRoot()->GetTotalNodeCount());
  }
}

TEST_F(CookiesTreeModelTest, RemoveSingleCookieNode) {
  auto container = std::make_unique<LocalDataContainer>(
      mock_browsing_data_cookie_helper_, mock_browsing_data_database_helper_,
      mock_browsing_data_local_storage_helper_,
      mock_browsing_data_session_storage_helper_,
      mock_browsing_data_appcache_helper_,
      mock_browsing_data_indexed_db_helper_,
      mock_browsing_data_file_system_helper_, mock_browsing_data_quota_helper_,
      mock_browsing_data_service_worker_helper_,
      mock_browsing_data_shared_worker_helper_,
      mock_browsing_data_cache_storage_helper_,
      mock_browsing_data_flash_lso_helper_,
      mock_browsing_data_media_license_helper_);
  CookiesTreeModel cookies_model(std::move(container),
                                 special_storage_policy());

  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo1"), "A=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo2"), "B=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "C=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "D=1");
  mock_browsing_data_cookie_helper_->Notify();
  mock_browsing_data_database_helper_->AddDatabaseSamples();
  mock_browsing_data_database_helper_->Notify();
  mock_browsing_data_local_storage_helper_->AddLocalStorageSamples();
  mock_browsing_data_local_storage_helper_->Notify();
  mock_browsing_data_session_storage_helper_->AddLocalStorageSamples();
  mock_browsing_data_session_storage_helper_->Notify();
  mock_browsing_data_indexed_db_helper_->AddIndexedDBSamples();
  mock_browsing_data_indexed_db_helper_->Notify();
  mock_browsing_data_file_system_helper_->AddFileSystemSamples();
  mock_browsing_data_file_system_helper_->Notify();
  mock_browsing_data_quota_helper_->AddQuotaSamples();
  mock_browsing_data_quota_helper_->Notify();
  mock_browsing_data_service_worker_helper_->AddServiceWorkerSamples();
  mock_browsing_data_service_worker_helper_->Notify();
  mock_browsing_data_shared_worker_helper_->AddSharedWorkerSamples();
  mock_browsing_data_shared_worker_helper_->Notify();
  mock_browsing_data_cache_storage_helper_->AddCacheStorageSamples();
  mock_browsing_data_cache_storage_helper_->Notify();

  {
    SCOPED_TRACE(
        "Initial State 4 cookies, 2 databases, 2 local storages, "
        "2 session storages, 2 indexed DBs, 3 file systems, "
        "2 quotas, 2 service workers, 2 shared workers, 2 caches.");
    // 58 because there's the root, then
    // cshost1 -> cache storage -> https://cshost1:1/
    // cshost2 -> cache storage -> https://cshost2:2/
    // foo1 -> cookies -> a,
    // foo2 -> cookies -> b,
    // foo3 -> cookies -> c,d
    // dbhost1 -> database -> http://gdbhost1:1/,
    // dbhost2 -> database -> http://gdbhost2:2/,
    // host1 -> localstorage -> http://host1:1/,
    //       -> sessionstorage -> http://host1:1/,
    // host2 -> localstorage -> http://host2:2/,
    //       -> sessionstorage -> http://host2:2/,
    // idbhost1 -> sessionstorage -> http://idbhost1:1/,
    // idbhost2 -> sessionstorage -> http://idbhost2:2/,
    // fshost1 -> filesystem -> http://fshost1:1/,
    // fshost2 -> filesystem -> http://fshost2:1/,
    // fshost3 -> filesystem -> http://fshost3:1/,
    // quotahost1 -> quotahost1,
    // quotahost2 -> quotahost2.
    // swhost1 -> service worker -> https://swhost1:1
    // swhost2 -> service worker -> https://swhost1:2
    // sharedworkerhost1 -> shared worker -> https://sharedworkerhost1:1
    // sharedworkerhost2 -> shared worker -> https://sharedworkerhost2:2
    EXPECT_EQ(64, cookies_model.GetRoot()->GetTotalNodeCount());
    EXPECT_STREQ("A,B,C,D", GetDisplayedCookies(&cookies_model).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(&cookies_model));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(&cookies_model));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(&cookies_model));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(&cookies_model));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(&cookies_model));
    EXPECT_EQ(
        "https://sharedworkerhost1:1/app/worker.js,"
        "https://sharedworkerhost2:2/worker.js",
        GetDisplayedSharedWorkers(&cookies_model));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(&cookies_model));
  }
  DeleteStoredObjects(cookies_model.GetRoot()->children()[4].get());
  {
    SCOPED_TRACE("Third cookie origin removed");
    EXPECT_STREQ("A,B", GetDisplayedCookies(&cookies_model).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(&cookies_model));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(&cookies_model));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(&cookies_model));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(&cookies_model));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(&cookies_model));
    EXPECT_EQ(
        "https://sharedworkerhost1:1/app/worker.js,"
        "https://sharedworkerhost2:2/worker.js",
        GetDisplayedSharedWorkers(&cookies_model));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(&cookies_model));
    EXPECT_EQ(60, cookies_model.GetRoot()->GetTotalNodeCount());
  }
}

TEST_F(CookiesTreeModelTest, RemoveSingleCookieNodeOf3) {
  auto container = std::make_unique<LocalDataContainer>(
      mock_browsing_data_cookie_helper_, mock_browsing_data_database_helper_,
      mock_browsing_data_local_storage_helper_,
      mock_browsing_data_session_storage_helper_,
      mock_browsing_data_appcache_helper_,
      mock_browsing_data_indexed_db_helper_,
      mock_browsing_data_file_system_helper_, mock_browsing_data_quota_helper_,
      mock_browsing_data_service_worker_helper_,
      mock_browsing_data_shared_worker_helper_,
      mock_browsing_data_cache_storage_helper_,
      mock_browsing_data_flash_lso_helper_,
      mock_browsing_data_media_license_helper_);
  CookiesTreeModel cookies_model(std::move(container),
                                 special_storage_policy());

  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo1"), "A=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo2"), "B=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "C=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "D=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "E=1");
  mock_browsing_data_cookie_helper_->Notify();
  mock_browsing_data_database_helper_->AddDatabaseSamples();
  mock_browsing_data_database_helper_->Notify();
  mock_browsing_data_local_storage_helper_->AddLocalStorageSamples();
  mock_browsing_data_local_storage_helper_->Notify();
  mock_browsing_data_session_storage_helper_->AddLocalStorageSamples();
  mock_browsing_data_session_storage_helper_->Notify();
  mock_browsing_data_indexed_db_helper_->AddIndexedDBSamples();
  mock_browsing_data_indexed_db_helper_->Notify();
  mock_browsing_data_file_system_helper_->AddFileSystemSamples();
  mock_browsing_data_file_system_helper_->Notify();
  mock_browsing_data_quota_helper_->AddQuotaSamples();
  mock_browsing_data_quota_helper_->Notify();
  mock_browsing_data_service_worker_helper_->AddServiceWorkerSamples();
  mock_browsing_data_service_worker_helper_->Notify();
  mock_browsing_data_shared_worker_helper_->AddSharedWorkerSamples();
  mock_browsing_data_shared_worker_helper_->Notify();
  mock_browsing_data_cache_storage_helper_->AddCacheStorageSamples();
  mock_browsing_data_cache_storage_helper_->Notify();

  {
    SCOPED_TRACE(
        "Initial State 5 cookies, 2 databases, 2 local storages, "
        "2 session storages, 2 indexed DBs, 3 filesystems, "
        "2 quotas, 2 service workers, 2 shared workers, 2 caches.");
    // 59 because there's the root, then
    // cshost1 -> cache storage -> https://cshost1:1/
    // cshost2 -> cache storage -> https://cshost2:2/
    // foo1 -> cookies -> a,
    // foo2 -> cookies -> b,
    // foo3 -> cookies -> c,d,e
    // dbhost1 -> database -> http://gdbhost1:1/,
    // dbhost2 -> database -> http://gdbhost2:2/,
    // host1 -> localstorage -> http://host1:1/,
    //       -> sessionstorage -> http://host1:1/,
    // host2 -> localstorage -> http://host2:2/,
    //       -> sessionstorage -> http://host2:2/,
    // idbhost1 -> sessionstorage -> http://idbhost1:1/,
    // idbhost2 -> sessionstorage -> http://idbhost2:2/,
    // fshost1 -> filesystem -> http://fshost1:1/,
    // fshost2 -> filesystem -> http://fshost2:1/,
    // fshost3 -> filesystem -> http://fshost3:1/,
    // quotahost1 -> quotahost1,
    // quotahost2 -> quotahost2.
    // swhost1 -> service worker -> https://swhost1:1
    // swhost2 -> service worker -> https://swhost1:2
    // sharedworkerhost1 -> shared worker -> https://sharedworkerhost1:1
    // sharedworkerhost2 -> shared worker -> https://sharedworkerhost2:2
    EXPECT_EQ(65, cookies_model.GetRoot()->GetTotalNodeCount());
    EXPECT_STREQ("A,B,C,D,E", GetDisplayedCookies(&cookies_model).c_str());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(&cookies_model));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(&cookies_model));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(&cookies_model));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(&cookies_model));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(&cookies_model));
    EXPECT_EQ(
        "https://sharedworkerhost1:1/app/worker.js,"
        "https://sharedworkerhost2:2/worker.js",
        GetDisplayedSharedWorkers(&cookies_model));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(&cookies_model));
  }
  DeleteStoredObjects(cookies_model.GetRoot()
                          ->children()[4]
                          ->children()[0]
                          ->children()[1]
                          .get());
  {
    SCOPED_TRACE("Middle cookie in third cookie origin removed");
    EXPECT_STREQ("A,B,C,E", GetDisplayedCookies(&cookies_model).c_str());
    EXPECT_EQ(64, cookies_model.GetRoot()->GetTotalNodeCount());
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(&cookies_model));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(&cookies_model));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(&cookies_model));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(&cookies_model));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(&cookies_model));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(&cookies_model));
    EXPECT_EQ(
        "https://sharedworkerhost1:1/app/worker.js,"
        "https://sharedworkerhost2:2/worker.js",
        GetDisplayedSharedWorkers(&cookies_model));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(&cookies_model));
  }
}

TEST_F(CookiesTreeModelTest, RemoveSecondOrigin) {
  auto container = std::make_unique<LocalDataContainer>(
      mock_browsing_data_cookie_helper_, mock_browsing_data_database_helper_,
      mock_browsing_data_local_storage_helper_,
      mock_browsing_data_session_storage_helper_,
      mock_browsing_data_appcache_helper_,
      mock_browsing_data_indexed_db_helper_,
      mock_browsing_data_file_system_helper_, mock_browsing_data_quota_helper_,
      mock_browsing_data_service_worker_helper_,
      mock_browsing_data_shared_worker_helper_,
      mock_browsing_data_cache_storage_helper_,
      mock_browsing_data_flash_lso_helper_,
      mock_browsing_data_media_license_helper_);
  CookiesTreeModel cookies_model(std::move(container),
                                 special_storage_policy());

  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo1"), "A=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo2"), "B=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "C=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "D=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3"), "E=1");
  mock_browsing_data_cookie_helper_->Notify();

  {
    SCOPED_TRACE("Initial State 5 cookies");
    // 12 because there's the root, then foo1 -> cookies -> a,
    // foo2 -> cookies -> b, foo3 -> cookies -> c,d,e
    EXPECT_EQ(12, cookies_model.GetRoot()->GetTotalNodeCount());
    EXPECT_STREQ("A,B,C,D,E", GetDisplayedCookies(&cookies_model).c_str());
  }
  DeleteStoredObjects(cookies_model.GetRoot()->children()[1].get());
  {
    SCOPED_TRACE("Second origin removed");
    EXPECT_STREQ("A,C,D,E", GetDisplayedCookies(&cookies_model).c_str());
    // Left with root -> foo1 -> cookies -> a, foo3 -> cookies -> c,d,e
    EXPECT_EQ(9, cookies_model.GetRoot()->GetTotalNodeCount());
  }
}

TEST_F(CookiesTreeModelTest, OriginOrdering) {
  auto container = std::make_unique<LocalDataContainer>(
      mock_browsing_data_cookie_helper_, mock_browsing_data_database_helper_,
      mock_browsing_data_local_storage_helper_,
      mock_browsing_data_session_storage_helper_,
      mock_browsing_data_appcache_helper_,
      mock_browsing_data_indexed_db_helper_,
      mock_browsing_data_file_system_helper_, mock_browsing_data_quota_helper_,
      mock_browsing_data_service_worker_helper_,
      mock_browsing_data_shared_worker_helper_,
      mock_browsing_data_cache_storage_helper_,
      mock_browsing_data_flash_lso_helper_,
      mock_browsing_data_media_license_helper_);
  CookiesTreeModel cookies_model(std::move(container),
                                 special_storage_policy());

  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://a.foo2.com"), "A=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo2.com"), "B=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://b.foo1.com"), "C=1");
  // Leading dot on the foo4
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://foo4.com"), "D=1; domain=.foo4.com; path=/;");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://a.foo1.com"), "E=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo1.com"), "F=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3.com"), "G=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo4.com"), "H=1");
  mock_browsing_data_cookie_helper_->Notify();

  {
    SCOPED_TRACE("Initial State 8 cookies");
    EXPECT_EQ(23, cookies_model.GetRoot()->GetTotalNodeCount());
    EXPECT_STREQ("F,E,C,B,A,G,D,H",
        GetDisplayedCookies(&cookies_model).c_str());
  }
  // Delete "E"
  DeleteStoredObjects(cookies_model.GetRoot()->children()[1].get());
  {
    EXPECT_STREQ("F,C,B,A,G,D,H", GetDisplayedCookies(&cookies_model).c_str());
  }
}

TEST_F(CookiesTreeModelTest, ContentSettings) {
  GURL host("http://xyz.com/");
  auto container = std::make_unique<LocalDataContainer>(
      mock_browsing_data_cookie_helper_, mock_browsing_data_database_helper_,
      mock_browsing_data_local_storage_helper_,
      mock_browsing_data_session_storage_helper_,
      mock_browsing_data_appcache_helper_,
      mock_browsing_data_indexed_db_helper_,
      mock_browsing_data_file_system_helper_, mock_browsing_data_quota_helper_,
      mock_browsing_data_service_worker_helper_,
      mock_browsing_data_shared_worker_helper_,
      mock_browsing_data_cache_storage_helper_,
      mock_browsing_data_flash_lso_helper_,
      mock_browsing_data_media_license_helper_);
  CookiesTreeModel cookies_model(std::move(container),
                                 special_storage_policy());

  mock_browsing_data_cookie_helper_->AddCookieSamples(host, "A=1");
  mock_browsing_data_cookie_helper_->Notify();

  TestingProfile profile;
  HostContentSettingsMap* content_settings =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  content_settings::CookieSettings* cookie_settings =
      CookieSettingsFactory::GetForProfile(&profile).get();
  MockSettingsObserver observer(content_settings);

  CookieTreeRootNode* root =
      static_cast<CookieTreeRootNode*>(cookies_model.GetRoot());
  CookieTreeHostNode* origin =
      root->GetOrCreateHostNode(host);

  EXPECT_EQ(1u, origin->children().size());
  EXPECT_TRUE(origin->CanCreateContentException());
  EXPECT_CALL(observer, OnContentSettingsChanged(
                            content_settings, ContentSettingsType::COOKIES,
                            false, ContentSettingsPattern::FromURL(host),
                            ContentSettingsPattern::Wildcard(), false))
      .Times(2);
  origin->CreateContentException(
      cookie_settings, CONTENT_SETTING_SESSION_ONLY);
  EXPECT_TRUE(cookie_settings->IsCookieAccessAllowed(host, host));
  EXPECT_TRUE(cookie_settings->IsCookieSessionOnly(host));
}

TEST_F(CookiesTreeModelTest, FileSystemFilter) {
  std::unique_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());

  cookies_model->UpdateSearchResults(base::ASCIIToUTF16("fshost1"));
  EXPECT_EQ("http://fshost1:1/",
            GetDisplayedFileSystems(cookies_model.get()));

  cookies_model->UpdateSearchResults(base::ASCIIToUTF16("fshost2"));
  EXPECT_EQ("http://fshost2:2/",
            GetDisplayedFileSystems(cookies_model.get()));

  cookies_model->UpdateSearchResults(base::ASCIIToUTF16("fshost3"));
  EXPECT_EQ("http://fshost3:3/",
            GetDisplayedFileSystems(cookies_model.get()));

  cookies_model->UpdateSearchResults(base::string16());
  EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
            GetDisplayedFileSystems(cookies_model.get()));
}

TEST_F(CookiesTreeModelTest, ServiceWorkerFilter) {
  std::unique_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());

  cookies_model->UpdateSearchResults(base::ASCIIToUTF16("swhost1"));
  EXPECT_EQ("https://swhost1:1/",
            GetDisplayedServiceWorkers(cookies_model.get()));

  cookies_model->UpdateSearchResults(base::ASCIIToUTF16("swhost2"));
  EXPECT_EQ("https://swhost2:2/",
            GetDisplayedServiceWorkers(cookies_model.get()));

  cookies_model->UpdateSearchResults(base::ASCIIToUTF16("swhost3"));
  EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));

  cookies_model->UpdateSearchResults(base::string16());
  EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
            GetDisplayedServiceWorkers(cookies_model.get()));
}

TEST_F(CookiesTreeModelTest, SharedWorkerFilter) {
  std::unique_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());

  cookies_model->UpdateSearchResults(base::ASCIIToUTF16("sharedworkerhost1"));
  EXPECT_EQ("https://sharedworkerhost1:1/app/worker.js",
            GetDisplayedSharedWorkers(cookies_model.get()));

  cookies_model->UpdateSearchResults(base::ASCIIToUTF16("sharedworkerhost2"));
  EXPECT_EQ("https://sharedworkerhost2:2/worker.js",
            GetDisplayedSharedWorkers(cookies_model.get()));

  cookies_model->UpdateSearchResults(base::ASCIIToUTF16("sharedworkerhost3"));
  EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));

  cookies_model->UpdateSearchResults(base::string16());
  EXPECT_EQ(
      "https://sharedworkerhost1:1/app/worker.js,"
      "https://sharedworkerhost2:2/worker.js",
      GetDisplayedSharedWorkers(cookies_model.get()));
}

TEST_F(CookiesTreeModelTest, CacheStorageFilter) {
  std::unique_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());

  cookies_model->UpdateSearchResults(base::ASCIIToUTF16("cshost1"));
  EXPECT_EQ("https://cshost1:1/",
            GetDisplayedCacheStorages(cookies_model.get()));

  cookies_model->UpdateSearchResults(base::ASCIIToUTF16("cshost2"));
  EXPECT_EQ("https://cshost2:2/",
            GetDisplayedCacheStorages(cookies_model.get()));

  cookies_model->UpdateSearchResults(base::ASCIIToUTF16("cshost3"));
  EXPECT_EQ("", GetDisplayedCacheStorages(cookies_model.get()));

  cookies_model->UpdateSearchResults(base::string16());
  EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
            GetDisplayedCacheStorages(cookies_model.get()));
}

TEST_F(CookiesTreeModelTest, CookiesFilter) {
  auto container = std::make_unique<LocalDataContainer>(
      mock_browsing_data_cookie_helper_, mock_browsing_data_database_helper_,
      mock_browsing_data_local_storage_helper_,
      mock_browsing_data_session_storage_helper_,
      mock_browsing_data_appcache_helper_,
      mock_browsing_data_indexed_db_helper_,
      mock_browsing_data_file_system_helper_, mock_browsing_data_quota_helper_,
      mock_browsing_data_service_worker_helper_,
      mock_browsing_data_shared_worker_helper_,
      mock_browsing_data_cache_storage_helper_,
      mock_browsing_data_flash_lso_helper_,
      mock_browsing_data_media_license_helper_);
  CookiesTreeModel cookies_model(std::move(container),
                                 special_storage_policy());

  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://123.com"), "A=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo1.com"), "B=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo2.com"), "C=1");
  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL("http://foo3.com"), "D=1");
  mock_browsing_data_cookie_helper_->Notify();
  EXPECT_EQ("A,B,C,D", GetDisplayedCookies(&cookies_model));

  cookies_model.UpdateSearchResults(base::string16(base::ASCIIToUTF16("foo")));
  EXPECT_EQ("B,C,D", GetDisplayedCookies(&cookies_model));

  cookies_model.UpdateSearchResults(base::string16(base::ASCIIToUTF16("2")));
  EXPECT_EQ("A,C", GetDisplayedCookies(&cookies_model));

  cookies_model.UpdateSearchResults(base::string16(base::ASCIIToUTF16("foo3")));
  EXPECT_EQ("D", GetDisplayedCookies(&cookies_model));

  cookies_model.UpdateSearchResults(base::string16());
  EXPECT_EQ("A,B,C,D", GetDisplayedCookies(&cookies_model));
}

// Tests that cookie source URLs are stored correctly in the cookies
// tree model.
TEST_F(CookiesTreeModelTest, CanonicalizeCookieSource) {
  auto container = std::make_unique<LocalDataContainer>(
      mock_browsing_data_cookie_helper_, mock_browsing_data_database_helper_,
      mock_browsing_data_local_storage_helper_,
      mock_browsing_data_session_storage_helper_,
      mock_browsing_data_appcache_helper_,
      mock_browsing_data_indexed_db_helper_,
      mock_browsing_data_file_system_helper_, mock_browsing_data_quota_helper_,
      mock_browsing_data_service_worker_helper_,
      mock_browsing_data_shared_worker_helper_,
      mock_browsing_data_cache_storage_helper_,
      mock_browsing_data_flash_lso_helper_,
      mock_browsing_data_media_license_helper_);
  CookiesTreeModel cookies_model(std::move(container),
                                 special_storage_policy());

  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("file:///tmp/test.html"), "A=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://example.com"), "B=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://example.com/"), "C=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://example.com/test"), "D=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://example.com:1234/"), "E=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("https://example.com/"), "F=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://user:pwd@example.com/"), "G=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://example.com/test?foo"), "H=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://example.com/test#foo"), "I=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("https://example2.com/test#foo"), "J=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://example3.com:1234/test#foo"), "K=1");
  mock_browsing_data_cookie_helper_->AddCookieSamples(
      GURL("http://user:pwd@example4.com/test?foo"), "L=1");
  mock_browsing_data_cookie_helper_->Notify();

  // Check that all the above example.com cookies go on the example.com
  // host node.
  cookies_model.UpdateSearchResults(
      base::string16(base::ASCIIToUTF16("example.com")));
  EXPECT_EQ("B,C,D,E,F,G,H,I", GetDisplayedCookies(&cookies_model));

  TestingProfile profile;
  content_settings::CookieSettings* cookie_settings =
      CookieSettingsFactory::GetForProfile(&profile).get();

  // Check that content settings for different URLs get applied to the
  // correct URL. That is, setting a cookie on https://example2.com
  // should create a host node for http://example2.com and thus content
  // settings set on that host node should apply to http://example2.com.

  cookies_model.UpdateSearchResults(
      base::string16(base::ASCIIToUTF16("file://")));
  EXPECT_EQ("", GetDisplayedCookies(&cookies_model));
  CheckContentSettingsUrlForHostNodes(
      cookies_model.GetRoot(), CookieTreeNode::DetailedInfo::TYPE_ROOT,
      cookie_settings, GURL("file:///test/tmp.html"));

  cookies_model.UpdateSearchResults(
      base::string16(base::ASCIIToUTF16("example2.com")));
  EXPECT_EQ("J", GetDisplayedCookies(&cookies_model));
  CheckContentSettingsUrlForHostNodes(
      cookies_model.GetRoot(), CookieTreeNode::DetailedInfo::TYPE_ROOT,
      cookie_settings, GURL("http://example2.com"));

  cookies_model.UpdateSearchResults(
      base::string16(base::ASCIIToUTF16("example3.com")));
  EXPECT_EQ("K", GetDisplayedCookies(&cookies_model));
  CheckContentSettingsUrlForHostNodes(
      cookies_model.GetRoot(), CookieTreeNode::DetailedInfo::TYPE_ROOT,
      cookie_settings, GURL("http://example3.com"));

  cookies_model.UpdateSearchResults(
      base::string16(base::ASCIIToUTF16("example4.com")));
  EXPECT_EQ("L", GetDisplayedCookies(&cookies_model));
  CheckContentSettingsUrlForHostNodes(
      cookies_model.GetRoot(), CookieTreeNode::DetailedInfo::TYPE_ROOT,
      cookie_settings, GURL("http://example4.com"));
}

TEST_F(CookiesTreeModelTest, CookiesFilterWithoutSource) {
  // CanonicalCookies don't persist their source_ field. This is a regression
  // test for crbug.com/601582.
  auto container = std::make_unique<LocalDataContainer>(
      mock_browsing_data_cookie_helper_, mock_browsing_data_database_helper_,
      mock_browsing_data_local_storage_helper_,
      mock_browsing_data_session_storage_helper_,
      mock_browsing_data_appcache_helper_,
      mock_browsing_data_indexed_db_helper_,
      mock_browsing_data_file_system_helper_, mock_browsing_data_quota_helper_,
      mock_browsing_data_service_worker_helper_,
      mock_browsing_data_shared_worker_helper_,
      mock_browsing_data_cache_storage_helper_,
      mock_browsing_data_flash_lso_helper_,
      mock_browsing_data_media_license_helper_);
  CookiesTreeModel cookies_model(std::move(container),
                                 special_storage_policy());

  mock_browsing_data_cookie_helper_->
      AddCookieSamples(GURL(), "A=1");
  mock_browsing_data_cookie_helper_->Notify();
  EXPECT_EQ("A", GetDisplayedCookies(&cookies_model));
}

TEST_F(CookiesTreeModelTest, FlashFilter) {
  std::unique_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());

  cookies_model->UpdateSearchResults(base::string16(base::ASCIIToUTF16("xyz")));
  {
    SCOPED_TRACE("Search for 'xyz'");
    EXPECT_EQ("", GetDisplayedCookies(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("xyz.com", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedMediaLicenses(cookies_model.get()));
  }

  // Search for any domain containing 'y'.
  cookies_model->UpdateSearchResults(base::string16(base::ASCIIToUTF16("y")));
  {
    SCOPED_TRACE("Search for 'y'");
    EXPECT_EQ("", GetDisplayedCookies(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("xyz.com", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedMediaLicenses(cookies_model.get()));
  }

  // Search for any domain containing 'abc' (which doesn't match anything).
  cookies_model->UpdateSearchResults(base::string16(base::ASCIIToUTF16("abc")));
  {
    SCOPED_TRACE("Search for 'abc'");
    EXPECT_EQ("", GetDisplayedCookies(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedMediaLicenses(cookies_model.get()));
  }
}

TEST_F(CookiesTreeModelTest, MediaLicensesFilter) {
  std::unique_ptr<CookiesTreeModel> cookies_model(
      CreateCookiesTreeModelWithInitialSample());

  cookies_model->UpdateSearchResults(
      base::string16(base::ASCIIToUTF16("media1")));
  {
    SCOPED_TRACE("Search for 'media1'");
    EXPECT_EQ("", GetDisplayedCookies(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("https://media1/",
              GetDisplayedMediaLicenses(cookies_model.get()));
  }

  cookies_model->UpdateSearchResults(
      base::string16(base::ASCIIToUTF16("media")));
  {
    SCOPED_TRACE("Search for 'media'");
    EXPECT_EQ("", GetDisplayedCookies(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("https://media1/,https://media2/",
              GetDisplayedMediaLicenses(cookies_model.get()));
  }

  // Search for everything.
  cookies_model->UpdateSearchResults(base::string16());
  {
    SCOPED_TRACE("Search for everything");
    EXPECT_EQ("A,B,C", GetDisplayedCookies(cookies_model.get()));
    EXPECT_EQ("http://gdbhost1:1/,http://gdbhost2:2/",
              GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/,http://host2:2/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/,http://fshost2:2/,http://fshost3:3/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("quotahost1,quotahost2", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/,https://cshost2:2/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("https://swhost1:1/,https://swhost2:2/",
              GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ(
        "https://sharedworkerhost1:1/app/worker.js,"
        "https://sharedworkerhost2:2/worker.js",
        GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("xyz.com", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("https://media1/,https://media2/",
              GetDisplayedMediaLicenses(cookies_model.get()));
  }

  // Search for any domain containing '1'.
  cookies_model->UpdateSearchResults(base::string16(base::ASCIIToUTF16("1")));
  {
    SCOPED_TRACE("Search for '1'");
    EXPECT_EQ("A", GetDisplayedCookies(cookies_model.get()));
    EXPECT_EQ("http://gdbhost1:1/", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("http://host1:1/",
              GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("http://host1:1/",
              GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("http://fshost1:1/",
              GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("quotahost1", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("https://cshost1:1/",
              GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("https://swhost1:1/",
              GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("https://sharedworkerhost1:1/app/worker.js",
              GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("https://media1/",
              GetDisplayedMediaLicenses(cookies_model.get()));
  }

  // Search for any domain containing 'i'.
  cookies_model->UpdateSearchResults(base::string16(base::ASCIIToUTF16("i")));
  {
    SCOPED_TRACE("Search for 'i'");
    EXPECT_EQ("", GetDisplayedCookies(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedDatabases(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedLocalStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSessionStorages(cookies_model.get()));
    EXPECT_EQ("http://idbhost1:1/,http://idbhost2:2/",
              GetDisplayedIndexedDBs(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFileSystems(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedQuotas(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedCacheStorages(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedServiceWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedSharedWorkers(cookies_model.get()));
    EXPECT_EQ("", GetDisplayedFlashLSOs(cookies_model.get()));
    EXPECT_EQ("https://media1/,https://media2/",
              GetDisplayedMediaLicenses(cookies_model.get()));
  }
}

}  // namespace
