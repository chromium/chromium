// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_flash_lso_helper.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/browsing_data/counters/cache_counter.h"
#include "chrome/browser/browsing_data/counters/site_data_counting_helper.h"
#include "chrome/browser/browsing_data/local_data_container.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "net/cookies/canonical_cookie.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_features.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#if defined(OS_MACOSX)
#include "base/threading/platform_thread.h"
#endif
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/browsing_data/browsing_data_media_license_helper.h"
#include "chrome/browser/media/library_cdm_test_helper.h"
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

using content::BrowserThread;
using content::BrowsingDataFilterBuilder;

namespace {
static const char* kExampleHost = "example.com";
static const char* kLocalHost = "localhost";
static const base::Time kLastHour =
    base::Time::Now() - base::TimeDelta::FromHours(1);

// Check if |file| matches any regex in |whitelist|.
bool IsFileWhitelisted(const std::string& file,
                       const std::vector<std::string>& whitelist) {
  for (const std::string& pattern : whitelist) {
    if (RE2::PartialMatch(file, pattern))
      return true;
  }
  return false;
}

// Searches the user data directory for files that contain |hostname| in the
// filename or as part of the content. Returns the number of files that
// do not match any regex in |whitelist|.
bool CheckUserDirectoryForString(const std::string& hostname,
                                 const std::vector<std::string>& whitelist) {
  base::FilePath user_data_dir =
      g_browser_process->profile_manager()->user_data_dir();
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FileEnumerator enumerator(
      user_data_dir, true /* recursive */,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  int found = 0;
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    // Remove |user_data_dir| part from path.
    std::string file =
        path.NormalizePathSeparatorsTo('/').AsUTF8Unsafe().substr(
            user_data_dir.AsUTF8Unsafe().length());

    // Check file name.
    if (file.find(hostname) != std::string::npos) {
      if (IsFileWhitelisted(file, whitelist)) {
        LOG(INFO) << "Whitelisted: " << file;
      } else {
        found++;
        LOG(WARNING) << "Found file name: " << file;
      }
    }

    // Check leveldb content.
    if (path.BaseName().AsUTF8Unsafe() == "CURRENT") {
      // LevelDB instances consist of a folder where most files have variable
      // names that contain a revision number.
      // All leveldb folders have a "CURRENT" file that points to the current
      // manifest. We consider all folders with a CURRENT file to be leveldb
      // instances and try to open them.
      std::unique_ptr<leveldb::DB> db;
      std::string db_file = path.DirName().AsUTF8Unsafe();
      auto status = leveldb_env::OpenDB(leveldb_env::Options(), db_file, &db);
      if (status.ok()) {
        std::unique_ptr<leveldb::Iterator> it(
            db->NewIterator(leveldb::ReadOptions()));
        for (it->SeekToFirst(); it->Valid(); it->Next()) {
          std::string entry =
              it->key().ToString() + ":" + it->value().ToString();
          if (entry.find(hostname) != std::string::npos) {
            LOG(WARNING) << "Found leveldb entry: " << file << " " << entry;
            found++;
          }
        }
      } else {
        // TODO(crbug.com/846297): Some databases are already open and the LOCK
        // prevents us from accessing them.
        LOG(INFO) << "Could not open: " << file << " " << status.ToString();
      }
    }

    // TODO(crbug.com/846297): Add support for sqlite and other formats that
    // possibly contain non-plaintext data.

    // Check file content.
    if (enumerator.GetInfo().IsDirectory())
      continue;
    std::string content;
    if (!base::ReadFileToString(path, &content)) {
      LOG(INFO) << "Could not read: " << file;
      continue;
    }
    size_t pos = content.find(hostname);
    if (pos != std::string::npos) {
      if (IsFileWhitelisted(file, whitelist)) {
        LOG(INFO) << "Whitelisted: " << file;
        continue;
      }
      found++;
      // Print surrounding text of the match.
      std::string partial_content = content.substr(
          pos < 30 ? 0 : pos - 30,
          std::min(content.size() - 1, pos + hostname.size() + 30));
      LOG(WARNING) << "Found file content: " << file << "\n"
                   << partial_content << "\n";
    }
  }
  return found;
}

class CookiesTreeObserver : public CookiesTreeModel::Observer {
 public:
  explicit CookiesTreeObserver(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

  void TreeModelBeginBatch(CookiesTreeModel* model) override {}

  void TreeModelEndBatch(CookiesTreeModel* model) override {
    std::move(quit_closure_).Run();
  }

  void TreeNodesAdded(ui::TreeModel* model,
                      ui::TreeModelNode* parent,
                      size_t start,
                      size_t count) override {}
  void TreeNodesRemoved(ui::TreeModel* model,
                        ui::TreeModelNode* parent,
                        size_t start,
                        size_t count) override {}
  void TreeNodeChanged(ui::TreeModel* model, ui::TreeModelNode* node) override {
  }

 private:
  base::OnceClosure quit_closure_;
};

// Returns the sum of the number of datatypes per host.
int GetCookiesTreeModelCount(const CookieTreeNode* root) {
  int count = 0;
  for (const auto& node : root->children()) {
    EXPECT_GE(node->children().size(), 1u);
    count += std::count_if(node->children().cbegin(), node->children().cend(),
                           [](const auto& child) {
                             // TODO(crbug.com/642955): Include quota nodes.
                             return child->GetDetailedInfo().node_type !=
                                    CookieTreeNode::DetailedInfo::TYPE_QUOTA;
                           });
  }
  return count;
}

// Returns a string with information about the content of the
// cookie tree model.
std::string GetCookiesTreeModelInfo(const CookieTreeNode* root) {
  std::stringstream info;
  info << "CookieTreeModel: " << std::endl;
  for (const auto& node : root->children()) {
    info << node->GetTitle() << std::endl;
    for (const auto& child : node->children()) {
      // Quota nodes are not included in the UI due to crbug.com/642955.
      const auto node_type = child->GetDetailedInfo().node_type;
      if (node_type != CookieTreeNode::DetailedInfo::TYPE_QUOTA)
        info << "  " << child->GetTitle() << " " << node_type << std::endl;
    }
  }
  return info.str();
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Sets the APISID Gaia cookie, which is monitored by the AccountReconcilor.
bool SetGaiaCookieForProfile(Profile* profile) {
  GURL google_url = GaiaUrls::GetInstance()->secure_google_url();
  net::CanonicalCookie cookie(
      "SAPISID", std::string(), "." + google_url.host(), "/", base::Time(),
      base::Time(), base::Time(), true /* secure */, false /* httponly */,
      net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT);
  bool success = false;
  base::RunLoop loop;
  base::OnceCallback<void(net::CanonicalCookie::CookieInclusionStatus)>
      callback = base::BindLambdaForTesting(
          [&success, &loop](net::CanonicalCookie::CookieInclusionStatus s) {
            success = s.IsInclude();
            loop.Quit();
          });
  network::mojom::CookieManager* cookie_manager =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetCookieManagerForBrowserProcess();
  cookie_manager->SetCanonicalCookie(cookie, google_url.scheme(),
                                     net::CookieOptions::MakeAllInclusive(),
                                     std::move(callback));
  loop.Run();
  return success;
}
#endif

}  // namespace

class BrowsingDataRemoverBrowserTest : public InProcessBrowserTest {
 public:
  BrowsingDataRemoverBrowserTest() {
    std::vector<base::Feature> enabled_features = {
        leveldb::kLevelDBRewriteFeature};
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
    enabled_features.push_back(media::kExternalClearKeyForTesting);
#endif
    feature_list_.InitWithFeatures(enabled_features, {});
  }

