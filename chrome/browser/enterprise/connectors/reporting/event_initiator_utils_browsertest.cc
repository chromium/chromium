// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/event_initiator_utils.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/download/download_test_file_activity_observer.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_source.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/test/test_extension_dir.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace enterprise_connectors {

namespace {

using ExtensionInfo =
    safe_browsing::ExtensionTelemetryReportRequest::ExtensionInfo;

}  // namespace

class EventInitiatorUtilsBrowserTest : public InProcessBrowserTest {
 public:
  EventInitiatorUtilsBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        kEnterpriseExtensionActorAttribution);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  content::WebContents* web_contents() {
    return browser()->GetTabStripModel()->GetActiveWebContents();
  }

  content::DownloadManager* download_manager() {
    return browser()->GetProfile()->GetDownloadManager();
  }

  // Navigates the active tab to `url` by calling `chrome.tabs.update()` as
  // `extension`.
  [[nodiscard]] bool TabsUpdate(const extensions::Extension& extension,
                                const GURL& url) {
    auto function = base::MakeRefCounted<extensions::TabsUpdateFunction>();
    function->set_extension(&extension);
    return extensions::api_test_utils::RunFunction(
        function.get(),
        base::StringPrintf(
            R"([%d, {"url": "%s"}])",
            extensions::ExtensionTabUtil::GetTabId(web_contents()),
            url.spec().c_str()),
        browser()->GetProfile(),
        extensions::api_test_utils::FunctionMode::kNone);
  }

  // Returns the profile's download, or null unless there is exactly one.
  download::DownloadItem* GetOnlyDownload() {
    content::DownloadManager::DownloadVector downloads;
    download_manager()->GetAllDownloads(&downloads);
    return downloads.size() == 1u ? downloads[0].get() : nullptr;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// An extension that points a tab at a file with `chrome.tabs.update()` starts
// a download without the `chrome.downloads` API. The download is attributed to
// the extension through its request initiator.
IN_PROC_BROWSER_TEST_F(EventInitiatorUtilsBrowserTest,
                       TabsUpdateToDownloadIsAttributed) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("Test").Build();
  // Installed, so that its name and version are found in `ExtensionRegistry`.
  extensions::ExtensionRegistrar::Get(browser()->GetProfile())
      ->AddExtension(extension);

  content::DownloadTestObserverTerminal observer(
      download_manager(), /*wait_count=*/1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  // Served with "Content-Disposition: attachment".
  ASSERT_TRUE(TabsUpdate(
      *extension, embedded_test_server()->GetURL("/download-test3.gif")));
  observer.WaitForFinished();

  download::DownloadItem* download = GetOnlyDownload();
  ASSERT_TRUE(download);
  std::optional<ExtensionInfo> extension_info =
      GetExtensionInitiatorInfo(download);
  ASSERT_TRUE(extension_info.has_value());
  EXPECT_EQ(extension->id(), extension_info->id());
  EXPECT_EQ(extension->name(), extension_info->name());
  EXPECT_EQ(extension->VersionString(), extension_info->version());
}

// An `<a download>` link on an extension page starts a renderer-initiated
// download. The download is attributed to the extension through its request
// initiator.
IN_PROC_BROWSER_TEST_F(EventInitiatorUtilsBrowserTest,
                       AnchorDownloadOnExtensionPageIsAttributed) {
  extensions::TestExtensionDir extension_dir;
  extension_dir.WriteManifest(
      R"({"name": "Test", "version": "1.0", "manifest_version": 3})");
  // The link targets a file in the extension: the `download` attribute is only
  // honored for a URL that the page can read.
  extension_dir.WriteFile(
      FILE_PATH_LITERAL("page.html"),
      R"(<a id="link" href="file.txt" download>Download</a>)");
  extension_dir.WriteFile(FILE_PATH_LITERAL("file.txt"), "content");
  scoped_refptr<const extensions::Extension> extension =
      extensions::ChromeTestExtensionLoader(browser()->GetProfile())
          .LoadExtension(extension_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), extension->GetResourceURL("page.html")));

  content::DownloadTestObserverTerminal observer(
      download_manager(), /*wait_count=*/1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  ASSERT_TRUE(content::ExecJs(web_contents(),
                              "document.getElementById('link').click();"));
  observer.WaitForFinished();

  download::DownloadItem* download = GetOnlyDownload();
  ASSERT_TRUE(download);
  EXPECT_EQ(download::DownloadSource::FROM_RENDERER,
            download->GetDownloadSource());
  std::optional<ExtensionInfo> extension_info =
      GetExtensionInitiatorInfo(download);
  ASSERT_TRUE(extension_info.has_value());
  EXPECT_EQ(extension->id(), extension_info->id());
}

// "Save page as" on a page that an extension navigated to is the user's
// action. The save replays the initiator of that navigation, so the extension
// is the request initiator of the download, but it is not attributed.
IN_PROC_BROWSER_TEST_F(EventInitiatorUtilsBrowserTest,
                       SavingExtensionNavigatedPageIsNotAttributed) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("Test").Build();
  // Unlike an HTML page, an image is saved by downloading it again.
  ASSERT_TRUE(TabsUpdate(
      *extension, embedded_test_server()->GetURL("/downloads/image.jpg")));
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));

  // Accepts the "Save as" dialog.
  DownloadTestFileActivityObserver file_activity_observer(
      browser()->GetProfile());
  file_activity_observer.EnableFileChooser(true);
  content::DownloadTestObserverTerminal observer(
      download_manager(), /*wait_count=*/1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  web_contents()->OnSavePage();
  observer.WaitForFinished();

  download::DownloadItem* download = GetOnlyDownload();
  ASSERT_TRUE(download);
  EXPECT_EQ(extension->origin(), download->GetRequestInitiator());
  EXPECT_FALSE(GetExtensionInitiatorInfo(download).has_value());
}

}  // namespace enterprise_connectors
