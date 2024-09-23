// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/download/download_browsertest_utils.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_file_util.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/views/views_switches.h"

using content::DownloadManager;
using content::WebContents;
using download::DownloadItem;
using download::DownloadUrlParameters;
using extensions::Extension;

DownloadManager* DownloadManagerForBrowser(Browser* browser) {
  return browser->profile()->GetDownloadManager();
}

void SetPromptForDownload(Browser* browser, bool prompt_for_download) {
  browser->profile()->GetPrefs()->SetBoolean(prefs::kPromptForDownload,
                                             prompt_for_download);
}

DownloadTestObserverResumable::DownloadTestObserverResumable(
    DownloadManager* download_manager,
    size_t transition_count)
    : DownloadTestObserver(download_manager, 1, ON_DANGEROUS_DOWNLOAD_FAIL),
      transitions_left_(transition_count) {
  Init();
}

DownloadTestObserverResumable::~DownloadTestObserverResumable() = default;

bool DownloadTestObserverResumable::IsDownloadInFinalState(
    DownloadItem* download) {
  bool is_resumable_now = download->CanResume();
  if (!was_previously_resumable_ && is_resumable_now) {
    --transitions_left_;
  }
  was_previously_resumable_ = is_resumable_now;
  return transitions_left_ == 0;
}

DownloadTestObserverNotInProgress::DownloadTestObserverNotInProgress(
    DownloadManager* download_manager,
    size_t count)
    : DownloadTestObserver(download_manager, count, ON_DANGEROUS_DOWNLOAD_FAIL),
      started_observing_(false) {
  Init();
}

DownloadTestObserverNotInProgress::~DownloadTestObserverNotInProgress() =
    default;

void DownloadTestObserverNotInProgress::StartObserving() {
  started_observing_ = true;
}

bool DownloadTestObserverNotInProgress::IsDownloadInFinalState(
    DownloadItem* download) {
  return started_observing_ &&
         download->GetState() != DownloadItem::IN_PROGRESS;
}

DownloadTestBase::DownloadTestBase() = default;

DownloadTestBase::~DownloadTestBase() = default;

void DownloadTestBase::SetUpOnMainThread() {
  ASSERT_TRUE(CheckTestDir());
  ASSERT_TRUE(InitialSetup());

  https_test_server_ = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);

  host_resolver()->AddRule("www.a.com", "127.0.0.1");
  host_resolver()->AddRule("www.a.test", "127.0.0.1");
  host_resolver()->AddRule("www.b.test", "127.0.0.1");
  host_resolver()->AddRule("a.test", "127.0.0.1");
  host_resolver()->AddRule("b.test", "127.0.0.1");
  host_resolver()->AddRule("foo.com", "127.0.0.1");
  host_resolver()->AddRule("bar.com", "127.0.0.1");
  content::SetupCrossSiteRedirector(embedded_test_server());
}

void DownloadTestBase::SetUpCommandLine(base::CommandLine* command_line) {
  // Slower builders (linux-chromeos-rel, debug, and maybe others) are flaky
  // due to slower loading interacting with deferred commits.
  command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);

  // Clicks from tests should always be allowed, even on dialogs that have
  // protection against accidental double-clicking/etc.
  command_line->AppendSwitch(
      views::switches::kDisableInputEventActivationProtectionForTesting);
}

void DownloadTestBase::TearDownOnMainThread() {
  // Needs to be torn down on the main thread. file_activity_observer_ holds a
  // reference to the ChromeDownloadManagerDelegate which should be destroyed
  // on the UI thread.
  file_activity_observer_.reset();
}

bool DownloadTestBase::CheckTestDir() {
  bool have_test_dir =
      base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir_);
  EXPECT_TRUE(have_test_dir);
  return have_test_dir;
}

bool DownloadTestBase::InitialSetup() {
  // Sanity check default values for window and tab count.
  int window_count = chrome::GetTotalBrowserCount();
  EXPECT_EQ(1, window_count);
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  SetPromptForDownload(browser(), false);

  DownloadManager* manager = DownloadManagerForBrowser(browser());
  DownloadPrefs::FromDownloadManager(manager)->ResetAutoOpenByUser();

  file_activity_observer_ =
      std::make_unique<DownloadTestFileActivityObserver>(browser()->profile());

  return true;
}