  // Call to use an Incognito browser rather than the default.
  void UseIncognitoBrowser() {
    ASSERT_EQ(nullptr, incognito_browser_);
    incognito_browser_ = CreateIncognitoBrowser();
  }

  Browser* GetBrowser() const {
    return incognito_browser_ ? incognito_browser_ : browser();
  }

  void SetUpOnMainThread() override {
    base::FilePath path;
    base::PathService::Get(content::DIR_TEST_DATA, &path);
    host_resolver()->AddRule(kExampleHost, "127.0.0.1");
    embedded_test_server()->ServeFilesFromDirectory(path);
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void RunScriptAndCheckResult(const std::string& script,
                               const std::string& result) {
    std::string data;
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        GetBrowser()->tab_strip_model()->GetActiveWebContents(), script,
        &data));
    ASSERT_EQ(data, result);
  }

  bool RunScriptAndGetBool(const std::string& script) {
    bool data;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        GetBrowser()->tab_strip_model()->GetActiveWebContents(), script,
        &data));
    return data;
  }

  void VerifyDownloadCount(size_t expected) {
    content::DownloadManager* download_manager =
        content::BrowserContext::GetDownloadManager(GetBrowser()->profile());
    std::vector<download::DownloadItem*> downloads;
    download_manager->GetAllDownloads(&downloads);
    EXPECT_EQ(expected, downloads.size());
  }

  void DownloadAnItem() {
    // Start a download.
    content::DownloadManager* download_manager =
        content::BrowserContext::GetDownloadManager(GetBrowser()->profile());
    std::unique_ptr<content::DownloadTestObserver> observer(
        new content::DownloadTestObserverTerminal(
            download_manager, 1,
            content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT));

    GURL download_url = ui_test_utils::GetTestUrl(
        base::FilePath().AppendASCII("downloads"),
        base::FilePath().AppendASCII("a_zip_file.zip"));
    ui_test_utils::NavigateToURL(GetBrowser(), download_url);
    observer->WaitForFinished();

    VerifyDownloadCount(1u);
  }

  void RemoveAndWait(int remove_mask) {
    RemoveAndWait(remove_mask, base::Time(), base::Time::Max());
  }

  void RemoveAndWait(int remove_mask, base::Time delete_begin) {
    RemoveAndWait(remove_mask, delete_begin, base::Time::Max());
  }

  void RemoveAndWait(int remove_mask,
                     base::Time delete_begin,
                     base::Time delete_end) {
    content::BrowsingDataRemover* remover =
        content::BrowserContext::GetBrowsingDataRemover(
            GetBrowser()->profile());
    content::BrowsingDataRemoverCompletionObserver completion_observer(remover);
    remover->RemoveAndReply(
        delete_begin, delete_end, remove_mask,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
        &completion_observer);
    completion_observer.BlockUntilCompletion();
  }

  void RemoveWithFilterAndWait(
      int remove_mask,
      std::unique_ptr<BrowsingDataFilterBuilder> filter_builder) {
    content::BrowsingDataRemover* remover =
        content::BrowserContext::GetBrowsingDataRemover(
            GetBrowser()->profile());
    content::BrowsingDataRemoverCompletionObserver completion_observer(remover);
    remover->RemoveWithFilterAndReply(
        base::Time(), base::Time::Max(), remove_mask,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
        std::move(filter_builder), &completion_observer);
    completion_observer.BlockUntilCompletion();
  }

  // Test a data type by creating a value and checking it is counted by the
  // cookie counter. Then it deletes the value and checks that it has been
  // deleted and the cookie counter is back to zero.
  void TestSiteData(const std::string& type, base::Time delete_begin) {
    EXPECT_EQ(0, GetSiteDataCount());
    GURL url = embedded_test_server()->GetURL("/browsing_data/site_data.html");
    ui_test_utils::NavigateToURL(GetBrowser(), url);

    EXPECT_EQ(0, GetSiteDataCount());
    ExpectCookieTreeModelCount(0);
    EXPECT_FALSE(HasDataForType(type));

    SetDataForType(type);
    EXPECT_EQ(1, GetSiteDataCount());
    ExpectCookieTreeModelCount(1);
    EXPECT_TRUE(HasDataForType(type));

    RemoveAndWait(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_DATA,
                  delete_begin);
    EXPECT_EQ(0, GetSiteDataCount());
    ExpectCookieTreeModelCount(0);
    EXPECT_FALSE(HasDataForType(type));
  }

  // Test that storage systems like filesystem and websql, where just an access
  // creates an empty store, are counted and deleted correctly.
  void TestEmptySiteData(const std::string& type, base::Time delete_begin) {
    EXPECT_EQ(0, GetSiteDataCount());
    ExpectCookieTreeModelCount(0);
    GURL url = embedded_test_server()->GetURL("/browsing_data/site_data.html");
    ui_test_utils::NavigateToURL(GetBrowser(), url);
    EXPECT_EQ(0, GetSiteDataCount());
    ExpectCookieTreeModelCount(0);
    // Opening a store of this type creates a site data entry.
    EXPECT_FALSE(HasDataForType(type));
    EXPECT_EQ(1, GetSiteDataCount());
    ExpectCookieTreeModelCount(1);
    RemoveAndWait(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_DATA,
                  delete_begin);

    EXPECT_EQ(0, GetSiteDataCount());
    ExpectCookieTreeModelCount(0);
  }

  bool HasDataForType(const std::string& type) {
    return RunScriptAndGetBool("has" + type + "()");
  }

  void SetDataForType(const std::string& type) {
    ASSERT_TRUE(RunScriptAndGetBool("set" + type + "()"))
        << "Couldn't create data for: " << type;
  }

  int GetSiteDataCount() {
    base::RunLoop run_loop;
    int count = -1;
    (new SiteDataCountingHelper(GetBrowser()->profile(), base::Time(),
                                base::BindLambdaForTesting([&](int c) {
                                  count = c;
                                  run_loop.Quit();
                                })))
        ->CountAndDestroySelfWhenFinished();
    run_loop.Run();
    return count;
  }

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  int GetMediaLicenseCount() {
    base::RunLoop run_loop;
    int count = -1;
    content::StoragePartition* partition =
        content::BrowserContext::GetDefaultStoragePartition(
            browser()->profile());
    scoped_refptr<BrowsingDataMediaLicenseHelper> media_license_helper =
        BrowsingDataMediaLicenseHelper::Create(
            partition->GetFileSystemContext());
    media_license_helper->StartFetching(base::BindLambdaForTesting(
        [&](const std::list<BrowsingDataMediaLicenseHelper::MediaLicenseInfo>&
                licenses) {
          count = licenses.size();
          LOG(INFO) << "Found " << count << " licenses.";
          for (const auto& license : licenses)
            LOG(INFO) << license.last_modified_time;
          run_loop.Quit();
        }));
    run_loop.Run();
    return count;
  }
