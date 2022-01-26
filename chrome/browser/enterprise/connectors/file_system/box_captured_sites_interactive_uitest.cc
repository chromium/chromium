// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/browsertest_helper.h"

#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/internal/enterprise_connectors_interactive_uitest_test_accounts.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/ui/views/download/download_item_view.h"
#include "chrome/browser/ui/views/download/download_shelf_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/test_switches.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/history/core/browser/download_row.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/common/content_switches.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/network_switches.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const int kHostHttpPort = 8080;
const int kHostHttpsPort = 8081;

// The public key hash for the certificate Web Page Replay (WPR) uses to serve
// HTTPS content.
// The Captured Sites Test Framework relies on WPR to serve captured site
// traffic. If a machine does not have the WPR certificate installed, Chrome
// will detect a server certificate validation failure when WPR serves Chrome
// HTTPS content. In response Chrome will block the WPR HTTPS content.
// The test framework avoids this problem by launching Chrome with the
// ignore-certificate-errors-spki-list flag set to the WPR certificate's
// public key hash. Doing so tells Chrome to ignore server certificate
// validation errors from WPR.
const char kWebPageReplayCertSPKI[] =
    "PhrPvGIaAMmd29hj8BCZOq096yj7uMpRNHpn5PDxI6I=";

// Determine the test execution mode.
// By default, test should execute against captured web traffic.
// To generate/refresh captures for test, one can use the record mode to
// test against the live Box.com.
// For debugging, one can run in live mode, which executes the test
// against live traffic but without recording.
const char kRecordMode[] = "record";
const char kLiveMode[] = "live";

enum TestExecutionMode { kReplay, kRecord, kLive };

TestExecutionMode GetTestExecutionMode() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kRecordMode))
    return TestExecutionMode::kRecord;
  else if (command_line->HasSwitch(kLiveMode))
    return TestExecutionMode::kLive;
  return TestExecutionMode::kReplay;
}

// Print WPR output.
// Used for debugging WPR behavior. WPR output will contain information for
// each request WPR received and responded to.
const char kVerboseWprOutput[] = "log_verbose_wpr_output";
bool ShouldLogVerboseWprOutput() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(kVerboseWprOutput);
}

std::string FilePathToUTF8(const base::FilePath::StringType& str) {
#if BUILDFLAG(IS_WIN)
  return base::WideToUTF8(str);
#else
  return str;
#endif
}

bool GetWPRCaptureDir(base::FilePath* capture_dir) {
  base::FilePath src_dir;
  if (!base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir)) {
    ADD_FAILURE() << "Failed to extract the Chromium source directory!";
    return false;
  }
  *capture_dir = src_dir.AppendASCII("chrome")
                     .AppendASCII("test")
                     .AppendASCII("data")
                     .AppendASCII("enterprise")
                     .AppendASCII("connectors")
                     .AppendASCII("file_system")
                     .AppendASCII("captured_sites");
  return true;
}

class WebPageReplayUtil {
 public:
  WebPageReplayUtil() = default;

  ~WebPageReplayUtil() {
    StopWebPageReplayServer();
    StopWebPageRecordServer();
  }

  static void SetUpCommandLine(base::CommandLine* command_line) {
    // Direct traffic to the Web Page Replay server.
    command_line->AppendSwitchASCII(
        network::switches::kHostResolverRules,
        base::StringPrintf("MAP *:80 127.0.0.1:%d,"
                           "MAP *:443 127.0.0.1:%d,"
                           // But exclude traffic to the local server.
                           "EXCLUDE localhost",
                           kHostHttpPort, kHostHttpsPort));
    command_line->AppendSwitchASCII(
        network::switches::kIgnoreCertificateErrorsSPKIList,
        kWebPageReplayCertSPKI);
  }

  bool StartWebPageReplayServer(const base::FilePath& capture_file_path) {
    base::FilePath script_dir;
    if (!GetWPRSupportScriptDir(&script_dir)) {
      ADD_FAILURE() << "Failed to extract the WPR support script directory!";
      return false;
    }
    base::FilePath wpr_src_dir;
    if (!GetWPRSrcDir(&wpr_src_dir)) {
      ADD_FAILURE() << "Failed to extract the WPR src directory!";
      return false;
    }
    base::FilePath wpr_executable;
    if (!GetWprBinary(&wpr_executable)) {
      ADD_FAILURE() << "Failed to extract the WPR executable path!";
      return false;
    }

    base::LaunchOptions options = base::LaunchOptionsForTest();
    options.current_directory = wpr_executable.DirName();
    base::CommandLine command(wpr_executable);
    command.AppendArg("replay");

    command.AppendArg(base::StringPrintf(
        "--https_cert_file=%s",
        FilePathToUTF8(wpr_src_dir.AppendASCII("wpr_cert.pem").value())
            .c_str()));
    command.AppendArg(base::StringPrintf(
        "--https_key_file=%s",
        FilePathToUTF8(wpr_src_dir.AppendASCII("wpr_key.pem").value())
            .c_str()));
    command.AppendArg(base::StringPrintf("--http_port=%d", kHostHttpPort));
    command.AppendArg(base::StringPrintf("--https_port=%d", kHostHttpsPort));
    command.AppendArg(base::StringPrintf(
        "--inject_scripts=%s,%s",
        FilePathToUTF8(wpr_src_dir.AppendASCII("deterministic.js").value())
            .c_str(),
        FilePathToUTF8(script_dir.AppendASCII("automation_helper.js").value())
            .c_str()));
    command.AppendArg("--serve_response_in_chronological_sequence");
    if (!ShouldLogVerboseWprOutput())
      command.AppendArg("--quiet_mode");
    command.AppendArg(base::StringPrintf(
        "%s", FilePathToUTF8(capture_file_path.value()).c_str()));

    LOG(INFO) << command.GetCommandLineString();

    web_page_replay_server_ = base::LaunchProcess(command, options);

    base::RunLoop wpr_launch_waiter;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, wpr_launch_waiter.QuitClosure(), base::Seconds(5));
    wpr_launch_waiter.Run();