base::FilePath DownloadTestBase::GetTestDataDirectory() {
  base::FilePath test_file_directory;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_file_directory);
  return test_file_directory;
}

base::FilePath DownloadTestBase::OriginFile(const base::FilePath& file) {
  return test_dir_.Append(file);
}

base::FilePath DownloadTestBase::DestinationFile(Browser* browser,
                                                 const base::FilePath& file) {
  return GetDownloadDirectory(browser).Append(file.BaseName());
}

content::TestDownloadResponseHandler*
DownloadTestBase::test_response_handler() {
  return &test_response_handler_;
}

DownloadPrefs* DownloadTestBase::GetDownloadPrefs(Browser* browser) {
  return DownloadPrefs::FromDownloadManager(DownloadManagerForBrowser(browser));
}

base::FilePath DownloadTestBase::GetDownloadDirectory(Browser* browser) {
  return GetDownloadPrefs(browser)->DownloadPath();
}

content::DownloadTestObserver* DownloadTestBase::CreateWaiter(
    Browser* browser,
    int num_downloads) {
  DownloadManager* download_manager = DownloadManagerForBrowser(browser);
  return new content::DownloadTestObserverTerminal(
      download_manager, num_downloads,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
}

content::DownloadTestObserver* DownloadTestBase::CreateInProgressWaiter(
    Browser* browser,
    int num_downloads) {
  DownloadManager* download_manager = DownloadManagerForBrowser(browser);
  return new content::DownloadTestObserverInProgress(download_manager,
                                                     num_downloads);
}

content::DownloadTestObserver* DownloadTestBase::DangerousDownloadWaiter(
    Browser* browser,
    int num_downloads,
    content::DownloadTestObserver::DangerousDownloadAction
        dangerous_download_action) {
  DownloadManager* download_manager = DownloadManagerForBrowser(browser);
  return new content::DownloadTestObserverTerminal(
      download_manager, num_downloads, dangerous_download_action);
}

void DownloadTestBase::CheckDownloadStatesForBrowser(
    Browser* browser,
    size_t num,
    DownloadItem::DownloadState state) {
  std::vector<raw_ptr<DownloadItem, VectorExperimental>> download_items;
  GetDownloads(browser, &download_items);

  EXPECT_EQ(num, download_items.size());

  for (size_t i = 0; i < download_items.size(); ++i) {
    EXPECT_EQ(state, download_items[i]->GetState()) << " Item " << i;
  }
}

void DownloadTestBase::CheckDownloadStates(size_t num,
                                           DownloadItem::DownloadState state) {
  CheckDownloadStatesForBrowser(browser(), num, state);
}

bool DownloadTestBase::VerifyNoDownloads() const {
  DownloadManager::DownloadVector items;
  GetDownloads(browser(), &items);
  return items.empty();
}

void DownloadTestBase::DownloadAndWaitWithDisposition(
    Browser* browser,
    const GURL& url,
    WindowOpenDisposition disposition,
    int browser_test_flags,
    bool prompt_for_download) {
  // Setup notification, navigate, and block.
  std::unique_ptr<content::DownloadTestObserver> observer(
      CreateWaiter(browser, 1));
  SetPromptForDownload(browser, prompt_for_download);
  // This call will block until the condition specified by
  // |browser_test_flags|, but will not wait for the download to finish.
  ui_test_utils::NavigateToURLWithDisposition(browser, url, disposition,
                                              browser_test_flags);
  // Waits for the download to complete.
  observer->WaitForFinished();
  EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  // We don't expect a file chooser to be shown.
  EXPECT_FALSE(DidShowFileChooser());
}

void DownloadTestBase::DownloadAndWait(Browser* browser,
                                       const GURL& url,
                                       bool prompt_for_download) {
  DownloadAndWaitWithDisposition(
      browser, url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP, prompt_for_download);
}

bool DownloadTestBase::CheckDownload(Browser* browser,
                                     const base::FilePath& downloaded_filename,
                                     const base::FilePath& origin_filename) {
  // Find the path to which the data will be downloaded.
  base::FilePath downloaded_file(DestinationFile(browser, downloaded_filename));

  // Find the origin path (from which the data comes).
  base::FilePath origin_file(OriginFile(origin_filename));
  return CheckDownloadFullPaths(browser, downloaded_file, origin_file);
}

bool DownloadTestBase::CheckDownloadFullPaths(
    Browser* browser,
    const base::FilePath& downloaded_file,
    const base::FilePath& origin_file) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  bool origin_file_exists = base::PathExists(origin_file);
  EXPECT_TRUE(origin_file_exists) << origin_file.value();
  if (!origin_file_exists) {
    return false;
  }

  // Confirm the downloaded data file exists.
  bool downloaded_file_exists = base::PathExists(downloaded_file);
  EXPECT_TRUE(downloaded_file_exists) << downloaded_file.value();
  if (!downloaded_file_exists) {
    return false;
  }

  int64_t origin_file_size = 0;
  EXPECT_TRUE(base::GetFileSize(origin_file, &origin_file_size));
  std::string original_file_contents;
  EXPECT_TRUE(base::ReadFileToString(origin_file, &original_file_contents));
  EXPECT_TRUE(
      VerifyFile(downloaded_file, original_file_contents, origin_file_size));

  // Delete the downloaded copy of the file.
  bool downloaded_file_deleted = base::DieFileDie(downloaded_file, false);
  EXPECT_TRUE(downloaded_file_deleted);
  return downloaded_file_deleted;
}