#endif

  inline void ExpectCookieTreeModelCount(int expected) {
    std::unique_ptr<CookiesTreeModel> model = GetCookiesTreeModel();
    EXPECT_EQ(expected, GetCookiesTreeModelCount(model->GetRoot()))
        << GetCookiesTreeModelInfo(model->GetRoot());
  }

  void OnVideoDecodePerfInfo(base::RunLoop* run_loop,
                             bool* out_is_smooth,
                             bool* out_is_power_efficient,
                             bool is_smooth,
                             bool is_power_efficient) {
    *out_is_smooth = is_smooth;
    *out_is_power_efficient = is_power_efficient;
    run_loop->QuitWhenIdle();
  }

  network::mojom::NetworkContext* network_context() const {
    return content::BrowserContext::GetDefaultStoragePartition(
               GetBrowser()->profile())
        ->GetNetworkContext();
  }

 private:
  void OnCacheSizeResult(
      base::RunLoop* run_loop,
      browsing_data::BrowsingDataCounter::ResultInt* out_size,
      std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result) {
    if (!result->Finished())
      return;

    *out_size =
        static_cast<browsing_data::BrowsingDataCounter::FinishedResult*>(
            result.get())
            ->Value();
    run_loop->Quit();
  }

  std::unique_ptr<CookiesTreeModel> GetCookiesTreeModel() {
    Profile* profile = GetBrowser()->profile();
    content::StoragePartition* storage_partition =
        content::BrowserContext::GetDefaultStoragePartition(profile);
    content::IndexedDBContext* indexed_db_context =
        storage_partition->GetIndexedDBContext();
    content::ServiceWorkerContext* service_worker_context =
        storage_partition->GetServiceWorkerContext();
    content::CacheStorageContext* cache_storage_context =
        storage_partition->GetCacheStorageContext();
    storage::FileSystemContext* file_system_context =
        storage_partition->GetFileSystemContext();
    auto container = std::make_unique<LocalDataContainer>(
        new BrowsingDataCookieHelper(storage_partition),
        new BrowsingDataDatabaseHelper(profile),
        new BrowsingDataLocalStorageHelper(profile),
        /*session_storage_helper=*/nullptr,
        new BrowsingDataAppCacheHelper(storage_partition->GetAppCacheService()),
        new BrowsingDataIndexedDBHelper(indexed_db_context),
        BrowsingDataFileSystemHelper::Create(file_system_context),
        BrowsingDataQuotaHelper::Create(profile),
        new BrowsingDataServiceWorkerHelper(service_worker_context),
        new BrowsingDataSharedWorkerHelper(storage_partition,
                                           profile->GetResourceContext()),
        new BrowsingDataCacheStorageHelper(cache_storage_context),
        BrowsingDataFlashLSOHelper::Create(profile),
        BrowsingDataMediaLicenseHelper::Create(file_system_context));
    base::RunLoop run_loop;
    CookiesTreeObserver observer(run_loop.QuitClosure());
    auto model = std::make_unique<CookiesTreeModel>(
        std::move(container), profile->GetExtensionSpecialStoragePolicy());
    model->AddCookiesTreeObserver(&observer);
    run_loop.Run();
    return model;
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
    // Testing MediaLicenses requires additional command line parameters as
    // it uses the External Clear Key CDM.
    RegisterClearKeyCdm(command_line);
#endif
  }

  base::test::ScopedFeatureList feature_list_;
  Browser* incognito_browser_ = nullptr;
};

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Same as BrowsingDataRemoverBrowserTest, but forces Dice to be enabled.
class DiceBrowsingDataRemoverBrowserTest
    : public BrowsingDataRemoverBrowserTest {
 public:
  AccountInfo AddAccountToProfile(const std::string& account_id,
                                  Profile* profile,
                                  bool is_primary) {
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
    if (is_primary) {
      DCHECK(!identity_manager->HasPrimaryAccount());
      return signin::MakePrimaryAccountAvailable(identity_manager,
                                                 account_id + "@gmail.com");
    }
    auto account_info =
        signin::MakeAccountAvailable(identity_manager, account_id);
    DCHECK(
        identity_manager->HasAccountWithRefreshToken(account_info.account_id));
    return account_info;
  }
};
#endif

// Test BrowsingDataRemover for downloads.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, Download) {
  DownloadAnItem();
  RemoveAndWait(content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS);
  VerifyDownloadCount(0u);
}

// Test that the salt for media device IDs is reset when cookies are cleared.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, MediaDeviceIdSalt) {
  std::string original_salt = GetBrowser()->profile()->GetMediaDeviceIDSalt();
  RemoveAndWait(content::BrowsingDataRemover::DATA_TYPE_COOKIES);
  std::string new_salt = GetBrowser()->profile()->GetMediaDeviceIDSalt();
  EXPECT_NE(original_salt, new_salt);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Test that Sync is paused when cookies are cleared.