    if (!web_page_replay_server_.IsValid()) {
      ADD_FAILURE() << "Failed to start the WPR replay server!";
      return false;
    }

    return true;
  }

  void StopWebPageReplayServer() {
    if (web_page_replay_server_.IsValid())
      VLOG(1) << "Stopping process";
    if (web_page_replay_server_.IsValid() &&
        !web_page_replay_server_.Terminate(0, true)) {
      ADD_FAILURE() << "Failed to terminate the WPR replay server!";
    }
  }

  bool StartWebPageRecordServer(const base::FilePath& capture_file_path) {
#if BUILDFLAG(IS_POSIX)
    base::FilePath script_dir;
    if (!GetWPRSupportScriptDir(&script_dir)) {
      ADD_FAILURE() << "Failed to extract the WPR support script directory!";
      return false;
    }
    base::FilePath wpr_src_dir;
    if (!GetWPRSrcDir(&wpr_src_dir)) {
      ADD_FAILURE() << "Failed to extract the WPR src directory!";
      return false;
    }
    base::FilePath wpr_executable;
    if (!GetWprBinary(&wpr_executable)) {
      ADD_FAILURE() << "Failed to extract the WPR executable path!";
      return false;
    }

    // To signal the creation of a WPR capture, a user needs to send CTRL+C
    // to the running WPR recording process.
    // It is really difficult to code 'Sending CTRL+C to running process'.
    // So instead, we open a xterm console instead (linux only), and prompt
    // the tester to manually send CTRL+C to the WPR xterm window at the
    // end of the test.
    base::CommandLine launch_console_command({"xterm"});
    // Uncomment this line to debug any issues with WPR.
    // launch_console_command.AppendArg("-hold");
    launch_console_command.AppendArg("-e");
    launch_console_command.AppendArg(wpr_executable.value().c_str());
    launch_console_command.AppendArg("record");
    launch_console_command.AppendArg(base::StringPrintf(
        "--https_cert_file=%s",
        FilePathToUTF8(wpr_src_dir.AppendASCII("wpr_cert.pem").value())
            .c_str()));
    launch_console_command.AppendArg(base::StringPrintf(
        "--https_key_file=%s",
        FilePathToUTF8(wpr_src_dir.AppendASCII("wpr_key.pem").value())
            .c_str()));
    launch_console_command.AppendArg(
        base::StringPrintf("--http_port=%d", kHostHttpPort));
    launch_console_command.AppendArg(
        base::StringPrintf("--https_port=%d", kHostHttpsPort));
    launch_console_command.AppendArg(base::StringPrintf(
        "--inject_scripts=%s,%s",
        FilePathToUTF8(wpr_src_dir.AppendASCII("deterministic.js").value())
            .c_str(),
        FilePathToUTF8(script_dir.AppendASCII("automation_helper.js").value())
            .c_str()));
    launch_console_command.AppendArg(base::StringPrintf(
        "%s", FilePathToUTF8(capture_file_path.value()).c_str()));

    VLOG(2) << launch_console_command.GetCommandLineString();

    base::LaunchOptions options = base::LaunchOptionsForTest();
    web_page_record_server_ =
        base::LaunchProcess(launch_console_command, options);

    base::RunLoop wpr_launch_waiter;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, wpr_launch_waiter.QuitClosure(), base::Seconds(5));
    wpr_launch_waiter.Run();

    if (!web_page_record_server_.IsValid()) {
      ADD_FAILURE() << "Failed to start the WPR record server!";
      return false;
    }
    return true;
#else
    ADD_FAILURE() << "Recording is not supported on this platform!";
    return false;
#endif
  }

  void StopWebPageRecordServer() {
    if (web_page_record_server_.IsValid()) {
      VLOG(0)
          << "Please press Ctrl-C in the xterm to end the WPR record process.";
      int exit_code;
      web_page_record_server_.WaitForExit(&exit_code);
      EXPECT_EQ(exit_code, 0);
    }
  }

 private:
  static bool GetWprBinary(base::FilePath* path) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    base::FilePath exe_dir;
    if (!base::PathService::Get(base::DIR_SOURCE_ROOT, &exe_dir)) {
      ADD_FAILURE() << "Failed to extract the Chromium source directory!";
      return false;
    }

    base::FilePath web_page_replay_binary_dir =
        exe_dir.AppendASCII("third_party")
            .AppendASCII("catapult")
            .AppendASCII("telemetry")
            .AppendASCII("telemetry")
            .AppendASCII("bin");

#if BUILDFLAG(IS_WIN)
    base::FilePath wpr_executable_binary =
        base::FilePath(FILE_PATH_LITERAL("win"))
            .AppendASCII("AMD64")
            .AppendASCII("wpr.exe");
#elif BUILDFLAG(IS_MAC)
    base::FilePath wpr_executable_binary =
        base::FilePath(FILE_PATH_LITERAL("mac"))
            .AppendASCII("x86_64")
            .AppendASCII("wpr");
#elif BUILDFLAG(IS_POSIX)
    base::FilePath wpr_executable_binary =
        base::FilePath(FILE_PATH_LITERAL("linux"))
            .AppendASCII("x86_64")
            .AppendASCII("wpr");
