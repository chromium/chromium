// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/test/with_feature_override.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_test_file_activity_observer.h"
#include "chrome/browser/pdf/pdf_extension_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/file_system_chooser_test_helpers.h"
#include "content/public/test/hit_test_region_observer.h"
#include "extensions/browser/api/file_system/file_system_api.h"
#include "pdf/pdf_features.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/views/views_switches.h"

using extensions::FileSystemChooseEntryFunction;

namespace {

// Saved file is compared with the expectation after this delay.
constexpr base::TimeDelta kFileComparisonDelay = base::Seconds(1);

// LINT.IfChange(MaxSaveBufferSize)
constexpr uint32_t kMaxSaveBufferSize = 16 * 1000 * 1000;
// LINT.ThenChange(//pdf/pdf_view_web_plugin.cc:MaxSaveBufferSize)

// Filepath used to generate a PDF larger than `kMaxSaveBufferSize` in memory.
constexpr std::string_view kLargeTestCase = "/large_in_memory.pdf";

// Simulate saving the PDF from the context menu "Save As...".
void SimulateContextMenuSaveAs(content::RenderFrameHost* extension_host,
                               const GURL& url) {
  content::ContextMenuParams context_menu_params;
  context_menu_params.media_type =
      blink::mojom::ContextMenuDataMediaType::kPlugin;
  context_menu_params.src_url = url;
  context_menu_params.page_url = url;

  content::WaitForHitTestData(extension_host);
  TestRenderViewContextMenu menu(*extension_host, context_menu_params);
  menu.Init();
  menu.ExecuteCommand(IDC_SAVE_PAGE, 0);
}

// Trigger toolbar download button.
void TriggerDownloadButton(content::RenderFrameHost* extension_host) {
  ASSERT_TRUE(content::ExecJs(
      extension_host,
      "var viewer = document.getElementById('viewer');"
      "var toolbar = viewer.shadowRoot.getElementById('toolbar');"
      "var downloads = toolbar.shadowRoot.getElementById('downloads');"
      "downloads.shadowRoot.getElementById('save').click();"));
}

// Compares content of the saved file and original test file. This function may
// be called while saving is not finished yet. On Windows trying to open the
// file for reading while it is open for writing may result in a failure in
// closing the file after save is finished. Hence file checking is done with a
// delay to reduce the possibility of flakiness, and is repeated for the rare
// cases that saving takes more than the 1s delay.
void CompareFileContent(const base::FilePath& test_file_path,
                        const base::FilePath& save_path,
                        base::OnceCallback<void(void)> callback,
                        bool wait_before_compare) {
  if (!wait_before_compare && base::ContentsEqual(test_file_path, save_path)) {
    std::move(callback).Run();
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CompareFileContent, test_file_path, save_path,
                     std::move(callback), /*wait_before_compare=*/false),
      kFileComparisonDelay);
}

// Generates a PDF larger than kMaxSaveBufferSize bytes.
std::string GetLargePDFContent() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath test_file_path = chrome_test_utils::GetTestFilePath(
      base::FilePath(FILE_PATH_LITERAL("pdf")),
      base::FilePath(FILE_PATH_LITERAL("test.pdf")));
  std::string content;
  EXPECT_TRUE(base::ReadFileToString(test_file_path, &content));
  content.resize(kMaxSaveBufferSize + 1, ' ');
  return content;
}
}  // namespace