IN_PROC_BROWSER_TEST_F(DiceBrowsingDataRemoverBrowserTest, SyncToken) {
  Profile* profile = browser()->profile();
  // Set a Gaia cookie.
  ASSERT_TRUE(SetGaiaCookieForProfile(profile));
  // Set a Sync account and a secondary account.
  const char kPrimaryAccountId[] = "primary_account_id";
  AccountInfo primary_account =
      AddAccountToProfile(kPrimaryAccountId, profile, /*is_primary=*/true);
  const char kSecondaryAccountId[] = "secondary_account_id";
  AccountInfo secondary_account =
      AddAccountToProfile(kSecondaryAccountId, profile, /*is_primary=*/false);
  // Clear cookies.
  RemoveAndWait(content::BrowsingDataRemover::DATA_TYPE_COOKIES);
  // Check that the Sync account was not removed and Sync was paused.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  EXPECT_TRUE(
      identity_manager->HasAccountWithRefreshToken(primary_account.account_id));
  EXPECT_EQ(
      GoogleServiceAuthError::InvalidGaiaCredentialsReason::
          CREDENTIALS_REJECTED_BY_CLIENT,
      identity_manager
          ->GetErrorStateOfRefreshTokenForAccount(primary_account.account_id)
          .GetInvalidGaiaCredentialsReason());
  // Check that the secondary token was revoked.
  EXPECT_FALSE(identity_manager->HasAccountWithRefreshToken(
      secondary_account.account_id));
}

// Test that Sync is not paused when cookies are cleared, if synced data is
// being deleted.
IN_PROC_BROWSER_TEST_F(DiceBrowsingDataRemoverBrowserTest,
                       SyncTokenScopedDeletion) {
  Profile* profile = browser()->profile();
  // Set a Gaia cookie.
  ASSERT_TRUE(SetGaiaCookieForProfile(profile));
  // Set a Sync account and a secondary account.
  const char kPrimaryAccountId[] = "primary_account_id";
  AccountInfo primary_account =
      AddAccountToProfile(kPrimaryAccountId, profile, /*is_primary=*/true);
  const char kSecondaryAccountId[] = "secondary_account_id";
  AccountInfo secondary_account =
      AddAccountToProfile(kSecondaryAccountId, profile, /*is_primary=*/false);
  // Sync data is being deleted.
  std::unique_ptr<AccountReconcilor::ScopedSyncedDataDeletion> deletion =
      AccountReconcilorFactory::GetForProfile(profile)
          ->GetScopedSyncDataDeletion();
  // Clear cookies.
  RemoveAndWait(content::BrowsingDataRemover::DATA_TYPE_COOKIES);
  // Check that the Sync token was not revoked.
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  EXPECT_TRUE(
      identity_manager->HasAccountWithRefreshToken(primary_account.account_id));
  EXPECT_FALSE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account.account_id));
  // Check that the secondary token was revoked.
  EXPECT_FALSE(identity_manager->HasAccountWithRefreshToken(
      secondary_account.account_id));
}

// Test that Sync is paused when cookies are cleared if Sync was in error, even
// if synced data is being deleted.
IN_PROC_BROWSER_TEST_F(DiceBrowsingDataRemoverBrowserTest, SyncTokenError) {
  Profile* profile = browser()->profile();
  // Set a Gaia cookie.
  ASSERT_TRUE(SetGaiaCookieForProfile(profile));
  // Set a Sync account with authentication error.
  const char kAccountId[] = "account_id";
  AccountInfo primary_account =
      AddAccountToProfile(kAccountId, profile, /*is_primary=*/true);
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager, primary_account.account_id,
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));

  // Sync data is being deleted.
  std::unique_ptr<AccountReconcilor::ScopedSyncedDataDeletion> deletion =
      AccountReconcilorFactory::GetForProfile(profile)
          ->GetScopedSyncDataDeletion();
  // Clear cookies.
  RemoveAndWait(content::BrowsingDataRemover::DATA_TYPE_COOKIES);
  // Check that the account was not removed and Sync was paused.
  EXPECT_TRUE(
      identity_manager->HasAccountWithRefreshToken(primary_account.account_id));
  EXPECT_EQ(
      GoogleServiceAuthError::InvalidGaiaCredentialsReason::
          CREDENTIALS_REJECTED_BY_CLIENT,
      identity_manager
          ->GetErrorStateOfRefreshTokenForAccount(primary_account.account_id)
          .GetInvalidGaiaCredentialsReason());
}

// Test that the tokens are revoked when cookies are cleared when there is no
// primary account.
IN_PROC_BROWSER_TEST_F(DiceBrowsingDataRemoverBrowserTest, NoSync) {
  Profile* profile = browser()->profile();
  // Set a Gaia cookie.
  ASSERT_TRUE(SetGaiaCookieForProfile(profile));
  // Set a non-Sync account.
  const char kAccountId[] = "account_id";
  AccountInfo secondary_account =
      AddAccountToProfile(kAccountId, profile, /*is_primary=*/false);
  // Clear cookies.
  RemoveAndWait(content::BrowsingDataRemover::DATA_TYPE_COOKIES);
  // Check that the account was removed.
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  EXPECT_FALSE(identity_manager->HasAccountWithRefreshToken(
      secondary_account.account_id));
}
#endif

// The call to Remove() should crash in debug (DCHECK), but the browser-test
// process model prevents using a death test.
#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
// Test BrowsingDataRemover for prohibited downloads. Note that this only
// really exercises the code in a Release build.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, DownloadProhibited) {
  PrefService* prefs = GetBrowser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kAllowDeletingBrowserHistory, false);

  DownloadAnItem();
  RemoveAndWait(content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS);
  VerifyDownloadCount(1u);
}
#endif