DownloadItem* DownloadTestBase::CreateSlowTestDownload(Browser* browser) {
  if (!browser) {
    browser = DownloadTestBase::browser();
  }
  DownloadManager* manager = DownloadManagerForBrowser(browser);

  std::unique_ptr<content::DownloadTestObserver> observer =
      std::make_unique<content::DownloadTestObserverInProgress>(manager, 1);
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &content::SlowDownloadHttpResponse::HandleSlowDownloadRequest));
  EXPECT_TRUE(embedded_test_server()->Start());
  GURL slow_download_url = embedded_test_server()->GetURL(
      content::SlowDownloadHttpResponse::kKnownSizeUrl);

  EXPECT_EQ(0, manager->BlockingShutdownCount());
  EXPECT_EQ(0, manager->InProgressCount());
  if (manager->InProgressCount() != 0) {
    return nullptr;
  }

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser, slow_download_url));

  observer->WaitForFinished();
  EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::IN_PROGRESS));

  DownloadManager::DownloadVector items;
  manager->GetAllDownloads(&items);

  DownloadItem* new_item = nullptr;
  for (download::DownloadItem* item : items) {
    if (item->GetState() == DownloadItem::IN_PROGRESS) {
      // There should be only one IN_PROGRESS item.
      EXPECT_FALSE(new_item);
      new_item = item;
    }
  }
  return new_item;
}