#else
#error Plaform is not supported.
#endif
    *path = web_page_replay_binary_dir.Append(wpr_executable_binary);
    return base::PathExists(*path);
  }

  bool static GetWPRSupportScriptDir(base::FilePath* path) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    base::FilePath src_dir;
    if (!base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir)) {
      ADD_FAILURE() << "Failed to extract the Chromium source directory!";
      return false;
    }

    *path = src_dir.AppendASCII("chrome")
                .AppendASCII("test")
                .AppendASCII("data")
                .AppendASCII("web_page_replay_go_helper_scripts");
    return base::DirectoryExists(*path);
  }

  bool static GetWPRSrcDir(base::FilePath* path) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    base::FilePath src_dir;
    if (!base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir)) {
      ADD_FAILURE() << "Failed to extract the Chromium source directory!";
      return false;
    }

    *path = src_dir.AppendASCII("third_party")
                .AppendASCII("catapult")
                .AppendASCII("web_page_replay_go");
    return base::DirectoryExists(*path);
  }

  base::Process web_page_replay_server_;
  base::Process web_page_record_server_;
};

class DownloadManagerObserver : public content::DownloadManager::Observer {
 public:
  explicit DownloadManagerObserver(Browser* browser)
      : browser_(browser),
        download_manager_(browser->profile()->GetDownloadManager()) {
    download_manager_->AddObserver(this);
  }

  ~DownloadManagerObserver() override {
    if (download_manager_)
      download_manager_->RemoveObserver(this);
  }

  void ManagerGoingDown(content::DownloadManager* manager) override {
    DCHECK_EQ(manager, download_manager_);
    download_manager_ = nullptr;
  }

  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override {
    download_items_.push_back(item);

    if (!stop_waiting_for_download_.is_null()) {
      std::move(stop_waiting_for_download_).Run();
      stop_waiting_for_download_.Reset();
    }
  }

  void WaitForDownloadCreation() {
    base::RunLoop run_loop;
    stop_waiting_for_download_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void WaitForDownloadToFinish() {
    content::DownloadTestObserverTerminal observer(
        download_manager_, /*wait count*/ 1,
        content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_QUIT);
    observer.WaitForFinished();
  }

  std::vector<history::DownloadRow> WaitForDownloadHistoryInfo() {
    std::vector<history::DownloadRow> results;
    base::RunLoop run_loop;
    HistoryServiceFactory::GetForProfile(browser_->profile(),
                                         ServiceAccessType::EXPLICIT_ACCESS)
        ->QueryDownloads(base::BindLambdaForTesting(
            [&](std::vector<history::DownloadRow> rows) {
              results = std::move(rows);
              run_loop.Quit();
            }));
    run_loop.Run();
    return results;
  }

  download::DownloadItem* GetLatestDownloadItem() {
    DCHECK(!download_items_.empty());
    return download_items_.back();
  }

  std::vector<download::DownloadItem*> GetAllDownloadItems() {
    return download_items_;
  }

  content::DownloadManager* download_manager() { return download_manager_; }

 private:
  raw_ptr<Browser> browser_ = nullptr;
  raw_ptr<content::DownloadManager> download_manager_ = nullptr;
  std::vector<download::DownloadItem*> download_items_;
  base::OnceClosure stop_waiting_for_download_;
};

}  // namespace

namespace enterprise_connectors {

class BoxCapturedSitesInteractiveTest
    : public FileSystemConnectorBrowserTestBase {
 public:
  BoxCapturedSitesInteractiveTest() = default;
  ~BoxCapturedSitesInteractiveTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    FileSystemConnectorBrowserTestBase::SetUpOnMainThread();
    download_manager_observer_ =
        std::make_unique<DownloadManagerObserver>(browser());

    // Allow test in live mode to access the Internet.
    if (GetTestExecutionMode() == TestExecutionMode::kLive)
      host_resolver()->AllowDirectLookup("*");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    FileSystemConnectorBrowserTestBase::SetUpCommandLine(command_line);
    if (GetTestExecutionMode() != TestExecutionMode::kLive)
      WebPageReplayUtil::SetUpCommandLine(command_line);

      // Disable GPU acceleration on Linux to avoid the GPU process
      // crashing, and inadvertently block page load.
#if BUILDFLAG(IS_POSIX)
    command_line->AppendSwitch(switches::kDisableGpu);
    command_line->AppendSwitch(switches::kDisableSoftwareRasterizer);
#endif
  }

  void TearDownOnMainThread() override {
    FileSystemConnectorBrowserTestBase::TearDownOnMainThread();

    // Make sure that this function terminates all in-progress downloads.
    // Otherwise Chrome will prompt the user to confirm closing the Chrome
    // window while there are active downloads. The prompt in turn prevents
    // the test from terminating.
    if (download_manager_observer()) {
      for (auto* download_item :
           download_manager_observer()->GetAllDownloadItems()) {
        if (!download_item->IsDone())
          download_item->Cancel(false);
      }
    }
  }

  void StartWprUsingFSCCaptureDir(const char* replay_file_relative_path) {
    base::FilePath capture_dir;
    ASSERT_TRUE(GetWPRCaptureDir(&capture_dir))
        << "Failed to get the FSC integration test root capture directory!";
    const base::FilePath replay_file_abs_path =
        capture_dir.AppendASCII(replay_file_relative_path);

    switch (GetTestExecutionMode()) {
      case TestExecutionMode::kRecord:
        ASSERT_TRUE(web_page_replay_util()->StartWebPageRecordServer(
            replay_file_abs_path))
            << "Failed to start WPR recording session!";
        break;
      case TestExecutionMode::kReplay:
        ASSERT_TRUE(web_page_replay_util()->StartWebPageReplayServer(
            replay_file_abs_path))
            << "Failed to start WPR replay session!";
        break;
      case TestExecutionMode::kLive:
        break;
      default:
        NOTREACHED() << "Unrecognized test execution mode!";
    }
  }