// Verify VideoDecodePerfHistory is cleared when deleting all history from
// beginning of time.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, VideoDecodePerfHistory) {
  media::VideoDecodePerfHistory* video_decode_perf_history =
      GetBrowser()->profile()->GetVideoDecodePerfHistory();

  // Save a video decode record. Note: we avoid using a web page to generate the
  // stats as this takes at least 5 seconds and even then is not a guarantee
  // depending on scheduler. Manual injection is quick and non-flaky.
  const media::VideoCodecProfile kProfile = media::VP9PROFILE_PROFILE0;
  const gfx::Size kSize(100, 200);
  const int kFrameRate = 30;
  const int kFramesDecoded = 1000;
  const int kFramesDropped = .9 * kFramesDecoded;
  const int kFramesPowerEfficient = 0;
  const url::Origin kOrigin = url::Origin::Create(GURL("http://example.com"));
  const bool kIsTopFrame = true;
  const uint64_t kPlayerId = 1234u;

  media::mojom::PredictionFeatures prediction_features;
  prediction_features.profile = kProfile;
  prediction_features.video_size = kSize;
  prediction_features.frames_per_sec = kFrameRate;

  media::mojom::PredictionTargets prediction_targets;
  prediction_targets.frames_decoded = kFramesDecoded;
  prediction_targets.frames_dropped = kFramesDropped;
  prediction_targets.frames_power_efficient = kFramesPowerEfficient;

  {
    base::RunLoop run_loop;
    video_decode_perf_history->GetSaveCallback().Run(
        ukm::kInvalidSourceId, media::learning::FeatureValue(0), kIsTopFrame,
        prediction_features, prediction_targets, kPlayerId,
        run_loop.QuitWhenIdleClosure());
    run_loop.Run();
  }

  // Verify history exists.
  // Expect |is_smooth| = false and |is_power_efficient| = false given that 90%
  // of recorded frames were dropped and 0 were power efficient.
  bool is_smooth = true;
  bool is_power_efficient = true;
  {
    base::RunLoop run_loop;
    video_decode_perf_history->GetPerfInfo(
        media::mojom::PredictionFeatures::New(prediction_features),
        base::BindOnce(&BrowsingDataRemoverBrowserTest::OnVideoDecodePerfInfo,
                       base::Unretained(this), &run_loop, &is_smooth,
                       &is_power_efficient));
    run_loop.Run();
  }
  EXPECT_FALSE(is_smooth);
  EXPECT_FALSE(is_power_efficient);

  // Clear history.
  RemoveAndWait(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY);

  // Verify history no longer exists. Both |is_smooth| and |is_power_efficient|
  // should now report true because the VideoDecodePerfHistory optimistically
  // returns true when it has no data.
  {
    base::RunLoop run_loop;
    video_decode_perf_history->GetPerfInfo(
        media::mojom::PredictionFeatures::New(prediction_features),
        base::BindOnce(&BrowsingDataRemoverBrowserTest::OnVideoDecodePerfInfo,
                       base::Unretained(this), &run_loop, &is_smooth,
                       &is_power_efficient));
    run_loop.Run();
  }
  EXPECT_TRUE(is_smooth);
  EXPECT_TRUE(is_power_efficient);
}

// Verify can modify database after deleting it.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, Database) {
  GURL url = embedded_test_server()->GetURL("/simple_database.html");
  ui_test_utils::NavigateToURL(GetBrowser(), url);

  RunScriptAndCheckResult("createTable()", "done");
  RunScriptAndCheckResult("insertRecord('text')", "done");
  RunScriptAndCheckResult("getRecords()", "text");

  RemoveAndWait(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_DATA);

  ui_test_utils::NavigateToURL(GetBrowser(), url);
  RunScriptAndCheckResult("createTable()", "done");
  RunScriptAndCheckResult("insertRecord('text2')", "done");
  RunScriptAndCheckResult("getRecords()", "text2");
}

// Verifies that cache deletion finishes successfully. Completes deletion of
// cache should leave it empty, and partial deletion should leave nonzero
// amount of data. Note that this tests the integration of BrowsingDataRemover
// with ConditionalCacheDeletionHelper. Whether ConditionalCacheDeletionHelper
// actually deletes the correct entries is tested
// in ConditionalCacheDeletionHelperBrowsertest.
// TODO(crbug.com/817417): check the cache size instead of stopping the server
// and loading the request again.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, Cache) {
  // Load several resources.
  GURL url1 = embedded_test_server()->GetURL("/cachetime");
  GURL url2 = embedded_test_server()->GetURL(kExampleHost, "/cachetime");
  ASSERT_FALSE(url::IsSameOriginWith(url1, url2));

  EXPECT_EQ(net::OK, content::LoadBasicRequest(network_context(), url1));
  EXPECT_EQ(net::OK, content::LoadBasicRequest(network_context(), url2));

  // Check that the cache has been populated by revisiting these pages with the
  // server stopped.
  ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  EXPECT_EQ(net::OK, content::LoadBasicRequest(network_context(), url1));
  EXPECT_EQ(net::OK, content::LoadBasicRequest(network_context(), url2));

  // Partially delete cache data. Delete data for localhost, which is the origin
  // of |url1|, but not for |kExampleHost|, which is the origin of |url2|.
  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder =
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::WHITELIST);
  filter_builder->AddOrigin(url::Origin::Create(url1));
  RemoveWithFilterAndWait(content::BrowsingDataRemover::DATA_TYPE_CACHE,
                          std::move(filter_builder));

  // After the partial deletion, the cache should be smaller but still nonempty.
  EXPECT_NE(net::OK, content::LoadBasicRequest(network_context(), url1));
  EXPECT_EQ(net::OK, content::LoadBasicRequest(network_context(), url2));

  // Another partial deletion with the same filter should have no effect.
  filter_builder =
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::WHITELIST);
  filter_builder->AddOrigin(url::Origin::Create(url1));
  RemoveWithFilterAndWait(content::BrowsingDataRemover::DATA_TYPE_CACHE,
                          std::move(filter_builder));
  EXPECT_NE(net::OK, content::LoadBasicRequest(network_context(), url1));
  EXPECT_EQ(net::OK, content::LoadBasicRequest(network_context(), url2));

  // Delete the remaining data.
  RemoveAndWait(content::BrowsingDataRemover::DATA_TYPE_CACHE);

  // The cache should be empty.
  EXPECT_NE(net::OK, content::LoadBasicRequest(network_context(), url1));
  EXPECT_NE(net::OK, content::LoadBasicRequest(network_context(), url2));
}

// Crashes the network service while clearing the HTTP cache to make sure the
// clear operation does complete.
// Note that there is a race between crashing the network service and clearing
// the cache, so the test might flakily fail if the tested behavior does not
// work.
// TODO(crbug.com/813882): test retry behavior by validating the cache is empty
// after the crash.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest,
                       ClearCacheAndNetworkServiceCrashes) {
  if (!content::IsOutOfProcessNetworkService())
    return;

  // Clear the cached data with a task posted to crash the network service.
  // The task should be run while waiting for the cache clearing operation to
  // complete, hopefully it happens before the cache has been cleared.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&content::BrowserTestBase::SimulateNetworkServiceCrash,
                     base::Unretained(this)));

  RemoveAndWait(content::BrowsingDataRemover::DATA_TYPE_CACHE);
}

