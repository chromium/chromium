// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "url/gurl.h"

using content::WebContents;

class PluginResponseInterceptorURLLoaderThrottleBrowserTest
    : public extensions::ExtensionApiTest {
 public:
  PluginResponseInterceptorURLLoaderThrottleBrowserTest() {}
  ~PluginResponseInterceptorURLLoaderThrottleBrowserTest() override {}

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    extensions::ExtensionApiTest::TearDownOnMainThread();
  }

  WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

class DownloadObserver : public content::DownloadManager::Observer {
 public:
  DownloadObserver() {}
  ~DownloadObserver() override {}

  const GURL& GetLastUrl() {
    // Wait until the download has been created.
    download_run_loop_.Run();
    return last_url_;
  }

  // content::DownloadManager::Observer implementation.
  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override {
    last_url_ = item->GetURL();
    download_run_loop_.Quit();
  }

 private:
  base::RunLoop download_run_loop_;
  GURL last_url_;
};

IN_PROC_BROWSER_TEST_F(PluginResponseInterceptorURLLoaderThrottleBrowserTest,
                       DownloadPdf) {
  // Register a download observer.
  WebContents* web_contents = GetActiveWebContents();
  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  content::DownloadManager* download_manager =
      browser_context->GetDownloadManager();
  DownloadObserver download_observer;
  download_manager->AddObserver(&download_observer);

  // Navigate to a PDF that's marked as an attachment and test that it
  // is downloaded.
  GURL url(embedded_test_server()->GetURL("/download.pdf"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_EQ(url, download_observer.GetLastUrl());

  // Didn't launch a PDF plugin process.
  EXPECT_EQ(0u, pdf_extension_test_util::CountPdfPluginProcesses(browser()));

  // Cancel the download to shutdown cleanly.
  download_manager->RemoveObserver(&download_observer);
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  download_manager->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  downloads[0]->Cancel(false);
}
