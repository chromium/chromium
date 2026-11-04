// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/numerics/byte_conversions.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/db/sb_local_database_manager.h"
#include "components/safe_browsing/core/browser/db/sb_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/sb_update_protocol_manager.h"
#include "components/safe_browsing/core/browser/db/v4_store.pb.h"
#include "components/safe_browsing/core/browser/db/v5_embedded_test_server_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/test/browser_test.h"
#include "crypto/hash.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {

class V5LocalDatabaseBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    const GURL bad_url =
        embedded_test_server()->GetURL("/safe_browsing/malware.html");
    FullHashStr full_hash = SBProtocolManagerUtil::GetFullHash(bad_url);
    HashPrefixStr prefix = SBProtocolManagerUtil::GetHashPrefix(full_hash);
    const auto sha256 = crypto::hash::Sha256(base::as_byte_span(prefix));

    V5::FullHash match;
    match.set_full_hash(full_hash);
    match.add_full_hash_details()->set_threat_type(V5::ThreatType::MALWARE);

    V5::HashList malware_list;
    std::string list_name = GetV5ListName(GetUrlMalwareId());
    malware_list.set_name(list_name);
    malware_list.set_version("v1");
    malware_list.mutable_minimum_wait_duration()->set_seconds(3600);
    malware_list.mutable_additions_four_bytes()->set_first_value(
        base::U32FromBigEndian(
            base::span<const uint8_t, 4>(base::as_byte_span(prefix))));
    malware_list.set_sha256_checksum(std::string(sha256.begin(), sha256.end()));

    StartRedirectingV5SearchHashesRequestsForTesting(
        /*response_map=*/{{bad_url, match}}, embedded_test_server());
    StartRedirectingV5UpdateRequestsForTesting(
        /*hash_lists_map=*/{{list_name, std::move(malware_list)}},
        embedded_test_server());
    embedded_test_server()->StartAcceptingConnections();
    InProcessBrowserTest::SetUp();
  }

  // Waits until startup checksum verification completes and the initial list
  // update timer is scheduled.
  void WaitForStartupVerification() {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return SBLocalDatabaseManager::current_local_database_manager()
          ->update_protocol_manager_->update_timer_.IsRunning();
    }));
  }

  // Fires the scheduled update timer to trigger and complete the initial list
  // update.
  void WaitForInitialUpdate() {
    WaitForStartupVerification();
    base::RunLoop run_loop;
    base::CallbackListSubscription subscription =
        g_browser_process->safe_browsing_service()
            ->database_manager()
            ->RegisterDatabaseUpdatedCallback(run_loop.QuitClosure());
    SBLocalDatabaseManager::current_local_database_manager()
        ->update_protocol_manager_->update_timer_.FireNow();
    run_loop.Run();
  }

  bool NavigateAndCheckIsBlocked(const GURL& url) {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    return chrome_browser_interstitials::IsShowingInterstitial(
        browser()->GetTabStripModel()->GetActiveWebContents());
  }

 private:
  base::test::ScopedFeatureList feature_list_{kLocalListsUseSBv5};
};

IN_PROC_BROWSER_TEST_F(V5LocalDatabaseBrowserTest, DatabaseUpdateBlocksUrl) {
  const GURL bad_url =
      embedded_test_server()->GetURL("/safe_browsing/malware.html");
  EXPECT_FALSE(NavigateAndCheckIsBlocked(bad_url));
  WaitForInitialUpdate();
  EXPECT_TRUE(NavigateAndCheckIsBlocked(bad_url));
}

class V5LocalDatabaseDiskMigrationBrowserTest
    : public V5LocalDatabaseBrowserTest {
 public:
  bool SetUpUserDataDirectory() override {
    CHECK(V5LocalDatabaseBrowserTest::SetUpUserDataDirectory());
    base::FilePath sb_dir = SafeBrowsingServiceImpl::GetBaseFilename();
    EXPECT_TRUE(base::CreateDirectory(sb_dir));
    const GURL bad_url =
        embedded_test_server()->GetURL("/safe_browsing/malware.html");
    HashPrefixStr prefix = SBProtocolManagerUtil::GetHashPrefix(
        SBProtocolManagerUtil::GetFullHash(bad_url));

    V4StoreFileFormat file_format;
    file_format.set_magic_number(0x600D71FE);
    file_format.set_version_number(9);
    ListUpdateResponse* response = file_format.mutable_list_update_response();
    response->set_new_client_state("v1");
    response->set_response_type(ListUpdateResponse::FULL_UPDATE);
    const auto sha256 = crypto::hash::Sha256(base::as_byte_span(prefix));
    response->mutable_checksum()->set_sha256(
        std::string(sha256.begin(), sha256.end()));
    HashFile* hash_file = file_format.add_hash_files();
    hash_file->set_prefix_size(4);
    hash_file->set_extension("4_1");
    hash_file->set_file_size(4);

    base::FilePath store_path = sb_dir.AppendASCII("UrlMalware.store");
    EXPECT_TRUE(base::WriteFile(store_path, file_format.SerializeAsString()));
    EXPECT_TRUE(base::WriteFile(store_path.AddExtensionASCII("4_1"), prefix));
    return true;
  }
};

IN_PROC_BROWSER_TEST_F(V5LocalDatabaseDiskMigrationBrowserTest,
                       MigratesV4StoreToV5AndBlocksUrl) {
  const GURL bad_url =
      embedded_test_server()->GetURL("/safe_browsing/malware.html");
  WaitForStartupVerification();

  base::FilePath sb_dir = SafeBrowsingServiceImpl::GetBaseFilename();
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::PathExists(sb_dir.AppendASCII("UrlMalware_v5.store")));
    EXPECT_FALSE(base::PathExists(sb_dir.AppendASCII("UrlMalware.store")));
  }
  EXPECT_TRUE(NavigateAndCheckIsBlocked(bad_url));
}

}  // namespace safe_browsing