// Verifies that the network quality prefs are cleared.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, VerifyNQECacheCleared) {
  base::HistogramTester histogram_tester;
  RemoveAndWait(content::BrowsingDataRemover::DATA_TYPE_CACHE);

  // Wait until there is at least one sample in NQE.PrefsSizeOnClearing.
  bool histogram_populated = false;
  for (size_t attempt = 0; attempt < 3; ++attempt) {
    const std::vector<base::Bucket> buckets =
        histogram_tester.GetAllSamples("NQE.PrefsSizeOnClearing");
    for (const auto& bucket : buckets) {
      if (bucket.count > 0) {
        histogram_populated = true;
        break;
      }
    }
    if (histogram_populated)
      break;

    // Retry fetching the histogram since it's not populated yet.
    content::FetchHistogramsFromChildProcesses();
    SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    base::RunLoop().RunUntilIdle();
  }

  histogram_tester.ExpectTotalCount("NQE.PrefsSizeOnClearing", 1);
}

IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest,
                       ExternalProtocolHandlerPrefs) {
  Profile* profile = GetBrowser()->profile();
  base::DictionaryValue prefs;
  prefs.SetBoolean("tel", false);
  profile->GetPrefs()->Set(prefs::kExcludedSchemes, prefs);
  ExternalProtocolHandler::BlockState block_state =
      ExternalProtocolHandler::GetBlockState("tel", profile);
  ASSERT_EQ(ExternalProtocolHandler::DONT_BLOCK, block_state);
  RemoveAndWait(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_DATA);
  block_state = ExternalProtocolHandler::GetBlockState("tel", profile);
  ASSERT_EQ(ExternalProtocolHandler::UNKNOWN, block_state);
}

IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, HistoryDeletion) {
  const std::string kType = "History";
  GURL url = embedded_test_server()->GetURL("/browsing_data/site_data.html");
  // Create a new tab to avoid confusion from having a NTP navigation entry.
  ui_test_utils::NavigateToURLWithDisposition(
      GetBrowser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_FALSE(HasDataForType(kType));
  SetDataForType(kType);
  EXPECT_TRUE(HasDataForType(kType));
  // Remove history from navigation to site_data.html.
  RemoveAndWait(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY);
  EXPECT_FALSE(HasDataForType(kType));
  SetDataForType(kType);
  EXPECT_TRUE(HasDataForType(kType));
  // Remove history from previous pushState() call in setHistory().
  RemoveAndWait(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY);
  EXPECT_FALSE(HasDataForType(kType));
}

// Parameterized to run tests for different deletion time ranges.
class BrowsingDataRemoverBrowserTestP
    : public BrowsingDataRemoverBrowserTest,
      public testing::WithParamInterface<base::Time> {};

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP, CookieDeletion) {
  TestSiteData("Cookie", GetParam());
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP,
                       CookieIncognitoDeletion) {
  UseIncognitoBrowser();
  TestSiteData("Cookie", GetParam());
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP, SessionCookieDeletion) {
  TestSiteData("SessionCookie", GetParam());
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP, LocalStorageDeletion) {
  TestSiteData("LocalStorage", GetParam());
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP,
                       LocalStorageIncognitoDeletion) {
  UseIncognitoBrowser();
  TestSiteData("LocalStorage", GetParam());
}

// TODO(crbug.com/772337): DISABLED until session storage is working correctly.
// Add Incognito variant when this is re-enabled.
IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP,
                       DISABLED_SessionStorageDeletion) {
  TestSiteData("SessionStorage", GetParam());
}

// SessionStorage is not supported by site data counting and the cookie tree
// model but we can test the web visible behavior.
IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP,
                       SessionStorageDeletionWebOnly) {
  GURL url = embedded_test_server()->GetURL("/browsing_data/site_data.html");
  ui_test_utils::NavigateToURL(GetBrowser(), url);
  const std::string type = "SessionStorage";
  EXPECT_FALSE(HasDataForType(type));
  SetDataForType(type);
  EXPECT_TRUE(HasDataForType(type));
  RemoveAndWait(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_DATA,
                GetParam());
  EXPECT_FALSE(HasDataForType(type));
}

// Test that session storage is not counted until crbug.com/772337 is fixed.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, SessionStorageCounting) {
  EXPECT_EQ(0, GetSiteDataCount());
  ExpectCookieTreeModelCount(0);
  GURL url = embedded_test_server()->GetURL("/browsing_data/site_data.html");
  ui_test_utils::NavigateToURL(GetBrowser(), url);
  EXPECT_EQ(0, GetSiteDataCount());
  ExpectCookieTreeModelCount(0);
  SetDataForType("SessionStorage");
  EXPECT_EQ(0, GetSiteDataCount());
  ExpectCookieTreeModelCount(0);
  EXPECT_TRUE(HasDataForType("SessionStorage"));
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP, ServiceWorkerDeletion) {
  TestSiteData("ServiceWorker", GetParam());
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP,
                       ServiceWorkerIncognitoDeletion) {
  UseIncognitoBrowser();
  TestSiteData("ServiceWorker", GetParam());
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP, CacheStorageDeletion) {
  TestSiteData("CacheStorage", GetParam());
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP,
                       CacheStorageIncognitoDeletion) {
  UseIncognitoBrowser();
  TestSiteData("CacheStorage", GetParam());
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP, FileSystemDeletion) {
  TestSiteData("FileSystem", GetParam());
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP,
                       FileSystemIncognitoDeletion) {
  UseIncognitoBrowser();
  TestSiteData("FileSystem", GetParam());
}

// Test that empty filesystems are deleted correctly.
IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP,
                       EmptyFileSystemDeletion) {
  TestEmptySiteData("FileSystem", GetParam());
}

// Test that empty filesystems are deleted correctly in incognito mode.
IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP,
                       EmptyFileSystemIncognitoDeletion) {
  UseIncognitoBrowser();
  TestEmptySiteData("FileSystem", GetParam());
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP, WebSqlDeletion) {
  TestSiteData("WebSql", GetParam());
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP,
                       WebSqlIncognitoDeletion) {
  UseIncognitoBrowser();
  TestSiteData("WebSql", GetParam());
}