  void StartDownloadByNavigatingToEmbeddedServerUrl(const char* relative_url) {
    GURL url = embedded_test_server()->GetURL(relative_url);
    VLOG(1) << url;
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_NONE);
    download_manager_observer_->WaitForDownloadCreation();
  }

  void GetItemViewForLastDownload(DownloadItemView** item_view) {
    EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
    DownloadShelfView* shelf = static_cast<DownloadShelfView*>(
        browser()->window()->GetDownloadShelf());
    ASSERT_TRUE(shelf) << "No download shelf found!";
    *item_view = shelf->GetViewOfLastDownloadItemForTesting();
    ASSERT_TRUE(item_view) << "No download item view found!";
  }

  void StartDownloadFromEmbeddedServerUrlByDownloadManagerRequest(
      const char* relative_url) {
    GURL url = embedded_test_server()->GetURL(relative_url);
    VLOG(1) << url;
    // First, set the tab URL to the download url's parent directory.
    // I.e. If the url is https://www.foo.com/path/file.zip, then set the
    // parent directory is https://www.foo.com/path.
    // This is to avoid leaving the current tab on about:blank.
    // When it comes to applying filters, the FSC will apply the url filter
    // to both the download url and to the tab url. Leaving the tab at
    // about:blank will result in unexpected filter behaviors.
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url.GetWithoutFilename(), WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

    download_manager_observer_->download_manager()->DownloadUrl(
        content::DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
            browser()->tab_strip_model()->GetActiveWebContents(), url,
            TRAFFIC_ANNOTATION_FOR_TESTS));
    download_manager_observer_->WaitForDownloadCreation();
  }

  void DownloadFromEmbeddedServerAndVerifyDownloadServiceProvider(
      const char* relative_url,
      DownloadServiceProvider expected_service_provider,
      bool& need_to_bypass_box_signin,
      const char* expected_mime,
      const std::string& username,
      const std::string& password) {
    StartDownloadFromEmbeddedServerUrlByDownloadManagerRequest(relative_url);
    BoxDownloadItemObserver download_item_observer(
        download_manager_observer()->GetLatestDownloadItem());
    if (strlen(expected_mime) > 0)
      EXPECT_EQ(download_item_observer.download_item()->GetMimeType(),
                expected_mime);

    download_item_observer.WaitForDownloadItemRerouteInfo();
    auto service_provider = download_item_observer.GetServiceProvider();
    ASSERT_NE(service_provider, DownloadServiceProvider::kUnknown);
    EXPECT_EQ(service_provider, expected_service_provider);

    if (service_provider == DownloadServiceProvider::kBox &&
        need_to_bypass_box_signin) {
      download_item_observer.WaitForSignInConfirmationDialog();
      download_item_observer.sign_in_observer()->AcceptSignInConfirmation();
      download_item_observer.sign_in_observer()
          ->AuthorizeWithUserAndPasswordSFA(username, password);
      need_to_bypass_box_signin = false;
      download_item_observer.upload_observer()->WaitForUploadCompletion();
      download_manager_observer()->WaitForDownloadToFinish();
    }
  }

  DownloadManagerObserver* download_manager_observer() {
    return download_manager_observer_.get();
  }

  WebPageReplayUtil* web_page_replay_util() { return &wpr_util_; }

 private:
  std::unique_ptr<DownloadManagerObserver> download_manager_observer_;
  WebPageReplayUtil wpr_util_;
};

IN_PROC_BROWSER_TEST_F(BoxCapturedSitesInteractiveTest,
                       SFA_DownloadSmallFileSuccess) {
  BoxTestAccount account = GetSFATestAccount();
  ASSERT_NO_FATAL_FAILURE(
      SetCloudFSCPolicy(GetAllAllowedTestPolicy(account.enterprise_id)));
  ASSERT_NO_FATAL_FAILURE(StartWprUsingFSCCaptureDir("box.com.sfa.wpr"));

  EXPECT_FALSE(browser()->window()->IsDownloadShelfVisible());

  StartDownloadByNavigatingToEmbeddedServerUrl(
      "/enterprise/connectors/file_system/downloads/cipd/"
      "direct_download_gibben.epub");
  BoxDownloadItemObserver download_item_observer(
      download_manager_observer()->GetLatestDownloadItem());
  download_item_observer.WaitForDownloadItemRerouteInfo();
  ASSERT_EQ(download_item_observer.GetServiceProvider(),
            DownloadServiceProvider::kBox);

  download_item_observer.WaitForSignInConfirmationDialog();
  ASSERT_NO_FATAL_FAILURE(
      download_item_observer.sign_in_observer()->AcceptSignInConfirmation());

  // Make sure that the download shelf is showing.
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());

  // Bypass the Box signin and authorize dialog.
  ASSERT_NO_FATAL_FAILURE(download_item_observer.sign_in_observer()
                              ->AuthorizeWithUserAndPasswordSFA(
                                  account.user_name, account.password));
  ASSERT_TRUE(
      download_item_observer.fetch_access_token_observer()->WaitForFetch());

  // Check that the download shelf is displaying the expected "uploading"
  // text.
  DownloadItemView* item_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetItemViewForLastDownload(&item_view));
  download_item_observer.upload_observer()->WaitForUploadStart();
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_DOWNLOAD_STATUS_UPLOADING,
                l10n_util::GetStringUTF16(IDS_FILE_SYSTEM_CONNECTOR_BOX)),
            item_view->GetStatusTextForTesting());

  // Check the in-progress download database.
  std::vector<download::DownloadItem*> downloads_in_progress;
  browser()->profile()->GetDownloadManager()->GetAllDownloads(
      &downloads_in_progress);
  EXPECT_EQ(1u, downloads_in_progress.size());
  EXPECT_EQ(download::DownloadItem::IN_PROGRESS,
            downloads_in_progress.front()->GetState());

  ASSERT_TRUE(
      download_item_observer.upload_observer()->WaitForUploadCompletion());
  download_manager_observer()->WaitForDownloadToFinish();
  EXPECT_TRUE(
      download_item_observer.upload_observer()->WaitForTmpFileDeletion());

  // Check that the download shelf is displaying the expected "uploaded"
  // text.
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_DOWNLOAD_STATUS_UPLOADED,
                l10n_util::GetStringUTF16(IDS_FILE_SYSTEM_CONNECTOR_BOX)),
            item_view->GetStatusTextForTesting());

  // Check the history database.
  auto downloads_in_history =
      download_manager_observer()->WaitForDownloadHistoryInfo();
  EXPECT_EQ(1u, downloads_in_history.size());

  // Open the downloaded item.
  ui_test_utils::TabAddedWaiter tab_waiter(browser());
  item_view->OpenItemForTesting();
  tab_waiter.Wait();
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(browser()->tab_strip_model()->GetWebContentsAt(1)->GetURL(),
            download_item_observer.upload_observer()->GetFileUrl());
}