bool DownloadTestBase::RunSizeTest(Browser* browser,
                                   SizeTestType type,
                                   const std::string& partial_indication,
                                   const std::string& total_indication) {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &content::SlowDownloadHttpResponse::HandleSlowDownloadRequest));
  EXPECT_TRUE(embedded_test_server()->Start());

  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(type == SIZE_TEST_TYPE_UNKNOWN || type == SIZE_TEST_TYPE_KNOWN);
  if (type != SIZE_TEST_TYPE_KNOWN && type != SIZE_TEST_TYPE_UNKNOWN) {
    return false;
  }
  GURL url(type == SIZE_TEST_TYPE_KNOWN
               ? embedded_test_server()->GetURL(
                     content::SlowDownloadHttpResponse::kKnownSizeUrl)
               : embedded_test_server()->GetURL(
                     content::SlowDownloadHttpResponse::kUnknownSizeUrl));
  GURL finish_url = embedded_test_server()->GetURL(
      content::SlowDownloadHttpResponse::kFinishSlowResponseUrl);

  // TODO(ahendrickson) -- |expected_title_in_progress| and
  // |expected_title_finished| need to be checked.
  base::FilePath filename = base::FilePath::FromUTF8Unsafe(url.path());
  std::u16string expected_title_in_progress(
      base::ASCIIToUTF16(partial_indication) + filename.LossyDisplayName());
  std::u16string expected_title_finished(base::ASCIIToUTF16(total_indication) +
                                         filename.LossyDisplayName());

  // Download a partial web page in a background tab and wait.
  // The mock system will not complete until it gets a special URL.
  std::unique_ptr<content::DownloadTestObserver> observer(
      CreateWaiter(browser, 1));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser, url));

  // TODO(ahendrickson): check download status text before downloading.
  // Need to:
  //  - Add a member function to the |DownloadShelf| interface class, that
  //    indicates how many members it has.
  //  - Add a member function to |DownloadShelf| to get the status text
  //    of a given member (for example, via the name in |DownloadItemView|'s
  //    GetAccessibleNodeData() member function), by index.
  //  - Iterate over browser->window()->GetDownloadShelf()'s members
  //    to see if any match the status text we want.  Start with the last one.

  // Allow the request to finish.  We do this by loading a second URL in a
  // separate tab.

  ui_test_utils::NavigateToURLWithDisposition(
      browser, finish_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  observer->WaitForFinished();
  EXPECT_EQ(1u, observer->NumDownloadsSeenInState(DownloadItem::COMPLETE));
  CheckDownloadStatesForBrowser(browser, 1, DownloadItem::COMPLETE);

  EXPECT_EQ(2, browser->tab_strip_model()->count());

  // TODO(ahendrickson): check download status text after downloading.

  base::FilePath download_path =
      GetDownloadDirectory(browser).Append(filename.BaseName());

  bool downloaded_path_exists = base::PathExists(download_path);
  EXPECT_TRUE(downloaded_path_exists);
  if (!downloaded_path_exists) {
    return false;
  }

  // Check the file contents.
  size_t file_size = content::SlowDownloadHttpResponse::kFirstResponsePartSize +
                     content::SlowDownloadHttpResponse::kSecondResponsePartSize;
  std::string expected_contents(file_size, '*');
  EXPECT_TRUE(VerifyFile(download_path, expected_contents, file_size));

  // Delete the file we just downloaded.
  EXPECT_TRUE(base::DieFileDie(download_path, false));
  EXPECT_FALSE(base::PathExists(download_path));

  return true;
}

void DownloadTestBase::GetDownloads(
    Browser* browser,
    std::vector<raw_ptr<DownloadItem, VectorExperimental>>* downloads) const {
  DCHECK(downloads);
  DownloadManager* manager = DownloadManagerForBrowser(browser);
  manager->GetAllDownloads(downloads);
}

// static
void DownloadTestBase::ExpectWindowCountAfterDownload(size_t expected) {
  EXPECT_EQ(expected, chrome::GetTotalBrowserCount());
}

void DownloadTestBase::EnableFileChooser(bool enable) {
  file_activity_observer_->EnableFileChooser(enable);
}

bool DownloadTestBase::DidShowFileChooser() {
  return file_activity_observer_->TestAndResetDidShowFileChooser();
}

bool DownloadTestBase::VerifyFile(const base::FilePath& path,
                                  const std::string& value,
                                  const int64_t file_size) {
  std::string file_contents;

  base::ScopedAllowBlockingForTesting allow_blocking;
  bool read = base::ReadFileToString(path, &file_contents);
  EXPECT_TRUE(read) << "Failed reading file: " << path.value() << std::endl;
  if (!read) {
    return false;  // Couldn't read the file.
  }

  // Note: we don't handle really large files (more than size_t can hold)
  // so we will fail in that case.
  size_t expected_size = static_cast<size_t>(file_size);

  // Check the size.
  EXPECT_EQ(expected_size, file_contents.size());
  if (expected_size != file_contents.size()) {
    return false;
  }

  // Check the contents.
  EXPECT_EQ(value, file_contents);
  if (memcmp(file_contents.c_str(), value.c_str(), expected_size) != 0) {
    return false;
  }

  return true;
}

void DownloadTestBase::DownloadFilesCheckErrorsSetup() {
  embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
  ASSERT_TRUE(embedded_test_server()->Start());
  std::vector<raw_ptr<DownloadItem, VectorExperimental>> download_items;
  GetDownloads(browser(), &download_items);
  ASSERT_TRUE(download_items.empty());

  EnableFileChooser(true);
}