// Test that empty websql dbs are deleted correctly.
IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP, EmptyWebSqlDeletion) {
  TestEmptySiteData("WebSql", GetParam());
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP, IndexedDbDeletion) {
  TestSiteData("IndexedDb", GetParam());
}

IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP,
                       IndexedDbIncognitoDeletion) {
  UseIncognitoBrowser();
  TestSiteData("IndexedDb", GetParam());
}

// Test that empty indexed dbs are deleted correctly.
IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP, EmptyIndexedDb) {
  TestEmptySiteData("IndexedDb", GetParam());
}

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
// Test Media Licenses by creating one and checking it is counted by the
// cookie counter. Then delete it and check that the cookie counter is back
// to zero.
IN_PROC_BROWSER_TEST_P(BrowsingDataRemoverBrowserTestP, MediaLicenseDeletion) {
  const std::string kMediaLicenseType = "MediaLicense";
  const base::Time delete_begin = GetParam();

  EXPECT_EQ(0, GetSiteDataCount());
  EXPECT_EQ(0, GetMediaLicenseCount());
  GURL url =
      embedded_test_server()->GetURL("/browsing_data/media_license.html");
  ui_test_utils::NavigateToURL(browser(), url);

  EXPECT_EQ(0, GetSiteDataCount());
  EXPECT_EQ(0, GetMediaLicenseCount());
  ExpectCookieTreeModelCount(0);
  EXPECT_FALSE(HasDataForType(kMediaLicenseType));

  SetDataForType(kMediaLicenseType);
  EXPECT_EQ(1, GetSiteDataCount());
  EXPECT_EQ(1, GetMediaLicenseCount());
  ExpectCookieTreeModelCount(1);
  EXPECT_TRUE(HasDataForType(kMediaLicenseType));

  // Try to remove the Media Licenses using a time frame up until an hour ago,
  // which should not remove the recently created Media License.
  RemoveAndWait(content::BrowsingDataRemover::DATA_TYPE_MEDIA_LICENSES,
                delete_begin, kLastHour);
  EXPECT_EQ(1, GetSiteDataCount());
  EXPECT_EQ(1, GetMediaLicenseCount());
  ExpectCookieTreeModelCount(1);
  EXPECT_TRUE(HasDataForType(kMediaLicenseType));

  // Now try with a time range that includes the current time, which should
  // clear the Media License created for this test.
  RemoveAndWait(content::BrowsingDataRemover::DATA_TYPE_MEDIA_LICENSES,
                delete_begin, base::Time::Max());
  EXPECT_EQ(0, GetSiteDataCount());
  EXPECT_EQ(0, GetMediaLicenseCount());
  ExpectCookieTreeModelCount(0);
  EXPECT_FALSE(HasDataForType(kMediaLicenseType));
}

// Create and save a media license (which will be deleted in the following
// test).
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest,
                       PRE_MediaLicenseTimedDeletion) {
  const std::string kMediaLicenseType = "MediaLicense";

  EXPECT_EQ(0, GetSiteDataCount());
  EXPECT_EQ(0, GetMediaLicenseCount());

  GURL url =
      embedded_test_server()->GetURL("/browsing_data/media_license.html");
  ui_test_utils::NavigateToURL(browser(), url);

  EXPECT_EQ(0, GetSiteDataCount());
  EXPECT_EQ(0, GetMediaLicenseCount());
  ExpectCookieTreeModelCount(0);
  EXPECT_FALSE(HasDataForType(kMediaLicenseType));

  SetDataForType(kMediaLicenseType);
  EXPECT_EQ(1, GetSiteDataCount());
  EXPECT_EQ(1, GetMediaLicenseCount());
  ExpectCookieTreeModelCount(1);
  EXPECT_TRUE(HasDataForType(kMediaLicenseType));
}

// Create and save a second media license, and then verify that timed deletion
// selects the correct license to delete.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest,
                       MediaLicenseTimedDeletion) {
  const std::string kMediaLicenseType = "MediaLicense";

  // As the PRE_ test should run first, there should be one media license
  // still stored. The time of it's creation should be sometime before
  // this test starts. We can't see the license, since it's stored for a
  // different origin (but we can delete it).
  const base::Time start = base::Time::Now();
  LOG(INFO) << "MediaLicenseTimedDeletion starting @ " << start;
  EXPECT_EQ(1, GetMediaLicenseCount());

  GURL url =
      embedded_test_server()->GetURL("/browsing_data/media_license.html");
  ui_test_utils::NavigateToURL(browser(), url);

#if defined(OS_MACOSX)
  // On some Macs the file system uses second granularity. So before
  // creating the second license, delay for 1 second so that the new
  // license's time is not the same second as |start|.
  base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(1));
#endif

  // This test should use a different domain than the PRE_ test, so there
  // should be no existing media license for it.
  // Note that checking HasDataForType() may result in an empty file being
  // created. Deleting licenses checks for any file within the time range
  // specified in order to delete all the files for the domain, so this may
  // cause problems (especially with Macs that use second granularity).
  // http://crbug.com/909829.
  EXPECT_FALSE(HasDataForType(kMediaLicenseType));

  // Create a media license for this domain.
  SetDataForType(kMediaLicenseType);
  EXPECT_EQ(2, GetMediaLicenseCount());
  EXPECT_TRUE(HasDataForType(kMediaLicenseType));

  // As Clear Browsing Data typically deletes recent data (e.g. last hour,
  // last day, etc.), try to remove the Media Licenses created since the
  // the start of this test, which should only delete the just created
  // media license, and leave the one created by the PRE_ test.
  RemoveAndWait(content::BrowsingDataRemover::DATA_TYPE_MEDIA_LICENSES, start);
  EXPECT_EQ(1, GetMediaLicenseCount());
  EXPECT_FALSE(HasDataForType(kMediaLicenseType));

  // Now try with a time range that includes all time, which should
  // clear the media license created by the PRE_ test.
  RemoveAndWait(content::BrowsingDataRemover::DATA_TYPE_MEDIA_LICENSES);
  EXPECT_EQ(0, GetMediaLicenseCount());
  ExpectCookieTreeModelCount(0);
}

IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest,
                       MediaLicenseDeletionWithFilter) {
  const std::string kMediaLicenseType = "MediaLicense";

  GURL url =
      embedded_test_server()->GetURL("/browsing_data/media_license.html");
  ui_test_utils::NavigateToURL(browser(), url);

  EXPECT_EQ(0, GetMediaLicenseCount());
  EXPECT_FALSE(HasDataForType(kMediaLicenseType));

  SetDataForType(kMediaLicenseType);
  EXPECT_EQ(1, GetMediaLicenseCount());
  EXPECT_TRUE(HasDataForType(kMediaLicenseType));

  // Try to remove the Media Licenses using a whitelist that doesn't include
  // the current URL. Media License should not be deleted.
  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder =
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::WHITELIST);
  filter_builder->AddOrigin(
      url::Origin::CreateFromNormalizedTuple("https", "test-origin", 443));
  RemoveWithFilterAndWait(
      content::BrowsingDataRemover::DATA_TYPE_MEDIA_LICENSES,
      std::move(filter_builder));
  EXPECT_EQ(1, GetMediaLicenseCount());

  // Now try with a blacklist that includes the current URL. Media License
  // should not be deleted.
  filter_builder =
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::BLACKLIST);
  filter_builder->AddOrigin(url::Origin::Create(url));
  RemoveWithFilterAndWait(
      content::BrowsingDataRemover::DATA_TYPE_MEDIA_LICENSES,
      std::move(filter_builder));
  EXPECT_EQ(1, GetMediaLicenseCount());

  // Now try with a whitelist that includes the current URL. Media License
  // should be deleted this time.
  filter_builder =
      BrowsingDataFilterBuilder::Create(BrowsingDataFilterBuilder::WHITELIST);
  filter_builder->AddOrigin(url::Origin::Create(url));
  RemoveWithFilterAndWait(
      content::BrowsingDataRemover::DATA_TYPE_MEDIA_LICENSES,
      std::move(filter_builder));
  EXPECT_EQ(0, GetMediaLicenseCount());
}
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

const std::vector<std::string> kStorageTypes{
    "Cookie",    "LocalStorage", "FileSystem",    "SessionStorage",
    "IndexedDb", "WebSql",       "ServiceWorker", "CacheStorage",
};

// Test that storage doesn't leave any traces on disk.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest,
                       PRE_PRE_StorageRemovedFromDisk) {
  ASSERT_EQ(0, CheckUserDirectoryForString(kLocalHost, {}));
  ASSERT_EQ(0, GetSiteDataCount());
  ExpectCookieTreeModelCount(0);

  // To use secure-only features on a host name, we need an https server.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(
      net::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
  base::FilePath path;
  base::PathService::Get(content::DIR_TEST_DATA, &path);
  https_server.ServeFilesFromDirectory(path);
  ASSERT_TRUE(https_server.Start());

  GURL url = https_server.GetURL(kLocalHost, "/browsing_data/site_data.html");
  ui_test_utils::NavigateToURL(GetBrowser(), url);

  for (const std::string& type : kStorageTypes) {
    SetDataForType(type);
    EXPECT_TRUE(HasDataForType(type));
  }
  // TODO(crbug.com/846297): Add more datatypes for testing. E.g. notifications,
  // payment handler, content settings, autofill, ...?
}

// Restart after creating the data to ensure that everything was written to
// disk.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest,
                       PRE_StorageRemovedFromDisk) {
  EXPECT_EQ(1, GetSiteDataCount());
  // Expect all datatypes from above except SessionStorage. SessionStorage is
  // not supported by the CookieTreeModel yet.
  ExpectCookieTreeModelCount(kStorageTypes.size() - 1);
  RemoveAndWait(ChromeBrowsingDataRemoverDelegate::DATA_TYPE_SITE_DATA |
                content::BrowsingDataRemover::DATA_TYPE_CACHE |
                ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY |
                ChromeBrowsingDataRemoverDelegate::DATA_TYPE_CONTENT_SETTINGS);
  EXPECT_EQ(0, GetSiteDataCount());
  ExpectCookieTreeModelCount(0);
}

// Check if any data remains after a deletion and a Chrome restart to force
// all writes to be finished.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest, StorageRemovedFromDisk) {
  // Deletions should remove all traces of browsing data from disk
  // but there are a few bugs that need to be fixed.
  // Any addition to this list must have an associated TODO().
  static const std::vector<std::string> whitelist = {
#if defined(OS_CHROMEOS)
    // TODO(crbug.com/846297): Many leveldb files remain on ChromeOS. I couldn't
    // reproduce this in manual testing, so it might be a timing issue when
    // Chrome is closed after the second test?
    "[0-9]{6}",
#endif
  };
  int found = CheckUserDirectoryForString(kLocalHost, whitelist);
  EXPECT_EQ(0, found) << "A non-whitelisted file contains the hostname.";
}

// TODO(crbug.com/840080): Filesystem can't be deleted on exit correctly at the
// moment.
// TODO(crbug.com/927312): LocalStorage deletion is flaky.
const std::vector<std::string> kSessionOnlyStorageTestTypes{
    "Cookie",
    // "LocalStorage",
    // "FileSystem",
    "SessionStorage",
    "IndexedDb",
    "WebSql",
    "ServiceWorker",
    "CacheStorage",
};

// Test that storage gets deleted if marked as SessionOnly.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest,
                       PRE_SessionOnlyStorageRemoved) {
  ExpectCookieTreeModelCount(0);
  GURL url = embedded_test_server()->GetURL("/browsing_data/site_data.html");
  ui_test_utils::NavigateToURL(GetBrowser(), url);

  for (const std::string& type : kSessionOnlyStorageTestTypes) {
    SetDataForType(type);
    EXPECT_TRUE(HasDataForType(type));
  }
  // Expect the datatypes from above except SessionStorage. SessionStorage is
  // not supported by the CookieTreeModel yet.
  ExpectCookieTreeModelCount(kSessionOnlyStorageTestTypes.size() - 1);
  HostContentSettingsMapFactory::GetForProfile(GetBrowser()->profile())
      ->SetDefaultContentSetting(ContentSettingsType::COOKIES,
                                 CONTENT_SETTING_SESSION_ONLY);
}

// Restart to delete session only storage.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverBrowserTest,
                       SessionOnlyStorageRemoved) {
  // All cookies should have been deleted.
  ExpectCookieTreeModelCount(0);
  GURL url = embedded_test_server()->GetURL("/browsing_data/site_data.html");
  ui_test_utils::NavigateToURL(GetBrowser(), url);
  for (const std::string& type : kSessionOnlyStorageTestTypes) {
    EXPECT_FALSE(HasDataForType(type));
  }
}

// Some storage backend use a different code path for full deletions and
// partial deletions, so we need to test both.
INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         BrowsingDataRemoverBrowserTestP,
                         ::testing::Values(base::Time(), kLastHour));