IN_PROC_BROWSER_TEST_F(BoxCapturedSitesInteractiveTest,
                       MFA_DownloadSmallFileSuccess) {
  BoxTestAccount account = GetMFATestAccount();
  ASSERT_NO_FATAL_FAILURE(
      SetCloudFSCPolicy(GetAllAllowedTestPolicy(account.enterprise_id)));
  ASSERT_NO_FATAL_FAILURE(StartWprUsingFSCCaptureDir("box.com.mfa.wpr"));

  StartDownloadByNavigatingToEmbeddedServerUrl(
      "/enterprise/connectors/file_system/downloads/cipd/"
      "direct_download_gibben.epub");
  BoxDownloadItemObserver download_item_observer(
      download_manager_observer()->GetLatestDownloadItem());
  download_item_observer.WaitForDownloadItemRerouteInfo();
  ASSERT_EQ(download_item_observer.GetServiceProvider(),
            DownloadServiceProvider::kBox);

  download_item_observer.WaitForSignInConfirmationDialog();
  ASSERT_NO_FATAL_FAILURE(
      download_item_observer.sign_in_observer()->AcceptSignInConfirmation());

  // Bypass the Box signin and authorize dialog.
  ASSERT_NO_FATAL_FAILURE(
      download_item_observer.sign_in_observer()
          ->AuthorizeWithUserAndPassword2FA(
              account.user_name, account.password, "123456",
              GetTestExecutionMode() != TestExecutionMode::kReplay));
  ASSERT_TRUE(
      download_item_observer.fetch_access_token_observer()->WaitForFetch());

  ASSERT_TRUE(
      download_item_observer.upload_observer()->WaitForUploadCompletion());
  download_manager_observer()->WaitForDownloadToFinish();
  EXPECT_TRUE(
      download_item_observer.upload_observer()->WaitForTmpFileDeletion());
}

IN_PROC_BROWSER_TEST_F(BoxCapturedSitesInteractiveTest,
                       DownloadLargeFileSuccess) {
  BoxTestAccount account = GetSFATestAccount();
  ASSERT_NO_FATAL_FAILURE(
      SetCloudFSCPolicy(GetAllAllowedTestPolicy(account.enterprise_id)));
  ASSERT_NO_FATAL_FAILURE(
      StartWprUsingFSCCaptureDir("box.com.large.download.wpr"));

  StartDownloadByNavigatingToEmbeddedServerUrl(
      "/enterprise/connectors/file_system/downloads/cipd/"
      "large_download_gibben.mobi");
  BoxDownloadItemObserver download_item_observer(
      download_manager_observer()->GetLatestDownloadItem());
  download_item_observer.WaitForDownloadItemRerouteInfo();
  ASSERT_EQ(download_item_observer.GetServiceProvider(),
            DownloadServiceProvider::kBox);

  // Sign in to authorize Chrome to upload to Box.com.
  download_item_observer.WaitForSignInConfirmationDialog();
  ASSERT_NO_FATAL_FAILURE(
      download_item_observer.sign_in_observer()->AcceptSignInConfirmation());
  ASSERT_NO_FATAL_FAILURE(download_item_observer.sign_in_observer()
                              ->AuthorizeWithUserAndPasswordSFA(
                                  account.user_name, account.password));

  ASSERT_TRUE(
      download_item_observer.fetch_access_token_observer()->WaitForFetch());

  // Check that the download shelf is displaying the expected "uploading"
  // text.
  DownloadItemView* item_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetItemViewForLastDownload(&item_view));
  download_item_observer.upload_observer()->WaitForUploadStart();
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_DOWNLOAD_STATUS_UPLOADING,
                l10n_util::GetStringUTF16(IDS_FILE_SYSTEM_CONNECTOR_BOX)),
            item_view->GetStatusTextForTesting());

  ASSERT_TRUE(
      download_item_observer.upload_observer()->WaitForUploadCompletion());
  download_manager_observer()->WaitForDownloadToFinish();
  EXPECT_TRUE(
      download_item_observer.upload_observer()->WaitForTmpFileDeletion());

  // Check that the download shelf is displaying the expected "uploaded"
  // text.
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_DOWNLOAD_STATUS_UPLOADED,
                l10n_util::GetStringUTF16(IDS_FILE_SYSTEM_CONNECTOR_BOX)),
            item_view->GetStatusTextForTesting());

  // Open the downloaded item.
  ui_test_utils::TabAddedWaiter tab_waiter(browser());
  item_view->OpenItemForTesting();
  tab_waiter.Wait();
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(browser()->tab_strip_model()->GetWebContentsAt(1)->GetURL(),
            download_item_observer.upload_observer()->GetFileUrl());
}

