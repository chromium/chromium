// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_remover_browsertest_base.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/browsing_data/counters/site_data_counting_helper.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/leveldatabase/leveldb_features.h"
#include "ui/base/models/tree_model.h"

namespace {

// Class for waiting for download manager to be initiailized.
class DownloadManagerWaiter : public content::DownloadManager::Observer {
 public:
  explicit DownloadManagerWaiter(content::DownloadManager* download_manager)
      : initialized_(false), download_manager_(download_manager) {
    download_manager_->AddObserver(this);
  }

  ~DownloadManagerWaiter() override { download_manager_->RemoveObserver(this); }

  void WaitForInitialized() {
    initialized_ = download_manager_->IsManagerInitialized();
    if (initialized_)
      return;
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void OnManagerInitialized() override {
    initialized_ = true;
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

 private:
  base::OnceClosure quit_closure_;
  bool initialized_;
  content::DownloadManager* download_manager_;
};
}  // namespace

BrowsingDataRemoverBrowserTestBase::BrowsingDataRemoverBrowserTestBase() =
    default;

BrowsingDataRemoverBrowserTestBase::~BrowsingDataRemoverBrowserTestBase() =
    default;

void BrowsingDataRemoverBrowserTestBase::InitFeatureList(
    std::vector<base::Feature> enabled_features) {
  feature_list_.InitWithFeatures(enabled_features, {});
}

// Call to use an Incognito browser rather than the default.
void BrowsingDataRemoverBrowserTestBase::UseIncognitoBrowser() {
  ASSERT_EQ(nullptr, incognito_browser_);
  incognito_browser_ = CreateIncognitoBrowser();
}

Browser* BrowsingDataRemoverBrowserTestBase::GetBrowser() const {
  return incognito_browser_ ? incognito_browser_ : browser();
}

void BrowsingDataRemoverBrowserTestBase::SetUpOnMainThread() {
  base::FilePath path;
  base::PathService::Get(content::DIR_TEST_DATA, &path);
  embedded_test_server()->ServeFilesFromDirectory(path);
  ASSERT_TRUE(embedded_test_server()->Start());
}

void BrowsingDataRemoverBrowserTestBase::RunScriptAndCheckResult(
    const std::string& script,
    const std::string& result) {
  std::string data;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      GetBrowser()->tab_strip_model()->GetActiveWebContents(), script, &data));
  ASSERT_EQ(data, result);
}

bool BrowsingDataRemoverBrowserTestBase::RunScriptAndGetBool(
    const std::string& script) {
  bool data;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      GetBrowser()->tab_strip_model()->GetActiveWebContents(), script, &data));
  return data;
}

void BrowsingDataRemoverBrowserTestBase::VerifyDownloadCount(size_t expected) {
  content::DownloadManager* download_manager =
      content::BrowserContext::GetDownloadManager(GetBrowser()->profile());
  DownloadManagerWaiter download_manager_waiter(download_manager);
  download_manager_waiter.WaitForInitialized();
  std::vector<download::DownloadItem*> downloads;
  download_manager->GetAllDownloads(&downloads);
  EXPECT_EQ(expected, downloads.size());
}

void BrowsingDataRemoverBrowserTestBase::DownloadAnItem() {
  // Start a download.
  content::DownloadManager* download_manager =
      content::BrowserContext::GetDownloadManager(GetBrowser()->profile());
  std::unique_ptr<content::DownloadTestObserver> observer(
      new content::DownloadTestObserverTerminal(
          download_manager, 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT));

  GURL download_url =
      ui_test_utils::GetTestUrl(base::FilePath().AppendASCII("downloads"),
                                base::FilePath().AppendASCII("a_zip_file.zip"));
  ui_test_utils::NavigateToURL(GetBrowser(), download_url);
  observer->WaitForFinished();

  VerifyDownloadCount(1u);
}

bool BrowsingDataRemoverBrowserTestBase::HasDataForType(
    const std::string& type) {
  return RunScriptAndGetBool("has" + type + "()");
}

void BrowsingDataRemoverBrowserTestBase::SetDataForType(
    const std::string& type) {
  ASSERT_TRUE(RunScriptAndGetBool("set" + type + "()"))
      << "Couldn't create data for: " << type;
}

int BrowsingDataRemoverBrowserTestBase::GetSiteDataCount() {
  base::RunLoop run_loop;
  int count = -1;
  (new SiteDataCountingHelper(GetBrowser()->profile(), base::Time(),
                              base::Time::Max(),
                              base::BindLambdaForTesting([&](int c) {
                                count = c;
                                run_loop.Quit();
                              })))
      ->CountAndDestroySelfWhenFinished();
  run_loop.Run();
  return count;
}

network::mojom::NetworkContext*
BrowsingDataRemoverBrowserTestBase::network_context() const {
  return content::BrowserContext::GetDefaultStoragePartition(
             GetBrowser()->profile())
      ->GetNetworkContext();
}