// TODO(crbug.com/394111292): Remove this test when `PdfGetSaveDataInBlocks`
// launches.
class PDFExtensionDownloadTest : public base::test::WithFeatureOverride,
                                 public PDFExtensionTestBase {
 public:
  PDFExtensionDownloadTest()
      : base::test::WithFeatureOverride(chrome_pdf::features::kPdfOopif) {}

  std::vector<base::test::FeatureRef> GetDisabledFeatures() const override {
    std::vector<base::test::FeatureRef> disabled =
        PDFExtensionTestBase::GetDisabledFeatures();
    disabled.push_back(chrome_pdf::features::kPdfGetSaveDataInBlocks);
    return disabled;
  }

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
    SimulateContextMenuSaveAs(extension_host, url);
    WaitForDownload(download_waiter.get());
  }

  void SavePdfWithDownloadButton(content::RenderFrameHost* extension_host) {
    auto download_waiter = CreateDownloadWaiter();
    TriggerDownloadButton(extension_host);
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

// Params: kPdfOopif, kPdfUseShowSaveFilePicker
class PDFExtensionSaveInBlocksTest
    : public PDFExtensionTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  PDFExtensionSaveInBlocksTest() = default;

  // PDFExtensionTestBase:
  bool UseOopif() const override { return get<0>(GetParam()); }

  bool IsPdfUseShowSaveFilePickerEnabled() const { return get<1>(GetParam()); }

  // PDFExtensionTestBase:
  void SetUpInProcessBrowserTestFixture() override {
    // Set up the policy provider to allow the file picker to run inside PDF
    // viewer's extension.
    if (IsPdfUseShowSaveFilePickerEnabled()) {
      SetUpPolicyProvider();
      SetPolicy();
    }
    PDFExtensionTestBase::SetUpInProcessBrowserTestFixture();
  }

  // PDFExtensionTestBase:
  void TearDownOnMainThread() override {
    if (!IsPdfUseShowSaveFilePickerEnabled()) {
      test_options_auto_reset_.reset();
      test_options_.reset();
    }
    PDFExtensionTestBase::TearDownOnMainThread();
  }

  // PDFExtensionTestBase:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      const override {
    std::vector<base::test::FeatureRefAndParams> enabled =
        PDFExtensionTestBase::GetEnabledFeatures();
    enabled.push_back({chrome_pdf::features::kPdfGetSaveDataInBlocks, {}});
    if (IsPdfUseShowSaveFilePickerEnabled()) {
      enabled.push_back({chrome_pdf::features::kPdfUseShowSaveFilePicker, {}});
    }
    return enabled;
  }

  // PDFExtensionTestBase:
  std::vector<base::test::FeatureRef> GetDisabledFeatures() const override {
    std::vector<base::test::FeatureRef> disabled =
        PDFExtensionTestBase::GetDisabledFeatures();
    if (!IsPdfUseShowSaveFilePickerEnabled()) {
      disabled.push_back(chrome_pdf::features::kPdfUseShowSaveFilePicker);
    }
    return disabled;
  }

  void SetUpPolicyProvider() {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void SetPolicy() {
    policy::PolicyMap policy_map;
    base::Value::List allowed_origins;
    allowed_origins.Append(
        base::FilePath(ChromeContentClient::kPDFExtensionPluginPath)
            .MaybeAsASCII());
    policy_map.Set(
        policy::key::kFileOrDirectoryPickerWithoutGestureAllowedForOrigins,
        policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
        policy::POLICY_SOURCE_PLATFORM, base::Value(std::move(allowed_origins)),
        nullptr);
    policy_provider_.UpdateChromePolicy(policy_map);
  }

  void SetAutoSelectSavePath(const base::FilePath& save_path) {
    if (IsPdfUseShowSaveFilePickerEnabled()) {
      ui::SelectFileDialog::SetFactory(
          std::make_unique<content::FakeSelectFileDialogFactory>(
              std::vector<base::FilePath>{save_path}));
    } else {
      FileSystemChooseEntryFunction::RegisterTempExternalFileSystemForTest(
          save_path.BaseName().MaybeAsASCII(), save_path.DirName());
      save_path_ = save_path;
      test_options_ =
          std::make_unique<FileSystemChooseEntryFunction::TestOptions>(
              FileSystemChooseEntryFunction::TestOptions(
                  {.path_to_be_picked = &save_path_}));
      test_options_auto_reset_ = std::make_unique<
          base::AutoReset<const FileSystemChooseEntryFunction::TestOptions*>>(
          FileSystemChooseEntryFunction::SetOptionsForTesting(*test_options_));
    }
  }

 private:
  // Needed for auto selecting save path with SaveFilePicker.
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;

  // Needed for auto selecting save path with FileChooser.
  base::FilePath save_path_;
  std::unique_ptr<FileSystemChooseEntryFunction::TestOptions> test_options_;
  std::unique_ptr<
      base::AutoReset<const FileSystemChooseEntryFunction::TestOptions*>>
      test_options_auto_reset_;
};