IN_PROC_BROWSER_TEST_F(BoxCapturedSitesInteractiveTest, EnterpriseIdMismatch) {
  BoxTestAccount account = GetSFATestAccount();
  SetCloudFSCPolicy(GetAllAllowedTestPolicy("123456789"));
  StartWprUsingFSCCaptureDir("box.com.ent.id.mismatch.wpr");

  StartDownloadByNavigatingToEmbeddedServerUrl(
      "/enterprise/connectors/file_system/downloads/cipd/"
      "direct_download_gibben.epub");
  BoxDownloadItemObserver download_item_observer(
      download_manager_observer()->GetLatestDownloadItem());
  download_item_observer.WaitForDownloadItemRerouteInfo();
  ASSERT_EQ(download_item_observer.GetServiceProvider(),
            DownloadServiceProvider::kBox);

  download_item_observer.WaitForSignInConfirmationDialog();
  download_item_observer.sign_in_observer()->AcceptSignInConfirmation();

  // Bypass the Box signin and authorize dialog.
  download_item_observer.sign_in_observer()->AuthorizeWithUserAndPasswordSFA(
      account.user_name, account.password);
  EXPECT_FALSE(
      download_item_observer.fetch_access_token_observer()->WaitForFetch());

  // The sign in confirmation dialog will relaunch after the enterprise ID
  // mismatch error. Close the confirmation dialog.
  download_item_observer.WaitForSignInConfirmationDialog();
  download_item_observer.sign_in_observer()->CancelSignInConfirmation();

  download_manager_observer()->WaitForDownloadToFinish();
  EXPECT_TRUE(
      download_item_observer.upload_observer()->WaitForTmpFileDeletion());

  // Check that the download shelf is displaying the expected "upload
  // cancelled" text.
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
  DownloadItemView* item_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetItemViewForLastDownload(&item_view));
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_CANCELLED),
            item_view->GetStatusTextForTesting());
}

IN_PROC_BROWSER_TEST_F(BoxCapturedSitesInteractiveTest,
                       CancelSignInConfirmation) {
  ASSERT_NO_FATAL_FAILURE(
      SetCloudFSCPolicy(GetAllAllowedTestPolicy("123456789")));
  ASSERT_NO_FATAL_FAILURE(
      StartWprUsingFSCCaptureDir("box.com.cancel.sign.in.confirmation.wpr"));

  StartDownloadByNavigatingToEmbeddedServerUrl(
      "/enterprise/connectors/file_system/downloads/"
      "small_download.zip");
  BoxDownloadItemObserver download_item_observer(
      download_manager_observer()->GetLatestDownloadItem());
  download_item_observer.WaitForDownloadItemRerouteInfo();
  ASSERT_EQ(download_item_observer.GetServiceProvider(),
            DownloadServiceProvider::kBox);

  download_item_observer.WaitForSignInConfirmationDialog();
  ASSERT_NO_FATAL_FAILURE(
      download_item_observer.sign_in_observer()->CancelSignInConfirmation());
  EXPECT_TRUE(
      download_item_observer.upload_observer()->WaitForUploadCompletion());
  EXPECT_TRUE(
      download_item_observer.upload_observer()->WaitForTmpFileDeletion());

  // Check that the download shelf is displaying the expected "upload
  // cancelled" text.
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
  DownloadItemView* item_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetItemViewForLastDownload(&item_view));
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_CANCELLED),
            item_view->GetStatusTextForTesting());
}

IN_PROC_BROWSER_TEST_F(BoxCapturedSitesInteractiveTest, ExitSignInDialog) {
  ASSERT_NO_FATAL_FAILURE(
      SetCloudFSCPolicy(GetAllAllowedTestPolicy("123456789")));
  ASSERT_NO_FATAL_FAILURE(
      StartWprUsingFSCCaptureDir("box.com.sign.in.fail.wpr"));

  StartDownloadByNavigatingToEmbeddedServerUrl(
      "/enterprise/connectors/file_system/downloads/"
      "small_download.zip");
  BoxDownloadItemObserver download_item_observer(
      download_manager_observer()->GetLatestDownloadItem());
  download_item_observer.WaitForDownloadItemRerouteInfo();
  ASSERT_EQ(download_item_observer.GetServiceProvider(),
            DownloadServiceProvider::kBox);

  download_item_observer.WaitForSignInConfirmationDialog();
  ASSERT_NO_FATAL_FAILURE(
      download_item_observer.sign_in_observer()->AcceptSignInConfirmation());
  ASSERT_NO_FATAL_FAILURE(
      download_item_observer.sign_in_observer()->SubmitInvalidSignInCredentials(
          "fake_user@FakeDomain.com", "fake_password"));
  download_item_observer.sign_in_observer()->CloseSignInWidget();
  EXPECT_TRUE(
      download_item_observer.upload_observer()->WaitForUploadCompletion());
  EXPECT_TRUE(
      download_item_observer.upload_observer()->WaitForTmpFileDeletion());

  // Check that the download shelf is displaying the expected "upload
  // cancelled" text.
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
  DownloadItemView* item_view = nullptr;
  ASSERT_NO_FATAL_FAILURE(GetItemViewForLastDownload(&item_view));
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_CANCELLED),
            item_view->GetStatusTextForTesting());
}