void DownloadTestBase::DownloadFilesCheckErrorsLoopBody(
    const DownloadInfo& download_info,
    size_t i) {
  SCOPED_TRACE(testing::Message()
               << " " << __FUNCTION__ << "()"
               << " index = " << i << " starting_url = '"
               << download_info.starting_url << "'"
               << " download_url = '" << download_info.expected_download_url
               << "'"
               << " method = "
               << ((download_info.download_method == DOWNLOAD_DIRECT)
                       ? "DOWNLOAD_DIRECT"
                       : "DOWNLOAD_NAVIGATE")
               << " show_item = " << download_info.show_download_item
               << " reason = "
               << DownloadInterruptReasonToString(download_info.reason));

  std::vector<raw_ptr<DownloadItem, VectorExperimental>> download_items;
  GetDownloads(browser(), &download_items);
  size_t downloads_expected = download_items.size();

  // GURL("http://foo/bar").Resolve("baz") => "http://foo/bar/baz"
  // GURL("http://foo/bar").Resolve("http://baz") => "http://baz"
  // I.e. both starting_url and expected_download_url can either be relative
  // to the base test server URL or be an absolute URL.
  GURL base_url = embedded_test_server()->GetURL("/downloads/");
  GURL starting_url = base_url.Resolve(download_info.starting_url);
  GURL download_url = base_url.Resolve(download_info.expected_download_url);
  ASSERT_TRUE(starting_url.is_valid());
  ASSERT_TRUE(download_url.is_valid());

  DownloadManager* download_manager = DownloadManagerForBrowser(browser());
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  std::unique_ptr<content::DownloadTestObserver> observer;
  if (download_info.reason == download::DOWNLOAD_INTERRUPT_REASON_NONE) {
    observer = std::make_unique<content::DownloadTestObserverTerminal>(
        download_manager, 1,
        content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  } else {
    observer = std::make_unique<content::DownloadTestObserverInterrupted>(
        download_manager, 1,
        content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  }

  if (download_info.download_method == DOWNLOAD_DIRECT) {
    // Go directly to download.  Don't wait for navigation.
    scoped_refptr<content::DownloadTestItemCreationObserver> creation_observer(
        new content::DownloadTestItemCreationObserver);

    std::unique_ptr<DownloadUrlParameters> params(
        content::DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
            web_contents, starting_url, TRAFFIC_ANNOTATION_FOR_TESTS));
    params->set_callback(creation_observer->callback());
    DownloadManagerForBrowser(browser())->DownloadUrl(std::move(params));

    // Wait until the item is created, or we have determined that it
    // won't be.
    creation_observer->WaitForDownloadItemCreation();

    EXPECT_NE(download::DownloadItem::kInvalidId,
              creation_observer->download_id());
  } else {
    // Navigate to URL normally, wait until done.
    ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
                                                              starting_url, 1);
  }

  if (download_info.show_download_item) {
    downloads_expected++;
    observer->WaitForFinished();
    DownloadItem::DownloadState final_state =
        (download_info.reason == download::DOWNLOAD_INTERRUPT_REASON_NONE)
            ? DownloadItem::COMPLETE
            : DownloadItem::INTERRUPTED;
    EXPECT_EQ(1u, observer->NumDownloadsSeenInState(final_state));
  }

  // Wait till the |DownloadFile|s are destroyed.
  content::RunAllTasksUntilIdle();

  // Validate that the correct files were downloaded.
  download_items.clear();
  GetDownloads(browser(), &download_items);
  ASSERT_EQ(downloads_expected, download_items.size());

  if (download_info.show_download_item) {
    // Find the last download item.
    DownloadItem* item = download_items[0];
    for (size_t d = 1; d < downloads_expected; ++d) {
      if (download_items[d]->GetStartTime() > item->GetStartTime()) {
        item = download_items[d];
      }
    }

    EXPECT_EQ(download_url, item->GetURL());
    EXPECT_EQ(download_info.reason, item->GetLastReason());

    if (item->GetState() == download::DownloadItem::COMPLETE) {
      // Clean up the file, in case it ended up in the My Documents folder.
      base::ScopedAllowBlockingForTesting allow_blocking;
      base::FilePath destination_folder = GetDownloadDirectory(browser());
      base::FilePath my_downloaded_file = item->GetTargetFilePath();
      EXPECT_TRUE(base::PathExists(my_downloaded_file));
      EXPECT_TRUE(base::DeleteFile(my_downloaded_file));
      item->Remove();

      EXPECT_EQ(
          download_info.should_redirect_to_documents ? std::string::npos : 0u,
          my_downloaded_file.value().find(destination_folder.value()));
      if (download_info.should_redirect_to_documents) {
        // If it's not where we asked it to be, it should be in the
        // My Documents folder.
        base::FilePath my_docs_folder;
        EXPECT_TRUE(base::PathService::Get(chrome::DIR_USER_DOCUMENTS,
                                           &my_docs_folder));
        EXPECT_EQ(0u, my_downloaded_file.value().find(my_docs_folder.value()));
      }
    }
  }
}