IN_PROC_BROWSER_TEST_P(PDFExtensionSaveInBlocksTest, BasicUsingContextMenu) {
  const GURL url = embedded_test_server()->GetURL("/pdf/test.pdf");
  content::RenderFrameHost* extension_host = LoadPdfGetExtensionHost(url);
  ASSERT_TRUE(extension_host);

  // Setup fake file selection reply.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath save_path = temp_dir.GetPath().AppendASCII("test.pdf");

  SetAutoSelectSavePath(save_path);
  SimulateContextMenuSaveAs(extension_host, url);

  base::FilePath test_file_path = chrome_test_utils::GetTestFilePath(
      base::FilePath(FILE_PATH_LITERAL("pdf")),
      base::FilePath(FILE_PATH_LITERAL("test.pdf")));
  base::test::TestFuture<void> future;
  CompareFileContent(test_file_path, save_path, future.GetCallback(),
                     /*wait_before_compare=*/true);
  ASSERT_TRUE(future.Wait());
}

IN_PROC_BROWSER_TEST_P(PDFExtensionSaveInBlocksTest, BasicUsingDownloadButton) {
  const GURL url = embedded_test_server()->GetURL("/pdf/test.pdf");
  content::RenderFrameHost* extension_host = LoadPdfGetExtensionHost(url);
  ASSERT_TRUE(extension_host);

  // Setup fake file selection reply.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath save_path = temp_dir.GetPath().AppendASCII("test.pdf");

  SetAutoSelectSavePath(save_path);
  TriggerDownloadButton(extension_host);

  base::FilePath test_file_path = chrome_test_utils::GetTestFilePath(
      base::FilePath(FILE_PATH_LITERAL("pdf")),
      base::FilePath(FILE_PATH_LITERAL("test.pdf")));
  base::test::TestFuture<void> future;
  CompareFileContent(test_file_path, save_path, future.GetCallback(),
                     /*wait_before_compare=*/true);
  ASSERT_TRUE(future.Wait());
}

INSTANTIATE_TEST_SUITE_P(All,
                         PDFExtensionSaveInBlocksTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

// Tests with an in memory generated PDF larger than `kMaxSaveBufferSize` bytes.
class PDFExtensionSaveInBlocksLargeFileTest
    : public PDFExtensionSaveInBlocksTest {
 public:
  PDFExtensionSaveInBlocksLargeFileTest() = default;

  // PDFExtensionTestBase:
  void RegisterTestServerRequestHandler() override {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        [](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.relative_url != kLargeTestCase) {
            return nullptr;
          }
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_content(GetLargePDFContent());
          return response;
        }));
  }
};

IN_PROC_BROWSER_TEST_P(PDFExtensionSaveInBlocksLargeFileTest,
                       UsingDownloadButton) {
  const GURL url = embedded_test_server()->GetURL(kLargeTestCase);
  content::RenderFrameHost* extension_host = LoadPdfGetExtensionHost(url);
  ASSERT_TRUE(extension_host);

  // Setup fake file selection reply.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath save_path = temp_dir.GetPath().AppendASCII("test.pdf");

  SetAutoSelectSavePath(save_path);
  TriggerDownloadButton(extension_host);

  base::FilePath expected_path = temp_dir.GetPath().AppendASCII("expected.pdf");
  EXPECT_TRUE(base::WriteFile(expected_path, GetLargePDFContent()));
  base::test::TestFuture<void> future;
  CompareFileContent(expected_path, save_path, future.GetCallback(),
                     /*wait_before_compare=*/true);
  ASSERT_TRUE(future.Wait());
}

INSTANTIATE_TEST_SUITE_P(All,
                         PDFExtensionSaveInBlocksLargeFileTest,
                         testing::Combine(testing::Bool(), testing::Bool()));
