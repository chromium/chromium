// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_BROWSERTEST_UTILS_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_BROWSERTEST_UTILS_H_

#include <stddef.h>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/download/download_test_file_activity_observer.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/slow_download_http_response.h"
#include "content/public/test/test_download_http_response.h"
#include "content/public/test/test_file_error_injector.h"
#include "extensions/browser/scoped_ignore_content_verifier_for_test.h"
#include "ui/base/window_open_disposition.h"

class DownloadPrefs;

// Gets the download manager for a browser.
content::DownloadManager* DownloadManagerForBrowser(Browser* browser);

// Sets the kPromptForDownload pref on `browser`. Generally this should be used
// with `prompt_for_download` false, as prompting for download location in a
// browser test will make the download time out.
void SetPromptForDownload(Browser* browser, bool prompt_for_download);

// DownloadTestObserver subclass that observes one download until it transitions
// from a non-resumable state to a resumable state a specified number of
// times. Note that this observer can only observe a single download.
class DownloadTestObserverResumable : public content::DownloadTestObserver {
 public:
  // Construct a new observer. |transition_count| is the number of times the
  // download should transition from a non-resumable state to a resumable state.
  DownloadTestObserverResumable(content::DownloadManager* download_manager,
                                size_t transition_count);

  DownloadTestObserverResumable(const DownloadTestObserverResumable&) = delete;
  DownloadTestObserverResumable& operator=(
      const DownloadTestObserverResumable&) = delete;

  ~DownloadTestObserverResumable() override;

 private:
  bool IsDownloadInFinalState(download::DownloadItem* download) override;

  bool was_previously_resumable_ = false;
  size_t transitions_left_;
};

// DownloadTestObserver subclass that observes a download until it transitions
// from IN_PROGRESS to another state, but only after StartObserving() is called.
class DownloadTestObserverNotInProgress : public content::DownloadTestObserver {
 public:
  DownloadTestObserverNotInProgress(content::DownloadManager* download_manager,
                                    size_t count);

  DownloadTestObserverNotInProgress(const DownloadTestObserverNotInProgress&) =
      delete;
  DownloadTestObserverNotInProgress& operator=(
      const DownloadTestObserverNotInProgress&) = delete;

  ~DownloadTestObserverNotInProgress() override;

  void StartObserving();

 private:
  bool IsDownloadInFinalState(download::DownloadItem* download) override;

  bool started_observing_;
};

class DownloadTestBase : public InProcessBrowserTest {
 public:
  // Choice of navigation or direct fetch.  Used by |DownloadFileCheckErrors()|.
  enum DownloadMethod { DOWNLOAD_NAVIGATE, DOWNLOAD_DIRECT };

  // Information passed in to |DownloadFileCheckErrors()|.
  struct DownloadInfo {
    const char* starting_url;           // URL for initiating the download.
    const char* expected_download_url;  // Expected value of DI::GetURL(). Can
                                        // be different if |starting_url|
                                        // initiates a download from another
                                        // URL.
    DownloadMethod download_method;     // Navigation or Direct.
    // Download interrupt reason (NONE is OK).
    download::DownloadInterruptReason reason;
    bool show_download_item;  // True if the download item appears on the shelf.
    bool should_redirect_to_documents;  // True if we save it in "My Documents".
  };

  struct FileErrorInjectInfo {
    DownloadInfo download_info;
    content::TestFileErrorInjector::FileErrorInfo error_info;
  };

  static constexpr char kDownloadTest1Path[] = "download-test1.lib";
#if BUILDFLAG(IS_WIN)
  static constexpr char kDangerousMockFilePath[] =
      "/downloads/dangerous/dangerous.exe";
#elif BUILDFLAG(IS_POSIX)
  // TODO(crbug.com/40800578): Find an actually "dangerous" extension for
  // Fuchsia.
  static constexpr char kDangerousMockFilePath[] =
      "/downloads/dangerous/dangerous.sh";
#endif

  DownloadTestBase();
  ~DownloadTestBase() override;

  void SetUpOnMainThread() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void TearDownOnMainThread() override;

  bool CheckTestDir();

  // Returning false indicates a failure of the setup, and should be asserted
  // in the caller.
  bool InitialSetup();

 protected:
  enum SizeTestType {
    SIZE_TEST_TYPE_KNOWN,
    SIZE_TEST_TYPE_UNKNOWN,
  };

  base::FilePath GetTestDataDirectory();

  // Location of the file source (the place from which it is downloaded).
  base::FilePath OriginFile(const base::FilePath& file);

  // Location of the file destination (place to which it is downloaded).
  base::FilePath DestinationFile(Browser* browser, const base::FilePath& file);

  content::TestDownloadResponseHandler* test_response_handler();

  DownloadPrefs* GetDownloadPrefs(Browser* browser);

  base::FilePath GetDownloadDirectory(Browser* browser);

  // Create a DownloadTestObserverTerminal that will wait for the
  // specified number of downloads to finish.
  content::DownloadTestObserver* CreateWaiter(Browser* browser,
                                              int num_downloads);

  // Create a DownloadTestObserverInProgress that will wait for the
  // specified number of downloads to start.
  content::DownloadTestObserver* CreateInProgressWaiter(Browser* browser,
                                                        int num_downloads);