void DownloadTestBase::DownloadFilesCheckErrors(size_t count,
                                                DownloadInfo* download_info) {
  DownloadFilesCheckErrorsSetup();

  for (size_t i = 0; i < count; ++i) {
    DownloadFilesCheckErrorsLoopBody(download_info[i], i);
  }
}

void DownloadTestBase::DownloadInsertFilesErrorCheckErrorsLoopBody(
    scoped_refptr<content::TestFileErrorInjector> injector,
    const FileErrorInjectInfo& info,
    size_t i) {
  SCOPED_TRACE(
      ::testing::Message()
      << " " << __FUNCTION__ << "()"
      << " index = " << i << " operation code = "
      << content::TestFileErrorInjector::DebugString(info.error_info.code)
      << " instance = " << info.error_info.operation_instance << " error = "
      << download::DownloadInterruptReasonToString(info.error_info.error));

  injector->InjectError(info.error_info);

  DownloadFilesCheckErrorsLoopBody(info.download_info, i);

  size_t expected_successes = info.download_info.show_download_item ? 1u : 0u;
  EXPECT_EQ(expected_successes, injector->TotalFileCount());
  EXPECT_EQ(0u, injector->CurrentFileCount());
}

void DownloadTestBase::DownloadInsertFilesErrorCheckErrors(
    size_t count,
    FileErrorInjectInfo* info) {
  DownloadFilesCheckErrorsSetup();

  // Set up file failures.
  scoped_refptr<content::TestFileErrorInjector> injector(
      content::TestFileErrorInjector::Create(
          DownloadManagerForBrowser(browser())));

  for (size_t i = 0; i < count; ++i) {
    DownloadInsertFilesErrorCheckErrorsLoopBody(injector, info[i], i);
  }
}

// Attempts to download a file to a read-only folder, based on information
// in |download_info|.
void DownloadTestBase::DownloadFilesToReadonlyFolder(
    size_t count,
    DownloadInfo* download_info) {
  DownloadFilesCheckErrorsSetup();

  // Make the test folder unwritable.
  base::FilePath destination_folder = GetDownloadDirectory(browser());
  DVLOG(1) << " " << __FUNCTION__ << "()"
           << " folder = '" << destination_folder.value() << "'";
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePermissionRestorer permission_restorer(destination_folder);
  EXPECT_TRUE(base::MakeFileUnwritable(destination_folder));

  for (size_t i = 0; i < count; ++i) {
    DownloadFilesCheckErrorsLoopBody(download_info[i], i);
  }
}

DownloadItem* DownloadTestBase::StartMockDownloadAndInjectError(
    content::TestFileErrorInjector* error_injector,
    download::DownloadInterruptReason error) {
  content::TestFileErrorInjector::FileErrorInfo error_info;
  error_info.code = content::TestFileErrorInjector::FILE_OPERATION_WRITE;
  error_info.operation_instance = 0;
  error_info.error = error;
  error_injector->InjectError(error_info);

  std::unique_ptr<content::DownloadTestObserver> observer(
      new DownloadTestObserverResumable(DownloadManagerForBrowser(browser()),
                                        1));

  if (!embedded_test_server()->Started()) {
    embedded_test_server()->ServeFilesFromDirectory(GetTestDataDirectory());
    EXPECT_TRUE(embedded_test_server()->Start());
  }

  GURL url =
      embedded_test_server()->GetURL("/" + std::string(kDownloadTest1Path));
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  observer->WaitForFinished();

  content::DownloadManager::DownloadVector downloads;
  DownloadManagerForBrowser(browser())->GetAllDownloads(&downloads);
  EXPECT_EQ(1u, downloads.size());

  if (downloads.size() != 1) {
    return nullptr;
  }

  error_injector->ClearError();
  DownloadItem* download = downloads[0];
  EXPECT_EQ(DownloadItem::INTERRUPTED, download->GetState());
  EXPECT_EQ(error, download->GetLastReason());
  return download;
}