IN_PROC_BROWSER_TEST_F(BoxCapturedSitesInteractiveTest,
                       FilterByEnabledSettings_match_mime_only) {
  BoxTestAccount account = GetSFATestAccount();
  // Set the SendDownloadToCloudEnterpriseConnector policy
  // Configure a policy to only to upload downloaded files to Box.com IFF:
  // - The download has a mime type of "application/zip".
  ASSERT_NO_FATAL_FAILURE(SetCloudFSCPolicy(GetTestPolicyWithEnabledFilter(
      account.enterprise_id, "*", "application/zip")));
  ASSERT_NO_FATAL_FAILURE(StartWprUsingFSCCaptureDir(
      "box.com.filter.by.enabled.matching.mime.wpr"));

  bool need_to_link_box_account = true;

  // Download a svg image, which should be downloaded directly to the local
  // file system.
  ASSERT_NO_FATAL_FAILURE(
      DownloadFromEmbeddedServerAndVerifyDownloadServiceProvider(
          "/enterprise/connectors/file_system/downloads/sub/example.svg",
          DownloadServiceProvider::kLocal, need_to_link_box_account,
          "image/svg+xml", account.user_name, account.password));

  // Download a zip file, which should be downloaded to Box.com.
  ASSERT_NO_FATAL_FAILURE(
      DownloadFromEmbeddedServerAndVerifyDownloadServiceProvider(
          "/enterprise/connectors/file_system/downloads/sub/"
          "angry_clouds.mp4.zip",
          DownloadServiceProvider::kBox, need_to_link_box_account,
          "application/zip", account.user_name, account.password));
}

IN_PROC_BROWSER_TEST_F(BoxCapturedSitesInteractiveTest,
                       FilterByEnabledSettings_match_url_only) {
  BoxTestAccount account = GetSFATestAccount();
  // Set the SendDownloadToCloudEnterpriseConnector policy
  // Configure a policy to only to upload downloaded files to Box.com IFF:
  // - The download comes from the '.../sub/' url.
  std::string include_url =
      embedded_test_server()
          ->GetURL("/enterprise/connectors/file_system/downloads/sub/")
          .GetContent();
  ASSERT_NO_FATAL_FAILURE(SetCloudFSCPolicy(GetTestPolicyWithEnabledFilter(
      account.enterprise_id, include_url.c_str(), "*")));
  ASSERT_NO_FATAL_FAILURE(
      StartWprUsingFSCCaptureDir("box.com.filter.by.enabled.matching.url.wpr"));

  bool need_to_link_box_account = true;

  // Download a zip file from outside the .../sub/ url, which should be
  // downloaded directly to the local file system.
  ASSERT_NO_FATAL_FAILURE(
      DownloadFromEmbeddedServerAndVerifyDownloadServiceProvider(
          "/enterprise/connectors/file_system/downloads/"
          "angry_clouds.mp4.zip",
          DownloadServiceProvider::kLocal, need_to_link_box_account,
          "" /* No need to check mime. */, account.user_name,
          account.password));

  // Download a zip file from the .../sub/ url, which should be downloaded to
  // Box.com.
  ASSERT_NO_FATAL_FAILURE(
      DownloadFromEmbeddedServerAndVerifyDownloadServiceProvider(
          "/enterprise/connectors/file_system/downloads/sub/"
          "angry_clouds.mp4.zip",
          DownloadServiceProvider::kBox, need_to_link_box_account,
          "" /* No need to check mime. */, account.user_name,
          account.password));
}

IN_PROC_BROWSER_TEST_F(BoxCapturedSitesInteractiveTest,
                       FilterByEnabledSettings_match_mime_and_url) {
  BoxTestAccount account = GetSFATestAccount();
  // Set the SendDownloadToCloudEnterpriseConnector policy
  // Configure a policy to only to upload downloaded files to Box.com IFF:
  // - The download comes from the '.../sub/' url.
  // - The download has a mime type of "application/zip".
  std::string include_url =
      embedded_test_server()
          ->GetURL("/enterprise/connectors/file_system/downloads/sub/")
          .GetContent();
  // Configure a policy to only to upload downloaded files to Box.com IFF:
  // 1. The download comes from the '.../sub/' url.
  // 2. The download has a mime type of "application/zip".
  ASSERT_NO_FATAL_FAILURE(SetCloudFSCPolicy(GetTestPolicyWithEnabledFilter(
      account.enterprise_id, include_url.c_str(), "application/zip")));
  ASSERT_NO_FATAL_FAILURE(StartWprUsingFSCCaptureDir(
      "box.com.filter.by.enabled.matching.url.and.mime.wpr"));

  // Download a zip file from the .../sub/ url, which should be downloaded to
  // Box.com.
  bool need_to_link_box_account = true;
  ASSERT_NO_FATAL_FAILURE(
      DownloadFromEmbeddedServerAndVerifyDownloadServiceProvider(
          "/enterprise/connectors/file_system/downloads/sub/"
          "angry_clouds.mp4.zip",
          DownloadServiceProvider::kBox, need_to_link_box_account,
          "application/zip", account.user_name, account.password));
}

IN_PROC_BROWSER_TEST_F(BoxCapturedSitesInteractiveTest,
                       FilterByDisabledSettings_match_mime_only) {
  BoxTestAccount account = GetSFATestAccount();
  // Set the SendDownloadToCloudEnterpriseConnector policy
  // Configure a policy to only to upload downloaded files to Box.com UNLESS:
  // - The download has a mime type of "application/zip".
  ASSERT_NO_FATAL_FAILURE(SetCloudFSCPolicy(GetTestPolicyWithDisabledFilter(
      account.enterprise_id, "*", "application/zip")));
  ASSERT_NO_FATAL_FAILURE(StartWprUsingFSCCaptureDir(
      "box.com.filter.by.disabled.matching.mime.wpr"));

  bool need_to_link_box_account = true;

  // Download a svg image, which should be downloaded to Box.com.
  ASSERT_NO_FATAL_FAILURE(
      DownloadFromEmbeddedServerAndVerifyDownloadServiceProvider(
          "/enterprise/connectors/file_system/downloads/sub/example.svg",
          DownloadServiceProvider::kBox, need_to_link_box_account,
          "image/svg+xml", account.user_name, account.password));

  // Download a zip file, which should be downloaded directly to the local
  // file system.
  ASSERT_NO_FATAL_FAILURE(
      DownloadFromEmbeddedServerAndVerifyDownloadServiceProvider(
          "/enterprise/connectors/file_system/downloads/sub/"
          "angry_clouds.mp4.zip",
          DownloadServiceProvider::kLocal, need_to_link_box_account,
          "application/zip", account.user_name, account.password));
}