  // Create a DownloadTestObserverTerminal that will wait for the
  // specified number of downloads to finish, or for
  // a dangerous download warning to be shown.
  content::DownloadTestObserver* DangerousDownloadWaiter(
      Browser* browser,
      int num_downloads,
      content::DownloadTestObserver::DangerousDownloadAction
          dangerous_download_action);

  void CheckDownloadStatesForBrowser(
      Browser* browser,
      size_t num,
      download::DownloadItem::DownloadState state);

  void CheckDownloadStates(size_t num,
                           download::DownloadItem::DownloadState state);

  bool VerifyNoDownloads() const;

  // Download |url|, then wait for the download to finish.
  // |disposition| indicates where the navigation occurs (current tab, new
  // foreground tab, etc).
  // |browser_test_flags| indicate what to wait for, and is an OR of 0 or more
  // values in the ui_test_utils::BrowserTestWaitFlags enum.
  // |prompt_for_download| indicates whether to prompt for the download location
  // and should generally be false, since the download location prompt can
  // cause the browser test to time out.
  void DownloadAndWaitWithDisposition(Browser* browser,
                                      const GURL& url,
                                      WindowOpenDisposition disposition,
                                      int browser_test_flags,
                                      bool prompt_for_download = false);

  // Download a file in the current tab, then wait for the download to finish.
  void DownloadAndWait(Browser* browser,
                       const GURL& url,
                       bool prompt_for_download = false);

  // Should only be called when the download is known to have finished
  // (in error or not).
  // Returning false indicates a failure of the function, and should be asserted
  // in the caller.
  bool CheckDownload(Browser* browser,
                     const base::FilePath& downloaded_filename,
                     const base::FilePath& origin_filename);

  // A version of CheckDownload that allows complete path specification.
  bool CheckDownloadFullPaths(Browser* browser,
                              const base::FilePath& downloaded_file,
                              const base::FilePath& origin_file);

  // Creates an in-progress download and returns a pointer to its DownloadItem.
  // Either supply a `browser` or the `browser()` in the test fixture will be
  // used.
  download::DownloadItem* CreateSlowTestDownload(Browser* browser = nullptr);

  bool RunSizeTest(Browser* browser,
                   SizeTestType type,
                   const std::string& partial_indication,
                   const std::string& total_indication);

  void GetDownloads(
      Browser* browser,
      std::vector<raw_ptr<download::DownloadItem, VectorExperimental>>*
          downloads) const;

  static void ExpectWindowCountAfterDownload(size_t expected);

  void EnableFileChooser(bool enable);
  bool DidShowFileChooser();

  // Checks that |path| is has |file_size| bytes, and matches the |value|
  // string.
  bool VerifyFile(const base::FilePath& path,
                  const std::string& value,
                  const int64_t file_size);

  // Attempts to download a file, based on information in |download_info|.
  // If a Select File dialog opens, will automatically choose the default.
  void DownloadFilesCheckErrorsSetup();

  // Attempts to download a set of files, based on information in the
  // |download_info| array.  |count| is the number of files.
  // If a Select File dialog appears, it will choose the default and return
  // immediately.
  void DownloadFilesCheckErrors(size_t count, DownloadInfo* download_info);
  void DownloadFilesCheckErrorsLoopBody(const DownloadInfo& download_info,
                                        size_t i);

  void DownloadInsertFilesErrorCheckErrors(size_t count,
                                           FileErrorInjectInfo* info);
  void DownloadInsertFilesErrorCheckErrorsLoopBody(
      scoped_refptr<content::TestFileErrorInjector> injector,
      const FileErrorInjectInfo& info,
      size_t i);

  // Attempts to download a file to a read-only folder, based on information
  // in |download_info|.
  void DownloadFilesToReadonlyFolder(size_t count, DownloadInfo* download_info);

  // This method:
  // * Starts a mock download by navigating to embedded test server URL.
  // * Injects |error| on the first write using |error_injector|.
  // * Waits for the download to be interrupted.
  // * Clears the errors on |error_injector|.
  // * Returns the resulting interrupted download.
  download::DownloadItem* StartMockDownloadAndInjectError(
      content::TestFileErrorInjector* error_injector,
      download::DownloadInterruptReason error);

  // Provide equivalent to embedded_test_server() with a variant that uses HTTPS
  // to avoid insecure download warnings.
  net::EmbeddedTestServer* https_test_server() {
    return https_test_server_.get();
  }

 private:
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;

  // Location of the test data.
  base::FilePath test_dir_;

  content::TestDownloadResponseHandler test_response_handler_;
  std::unique_ptr<DownloadTestFileActivityObserver> file_activity_observer_;
  extensions::ScopedIgnoreContentVerifierForTest ignore_content_verifier_;
  extensions::ScopedInstallVerifierBypassForTest ignore_install_verification_;

  // By default, the embedded test server uses HTTP. Keep an HTTPS server
  // as well so that we can avoid unexpected insecure download warnings.
  std::unique_ptr<net::EmbeddedTestServer> https_test_server_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_BROWSERTEST_UTILS_H_
