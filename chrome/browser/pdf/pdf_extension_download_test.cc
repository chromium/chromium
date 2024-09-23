// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/test/with_feature_override.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_test_file_activity_observer.h"
#include "chrome/browser/pdf/pdf_extension_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/hit_test_region_observer.h"
#include "pdf/pdf_features.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "ui/views/views_switches.h"

class PDFExtensionDownloadTest : public base::test::WithFeatureOverride,
                                 public PDFExtensionTestBase {
 public:
  PDFExtensionDownloadTest()
      : base::test::WithFeatureOverride(chrome_pdf::features::kPdfOopif) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PDFExtensionTestBase::SetUpCommandLine(command_line);

    // Clicks from tests should always be allowed, even on dialogs that have
    // protection against accidental double-clicking/etc.
    command_line->AppendSwitch(
        views::switches::kDisableInputEventActivationProtectionForTesting);
  }

  void SetUpOnMainThread() override {
    PDFExtensionTestBase::SetUpOnMainThread();

    browser()->profile()->GetPrefs()->SetBoolean(prefs::kPromptForDownload,
                                                 false);
    DownloadPrefs::FromDownloadManager(GetDownloadManager())
        ->ResetAutoOpenByUser();

    file_activity_observer_ =
        std::make_unique<DownloadTestFileActivityObserver>(
            browser()->profile());
    file_activity_observer_->EnableFileChooser(true);
  }

  void TearDownOnMainThread() override {
    // Needs to be torn down on the main thread. `file_activity_observer_` holds
    // a reference to the ChromeDownloadManagerDelegate which should be
    // destroyed on the UI thread.
    file_activity_observer_.reset();

    PDFExtensionTestBase::TearDownOnMainThread();
  }

  bool UseOopif() const override { return GetParam(); }

  void SavePdfFromExtensionFrameContextMenu(
      content::RenderFrameHost* extension_host,
      const GURL& url) {
    auto download_waiter = CreateDownloadWaiter();

    // Simulate saving the PDF from the context menu "Save As...".
    content::ContextMenuParams context_menu_params;
    context_menu_params.media_type =
        blink::mojom::ContextMenuDataMediaType::kPlugin;
    context_menu_params.src_url = url;
    context_menu_params.page_url = url;

    content::WaitForHitTestData(extension_host);
    TestRenderViewContextMenu menu(*extension_host, context_menu_params);
    menu.Init();
    menu.ExecuteCommand(IDC_SAVE_PAGE, 0);

    WaitForDownload(download_waiter.get());
  }

  void SavePdfWithDownloadButton(content::RenderFrameHost* extension_host) {
    auto download_waiter = CreateDownloadWaiter();

    ASSERT_TRUE(content::ExecJs(
        extension_host,
        "var viewer = document.getElementById('viewer');"
        "var toolbar = viewer.shadowRoot.getElementById('toolbar');"
        "var downloads = toolbar.shadowRoot.getElementById('downloads');"
        "downloads.shadowRoot.getElementById('download').click();"));

    WaitForDownload(download_waiter.get());
  }

  void CheckDownloadedFile(const base::FilePath::StringType& expected_name) {
    content::DownloadManager::DownloadVector downloads;
    GetDownloadManager()->GetAllDownloads(&downloads);
    ASSERT_EQ(1u, downloads.size());
    EXPECT_EQ(expected_name,
              downloads[0]->GetTargetFilePath().BaseName().value());
  }

 private:
  content::DownloadManager* GetDownloadManager() {
    return browser()->profile()->GetDownloadManager();
  }

  std::unique_ptr<content::DownloadTestObserver> CreateDownloadWaiter() {
    return std::make_unique<content::DownloadTestObserverTerminal>(
        GetDownloadManager(), /*wait_count=*/1,
        content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  }

  void WaitForDownload(content::DownloadTestObserver* download_waiter) {
    download_waiter->WaitForFinished();
    EXPECT_EQ(1u, download_waiter->NumDownloadsSeenInState(
                      download::DownloadItem::COMPLETE));
  }

  std::unique_ptr<DownloadTestFileActivityObserver> file_activity_observer_;
};

IN_PROC_BROWSER_TEST_P(PDFExtensionDownloadTest, BasicUsingContextMenu) {
  const GURL url = embedded_test_server()->GetURL("/pdf/test.pdf");
  content::RenderFrameHost* extension_host = LoadPdfGetExtensionHost(url);
  ASSERT_TRUE(extension_host);
  SavePdfFromExtensionFrameContextMenu(extension_host, url);
  CheckDownloadedFile(FILE_PATH_LITERAL("test.pdf"));
}

IN_PROC_BROWSER_TEST_P(PDFExtensionDownloadTest, BasicUsingDownloadButton) {
  const GURL url = embedded_test_server()->GetURL("/pdf/test.pdf");
  content::RenderFrameHost* extension_host = LoadPdfGetExtensionHost(url);
  ASSERT_TRUE(extension_host);
  SavePdfWithDownloadButton(extension_host);
  CheckDownloadedFile(FILE_PATH_LITERAL("test.pdf"));
}

// TODO(crbug.com/40268279): Stop testing both modes after OOPIF PDF viewer
// launches.
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFExtensionDownloadTest);