IN_PROC_BROWSER_TEST_F(BoxCapturedSitesInteractiveTest,
                       FilterByDisabledSettings_match_url_only) {
  BoxTestAccount account = GetSFATestAccount();
  // Set the SendDownloadToCloudEnterpriseConnector policy
  // Configure a policy to only to upload downloaded files to Box.com UNLESS:
  // - The download comes from the '.../sub/' url.
  std::string include_url =
      embedded_test_server()
          ->GetURL("/enterprise/connectors/file_system/downloads/sub/")
          .GetContent();
  ASSERT_NO_FATAL_FAILURE(SetCloudFSCPolicy(GetTestPolicyWithDisabledFilter(
      account.enterprise_id, include_url.c_str(), "*")));
  ASSERT_NO_FATAL_FAILURE(StartWprUsingFSCCaptureDir(
      "box.com.filter.by.disabled.matching.url.wpr"));

  bool need_to_link_box_account = true;

  // Download a zip file from outside the .../sub/ url, which should be
  // downloaded to Box.com.
  ASSERT_NO_FATAL_FAILURE(
      DownloadFromEmbeddedServerAndVerifyDownloadServiceProvider(
          "/enterprise/connectors/file_system/downloads/"
          "angry_clouds.mp4.zip",
          DownloadServiceProvider::kBox, need_to_link_box_account,
          "" /* No need to check mime. */, account.user_name,
          account.password));

  // Download a zip file from the .../sub/ url, which should be
  // downloaded directly to the local file system.
  ASSERT_NO_FATAL_FAILURE(
      DownloadFromEmbeddedServerAndVerifyDownloadServiceProvider(
          "/enterprise/connectors/file_system/downloads/sub/"
          "angry_clouds.mp4.zip",
          DownloadServiceProvider::kLocal, need_to_link_box_account,
          "" /* No need to check mime. */, account.user_name,
          account.password));
}

IN_PROC_BROWSER_TEST_F(BoxCapturedSitesInteractiveTest,
                       FilterByDisabledSettings_match_mime_and_url) {
  BoxTestAccount account = GetSFATestAccount();
  // Set the SendDownloadToCloudEnterpriseConnector policy
  // Configure a policy to only to upload downloaded files to Box.com IFF:
  // - The download comes from the '.../sub/' url.
  // - The download has a mime type of "application/zip".
  std::string include_url =
      embedded_test_server()
          ->GetURL("/enterprise/connectors/file_system/downloads/sub/")
          .GetContent();
  // Configure a policy to only to upload downloaded files to Box.com UNLESS:
  // 1. The download comes from the '.../sub/' url.
  // 2. The download has a mime type of "application/zip".
  ASSERT_NO_FATAL_FAILURE(SetCloudFSCPolicy(GetTestPolicyWithDisabledFilter(
      account.enterprise_id, include_url.c_str(), "application/zip")));
  ASSERT_NO_FATAL_FAILURE(StartWprUsingFSCCaptureDir(
      "box.com.filter.by.disabled.matching.url.and.mime.wpr"));

  // Download a zip file from the .../sub/ url, which should be
  // downloaded directly to the local file system.
  bool need_to_link_box_account = true;
  ASSERT_NO_FATAL_FAILURE(
      DownloadFromEmbeddedServerAndVerifyDownloadServiceProvider(
          "/enterprise/connectors/file_system/downloads/sub/"
          "angry_clouds.mp4.zip",
          DownloadServiceProvider::kLocal, need_to_link_box_account,
          "" /* No need to check mime. */, account.user_name,
          account.password));
}

IN_PROC_BROWSER_TEST_F(BoxCapturedSitesInteractiveTest,
                       SignInDlg_PrepopulateAccount) {
  const char kUserName[] = "Test User";
  const char kLogin[] = "test_user@test.com";
  base::DictionaryValue account_info;
  account_info.SetStringKey(enterprise_connectors::kBoxNameFieldName,
                            kUserName);
  account_info.SetStringKey(enterprise_connectors::kBoxLoginFieldName, kLogin);
  SetFileSystemAccountInfo(
      browser()->profile()->GetPrefs(),
      enterprise_connectors::kFileSystemServiceProviderPrefNameBox,
      std::move(account_info));

  ASSERT_NO_FATAL_FAILURE(
      SetCloudFSCPolicy(GetAllAllowedTestPolicy("123456789")));
  ASSERT_NO_FATAL_FAILURE(
      StartWprUsingFSCCaptureDir("box.com.sign.in.prepop.account.wpr"));

  StartDownloadByNavigatingToEmbeddedServerUrl(
      "/enterprise/connectors/file_system/downloads/"
      "small_download.zip");
  BoxDownloadItemObserver download_item_observer(
      download_manager_observer()->GetLatestDownloadItem());

  download_item_observer.WaitForSignInConfirmationDialog();
  ASSERT_NO_FATAL_FAILURE(
      download_item_observer.sign_in_observer()->AcceptSignInConfirmation());

  std::string login_val;
  ASSERT_TRUE(
      download_item_observer.sign_in_observer()->GetUserNameFromSignInPage(
          &login_val));
  EXPECT_EQ(kLogin, login_val);

  download_item_observer.sign_in_observer()->CloseSignInWidget();
  EXPECT_TRUE(
      download_item_observer.upload_observer()->WaitForUploadCompletion());
  EXPECT_TRUE(
      download_item_observer.upload_observer()->WaitForTmpFileDeletion());
}

}  // namespace enterprise_connectors
