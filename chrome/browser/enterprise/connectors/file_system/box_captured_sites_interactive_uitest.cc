// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/string_escape.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/task/current_thread.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/file_system/box_uploader.h"
#include "chrome/browser/enterprise/connectors/file_system/rename_handler.h"
#include "chrome/browser/enterprise/connectors/file_system/signin_experience.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/download/download_item_view.h"
#include "chrome/browser/ui/views/download/download_shelf_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/policy_constants.h"
#include "components/variations/variations_params_manager.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/dns/mock_host_resolver.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/test/test_url_loader_factory.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace content {
class DownloadManagerDelegate;
}

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

// The commandline flags to specify a REAL box.com username and password,
// used to create new web captures against the LIVE box.com site.
const char kBoxAccountUserName[] = "user_name";
const char kBoxAccountPassword[] = "password";

enum TestExecutionMode { kReplay, kRecord, kLive };

const std::string GetBoxAccountUserName() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kBoxAccountUserName)) {
    return command_line->GetSwitchValueASCII(kBoxAccountUserName);
  }
  // In replay mode, it is okay to return a fake account.
  return "FakeUser@FakeDomain.com";
}

const std::string GetBoxAccountPassword() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kBoxAccountPassword)) {
    return command_line->GetSwitchValueASCII(kBoxAccountPassword);
  }
  // In replay mode, it is okay to return a fake account.
  return "FakePassword";
}

// Determine the test execution mode.
// By default, test should execute against captured web traffic.
// To generate/refresh captures for test, one can use the record mode to
// test against the live Box.com.
// For debugging, one can run in live mode, which executes the test
// against live traffic but without recording.
const char kRecordMode[] = "record";
const char kLiveMode[] = "live";
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

std::string GetAllAllowedTestPolicy(const char* enterprise_id) {
  const char kConnectorPolicyString[] = R"PREFIX(
        [ {
           "domain": "*",
           "enable": [ {
               "mime_types": [ "*" ],
               "url_list": [ "*" ]
           } ],
           "enterprise_id": "%s",
           "service_provider": "box"
        } ])PREFIX";
  return base::StringPrintf(kConnectorPolicyString, enterprise_id);
}

std::string FilePathToUTF8(const base::FilePath::StringType& str) {
#if defined(OS_WIN)
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
                     .AppendASCII("captures");
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
        FROM_HERE, wpr_launch_waiter.QuitClosure(),
        base::TimeDelta::FromSeconds(5));
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
#if defined(OS_POSIX)
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
        FROM_HERE, wpr_launch_waiter.QuitClosure(),
        base::TimeDelta::FromSeconds(5));
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
      ASSERT_EQ(exit_code, 0);
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

#if defined(OS_WIN)
    base::FilePath wpr_executable_binary =
        base::FilePath(FILE_PATH_LITERAL("win"))
            .AppendASCII("AMD64")
            .AppendASCII("wpr.exe");
#elif defined(OS_MAC)
    base::FilePath wpr_executable_binary =
        base::FilePath(FILE_PATH_LITERAL("mac"))
            .AppendASCII("x86_64")
            .AppendASCII("wpr");
#elif defined(OS_POSIX)
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

}  // namespace

namespace enterprise_connectors {

class BoxSignInObserver : public SigninExperienceTestObserver,
                          public content::WebContentsObserver,
                          public views::WidgetObserver {
 public:
  enum class Page { kSignin, kAuth, kUnknown };

  explicit BoxSignInObserver(FileSystemRenameHandler* rename_handler) {
    InitForTesting(rename_handler);
  }

  ~BoxSignInObserver() override {
    if (sign_in_widget_)
      sign_in_widget_->RemoveObserver(this);
  }

  // Accept the Sign in confirmation dialog to bring up the Box.com
  // sign in dialog.
  void AcceptBoxSigninConfirmation() {
    signin_confirmation_dlg_->Accept();
    WaitForSignInDialogToShow();
  }

  void CancelBoxSignInConfirmation() {
    signin_confirmation_dlg_->Cancel();
  }

  // Bypass Single-Factor-Authentication sign in and authorize
  // Chrome to access Box.com resources.
  void AuthorizeWithUserAndPasswordSFA(const std::string& username,
                                       const std::string& password) {
    if (current_page_ != Page::kSignin)
      WaitForPageLoad();
    ASSERT_EQ(current_page_, Page::kSignin);
    // Set username and password, then click the submit button.
    EXPECT_TRUE(content::ExecuteScript(
        web_contents(), GetSubmitAccountSignInScript(username, password)));
    WaitForPageLoad();
    ASSERT_EQ(current_page_, Page::kAuth);
    WaitForSignInDialogToClose(base::BindOnce(
        [](const content::ToRenderFrameHost& adapter) {
          EXPECT_TRUE(
              content::ExecuteScript(adapter, GetClickAuthorizeScript()));
        },
        std::move(web_contents())));
  }

  // Bypass 2-Factor-Authentication sign in and authorize
  // Chrome to access Box.com resources.
  void AuthorizeWithUserAndPassword2FA(const std::string& username,
                                       const std::string& password,
                                       const std::string& sms_code) {
    if (current_page_ != Page::kSignin)
      WaitForPageLoad();
    ASSERT_EQ(current_page_, Page::kSignin);
    // Set username and password, then click the authorize button.
    EXPECT_TRUE(content::ExecuteScript(
        web_contents(), GetSubmitAccountSignInScript(username, password)));
    WaitForPageLoad();

    ASSERT_EQ(current_page_, Page::kSignin);
    // In replay mode, supply the temporary password given as a parameter.
    if (GetTestExecutionMode() == TestExecutionMode::kReplay) {
      EXPECT_TRUE(content::ExecuteScript(web_contents(),
                                         GetSubmitSmsCodeScript(sms_code)));
    } else {
      // If test is running the recording mode or live mode, one must manually
      // supply a valid Short Message Service code.
      VLOG(0) << "Please submit the Box.com sms code into the signin dialog.";
    }
    WaitForPageLoad();

    ASSERT_EQ(current_page_, Page::kAuth);
    WaitForSignInDialogToClose(base::BindOnce(
        [](const content::ToRenderFrameHost& adapter) {
          EXPECT_TRUE(
              content::ExecuteScript(adapter, GetClickAuthorizeScript()));
        },
        std::move(web_contents())));
  }

  void SubmitInvalidSignInCredentials(const std::string& username,
                                      const std::string& password) {
    if (current_page_ != Page::kSignin)
      WaitForPageLoad();
    ASSERT_EQ(current_page_, Page::kSignin);
    // Set username and password, then click the authorize button.
    EXPECT_TRUE(content::ExecuteScript(
        web_contents(), GetSubmitAccountSignInScript(username, password)));
    WaitForPageLoad();
    ASSERT_EQ(current_page_, Page::kSignin);
  }

  void CloseSignInWidget() {
    WaitForSignInDialogToClose(
        base::BindOnce([](views::Widget* dialog) { dialog->Close(); },
                       std::move(sign_in_widget_)));
  }

  void WaitForPageLoad() {
    base::RunLoop run_loop;
    stop_waiting_for_page_load_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void WaitForSignInConfirmationDialog() {
    if (signin_confirmation_dlg_)
      return;
    base::RunLoop run_loop;
    stop_waiting_for_signin_confirmation_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void WaitForSignInDialogToShow() {
    if (sign_in_widget_ == nullptr) {
      base::RunLoop run_loop;
      stop_waiting_for_widget_to_show_ = run_loop.QuitClosure();
      run_loop.Run();
    }

    if (current_page_ != Page::kSignin)
      WaitForPageLoad();
  }

  void WaitForSignInDialogToClose(base::OnceClosure trigger_close_action) {
    base::RunLoop run_loop;
    stop_waiting_for_dialog_shutdown_ = run_loop.QuitClosure();
    expecting_dialog_shutdown_ = true;
    std::move(trigger_close_action).Run();
    run_loop.Run();
  }

  // content::WebContentsObserver
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    const GURL& url = navigation_handle->GetURL();
    if (IsBoxSignInURI(url))
      current_page_ = Page::kSignin;
    else if (IsBoxAuthorizeURI(url))
      current_page_ = Page::kAuth;
    else
      current_page_ = Page::kUnknown;
    VLOG(0) << url.spec();
  }

  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    if (stop_waiting_for_page_load_) {
      std::move(stop_waiting_for_page_load_).Run();
      stop_waiting_for_page_load_.Reset();
    }
  }

  // views::WidgetObserver
  void OnWidgetDestroying(views::Widget* widget) override {
    ASSERT_EQ(sign_in_widget_, widget);
    EXPECT_TRUE(expecting_dialog_shutdown_);
    sign_in_widget_ = nullptr;
    if (stop_waiting_for_dialog_shutdown_)
      std::move(stop_waiting_for_dialog_shutdown_).Run();
  }

  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override {
    ASSERT_EQ(sign_in_widget_, widget);
    if (visible) {
      if (stop_waiting_for_widget_to_show_)
        std::move(stop_waiting_for_widget_to_show_).Run();
    }
  }

  // SigninExperienceTestObserver
  void OnConfirmationDialogCreated(
      views::DialogDelegate* confirmation_dialog_delegate) override {
    signin_confirmation_dlg_ = confirmation_dialog_delegate;
    if (stop_waiting_for_signin_confirmation_)
      std::move(stop_waiting_for_signin_confirmation_).Run();
  }

  void OnSignInDialogCreated(content::WebContents* dialog_web_content,
                             views::Widget* dialog_widget) override {
    this->Observe(dialog_web_content);
    dialog_widget->AddObserver(this);
    sign_in_widget_ = dialog_widget;
  }

 private:
  static bool IsBoxSignInURI(const GURL& url) {
    return url.host() == "account.box.com" &&
           url.path() == "/api/oauth2/authorize";
  }

  static bool IsBoxAuthorizeURI(const GURL& url) {
    return url.host() == "app.box.com" && url.path() == "/api/oauth2/authorize";
  }

  static std::string GetSubmitAccountSignInScript(const std::string& username,
                                                  const std::string& password) {
    return base::StringPrintf(
        R"PREFIX(
        (function() {
          document.getElementById('login').value = `%s`;
          document.getElementById('password').value = `%s`;
          document.getElementsByName('login_submit')[0].click();
        })();
        )PREFIX",
        username.c_str(), password.c_str());
  }

  static std::string GetSubmitSmsCodeScript(const std::string& password) {
    return base::StringPrintf(
        R"PREFIX(
        (function() {
          document.getElementById('2fa_sms_code').value = `%s`;
          document.querySelector('button[type="submit"]').click();
        })();
        )PREFIX",
        password.c_str());
  }

  static std::string GetClickAuthorizeScript() {
    return R"PREFIX(
        (function() {
          document.getElementById('consent_accept_button').click();
        })();
        )PREFIX";
  }

  Page current_page_ = Page::kUnknown;
  // This bool variable allows this class to differentiate an expected dialog
  // closure from unexpected dialog shutdown/crash/exits. Before triggering an
  // action to close the dialog, the test class will set this variable to true.
  bool expecting_dialog_shutdown_ = false;
  views::DialogDelegate* signin_confirmation_dlg_ = nullptr;
  views::Widget* sign_in_widget_ = nullptr;
  base::OnceClosure stop_waiting_for_signin_confirmation_;
  base::OnceClosure stop_waiting_for_page_load_;
  base::OnceClosure stop_waiting_for_widget_to_show_;
  base::OnceClosure stop_waiting_for_dialog_shutdown_;
};

class BoxDownloadItemObserver : public download::DownloadItem::Observer {
 public:
  explicit BoxDownloadItemObserver(download::DownloadItem* item)
      : download_item_(item) {
    download_item_->AddObserver(this);
  }

  ~BoxDownloadItemObserver() override {
    if (download_item_)
      download_item_->RemoveObserver(this);
  }

  void OnDownloadDestroyed(download::DownloadItem* item) override {
    ASSERT_EQ(item, download_item_);
    download_item_ = nullptr;
  }

  void OnDownloadUpdated(download::DownloadItem* item) override {
    ASSERT_EQ(item, download_item_);

    // Calling download::DownloadItem::GetRenameHandler before the
    // download::DownloadItem has a full path will result in the
    // creation of an invalid RenameHandler.
    // So check for the DownloadItem full path first to avoid
    // inadvertently breaking the download workflow.
    if (item->GetFullPath().empty())
      return;
    if (rename_handler_created_)
      return;
    if (!item->GetRenameHandler())
      return;

    rename_handler_created_ = true;
    FileSystemRenameHandler* rename_handler =
        static_cast<FileSystemRenameHandler*>(item->GetRenameHandler());
    sign_in_observer_ = std::make_unique<BoxSignInObserver>(rename_handler);
    fetch_access_token_observer_ =
        std::make_unique<BoxFetchAccessTokenTestObserver>(rename_handler);
    upload_observer_ =
        std::make_unique<BoxUploader::TestObserver>(rename_handler);
    if (run_loop_rename_handler_.running())
      run_loop_rename_handler_.Quit();
  }

  void WaitForRenameHandlerCreation() {
    if (sign_in_observer_.get() == nullptr)
      run_loop_rename_handler_.Run();
  }

  void WaitForSignInConfirmationDialog() {
    WaitForRenameHandlerCreation();
    sign_in_observer_->WaitForSignInConfirmationDialog();
  }

  download::DownloadItem* download_item() { return download_item_; }

  BoxSignInObserver* sign_in_observer() { return sign_in_observer_.get(); }

  BoxFetchAccessTokenTestObserver* fetch_access_token_observer() {
    return fetch_access_token_observer_.get();
  }

  BoxUploader::TestObserver* upload_observer() {
    return upload_observer_.get();
  }

 private:
  download::DownloadItem* download_item_ = nullptr;
  base::RunLoop run_loop_rename_handler_;
  bool rename_handler_created_ = false;
  std::unique_ptr<BoxSignInObserver> sign_in_observer_;
  std::unique_ptr<BoxFetchAccessTokenTestObserver> fetch_access_token_observer_;
  std::unique_ptr<BoxUploader::TestObserver> upload_observer_;
};

class DownloadManagerObserver : public content::DownloadManager::Observer {
 public:
  explicit DownloadManagerObserver(Browser* browser)
      : download_manager_(browser->profile()->GetDownloadManager()) {
    download_manager_->AddObserver(this);
  }

  ~DownloadManagerObserver() override {
    if (download_manager_)
      download_manager_->RemoveObserver(this);
  }

  void ManagerGoingDown(content::DownloadManager* manager) override {
    ASSERT_EQ(manager, download_manager_);
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

  download::DownloadItem* GetLatestDownloadItem() {
    EXPECT_FALSE(download_items_.empty());
    return download_items_.back();
  }

  content::DownloadManager* download_manager() { return download_manager_; }

 private:
  content::DownloadManager* download_manager_ = nullptr;
  std::vector<download::DownloadItem*> download_items_;
  base::OnceClosure stop_waiting_for_download_;
};

class BoxCapturedSitesInteractiveTest : public InProcessBrowserTest {
 public:
  BoxCapturedSitesInteractiveTest() = default;
  ~BoxCapturedSitesInteractiveTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    // Set up a localhost server to serve files for download.
    base::FilePath test_file_directory;
    ASSERT_TRUE(
        base::PathService::Get(chrome::DIR_TEST_DATA, &test_file_directory));
    embedded_test_server()->ServeFilesFromDirectory(test_file_directory);
    ASSERT_TRUE(embedded_test_server()->Start());

    // Allow test in live mode to access the Internet.
    if (GetTestExecutionMode() == TestExecutionMode::kLive)
      host_resolver()->AllowDirectLookup("*");

    download_manager_observer_ =
        std::make_unique<DownloadManagerObserver>(browser());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{kFileSystemConnectorEnabled},
        /*disabled_features=*/{});
    if (GetTestExecutionMode() != TestExecutionMode::kLive)
      WebPageReplayUtil::SetUpCommandLine(command_line);
  }

  void TearDownOnMainThread() override {
    // Make sure any pending requests have finished
    base::RunLoop().RunUntilIdle();
  }

  void SetCloudFSCPolicy(const std::string& policy_value) {
    browser()->profile()->GetPrefs()->Set(
        ConnectorPref(FileSystemConnector::SEND_DOWNLOAD_TO_CLOUD),
        *base::JSONReader::Read(policy_value.c_str()));
    // Verify that the FSC is enabled.
    ASSERT_TRUE(IsFSCEnabled());
  }

  bool IsFSCEnabled() {
    auto settings = GetFileSystemSettings(browser()->profile());
    return settings.has_value();
  }

  void StartWprUsingFSCCaptureDir(const char* replay_file_relative_path) {
    base::FilePath capture_dir;
    ASSERT_TRUE(GetWPRCaptureDir(&capture_dir));
    const base::FilePath replay_file_abs_path =
        capture_dir.AppendASCII(replay_file_relative_path);

    switch (GetTestExecutionMode()) {
      case TestExecutionMode::kRecord:
        ASSERT_TRUE(web_page_replay_util()->StartWebPageRecordServer(
            replay_file_abs_path));
        break;
      case TestExecutionMode::kReplay:
        ASSERT_TRUE(web_page_replay_util()->StartWebPageReplayServer(
            replay_file_abs_path));
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

  DownloadItemView* GetItemViewForLastDownload() {
    EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
    DownloadShelfView* shelf = static_cast<DownloadShelfView*>(
        browser()->window()->GetDownloadShelf());
    EXPECT_TRUE(shelf);
    DownloadItemView* item = shelf->GetViewOfLastDownloadItemForTesting();
    EXPECT_TRUE(item);
    return item;
  }

  DownloadManagerObserver* download_manager_observer() {
    return download_manager_observer_.get();
  }

  WebPageReplayUtil* web_page_replay_util() { return &wpr_util_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<DownloadManagerObserver> download_manager_observer_;
  WebPageReplayUtil wpr_util_;
};

IN_PROC_BROWSER_TEST_F(BoxCapturedSitesInteractiveTest,
                       SFA_DownloadSmallFileSuccess) {
  SetCloudFSCPolicy(GetAllAllowedTestPolicy("797972721"));
  StartWprUsingFSCCaptureDir("box.com.sfa.wpr");

  EXPECT_FALSE(browser()->window()->IsDownloadShelfVisible());

  StartDownloadByNavigatingToEmbeddedServerUrl(
      "/enterprise/connectors/file_system/downloads/cipd/"
      "direct_download_gibben.epub");
  BoxDownloadItemObserver download_item_observer(
      download_manager_observer()->GetLatestDownloadItem());

  download_item_observer.WaitForSignInConfirmationDialog();
  download_item_observer.sign_in_observer()->AcceptBoxSigninConfirmation();

  // Make sure that the download shelf is showing.
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());

  // Bypass the Box signin and authorize dialog.
  download_item_observer.sign_in_observer()->AuthorizeWithUserAndPasswordSFA(
      GetBoxAccountUserName(), GetBoxAccountPassword());
  EXPECT_TRUE(
      download_item_observer.fetch_access_token_observer()->WaitForFetch());

  // Check that the download shelf is displaying the expected "uploading"
  // text.
  DownloadItemView* item_view = GetItemViewForLastDownload();
  download_item_observer.upload_observer()->WaitForUploadStart();
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_DOWNLOAD_STATUS_UPLOADING,
                l10n_util::GetStringUTF16(IDS_FILE_SYSTEM_CONNECTOR_BOX)),
            item_view->GetStatusTextForTesting());

  EXPECT_TRUE(
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
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(browser()->tab_strip_model()->GetWebContentsAt(1)->GetURL(),
            download_item_observer.upload_observer()->GetFileUrl());
}

IN_PROC_BROWSER_TEST_F(BoxCapturedSitesInteractiveTest,
                       MFA_DownloadSmallFileSuccess) {
  SetCloudFSCPolicy(GetAllAllowedTestPolicy("611447719"));
  StartWprUsingFSCCaptureDir("box.com.mfa.wpr");

  StartDownloadByNavigatingToEmbeddedServerUrl(
      "/enterprise/connectors/file_system/downloads/cipd/"
      "direct_download_gibben.epub");
  BoxDownloadItemObserver download_item_observer(
      download_manager_observer()->GetLatestDownloadItem());

  download_item_observer.WaitForSignInConfirmationDialog();
  download_item_observer.sign_in_observer()->AcceptBoxSigninConfirmation();

  // Bypass the Box signin and authorize dialog.
  download_item_observer.sign_in_observer()->AuthorizeWithUserAndPassword2FA(
      GetBoxAccountUserName(), GetBoxAccountPassword(), "123456");
  EXPECT_TRUE(
      download_item_observer.fetch_access_token_observer()->WaitForFetch());

  EXPECT_TRUE(
      download_item_observer.upload_observer()->WaitForUploadCompletion());
  download_manager_observer()->WaitForDownloadToFinish();
  EXPECT_TRUE(
      download_item_observer.upload_observer()->WaitForTmpFileDeletion());
}

IN_PROC_BROWSER_TEST_F(BoxCapturedSitesInteractiveTest,
                       DownloadLargeFileSuccess) {
  SetCloudFSCPolicy(GetAllAllowedTestPolicy("797972721"));
  StartWprUsingFSCCaptureDir("box.com.large_download.wpr");

  StartDownloadByNavigatingToEmbeddedServerUrl(
      "/enterprise/connectors/file_system/downloads/cipd/"
      "large_download_gibben.mobi");
  BoxDownloadItemObserver download_item_observer(
      download_manager_observer()->GetLatestDownloadItem());

  // Sign in to authorize Chrome to upload to Box.com.
  download_item_observer.WaitForSignInConfirmationDialog();
  download_item_observer.sign_in_observer()->AcceptBoxSigninConfirmation();
  download_item_observer.sign_in_observer()->AuthorizeWithUserAndPasswordSFA(
      GetBoxAccountUserName(), GetBoxAccountPassword());

  EXPECT_TRUE(
      download_item_observer.fetch_access_token_observer()->WaitForFetch());

  // Check that the download shelf is displaying the expected "uploading"
  // text.
  DownloadItemView* item_view = GetItemViewForLastDownload();
  download_item_observer.upload_observer()->WaitForUploadStart();
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_DOWNLOAD_STATUS_UPLOADING,
                l10n_util::GetStringUTF16(IDS_FILE_SYSTEM_CONNECTOR_BOX)),
            item_view->GetStatusTextForTesting());

  EXPECT_TRUE(
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
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(browser()->tab_strip_model()->GetWebContentsAt(1)->GetURL(),
            download_item_observer.upload_observer()->GetFileUrl());
}

IN_PROC_BROWSER_TEST_F(BoxCapturedSitesInteractiveTest, EnterpriseIdMismatch) {
  SetCloudFSCPolicy(GetAllAllowedTestPolicy("123456789"));
  StartWprUsingFSCCaptureDir("box.com.ent.id.mismatch.wpr");

  StartDownloadByNavigatingToEmbeddedServerUrl(
      "/enterprise/connectors/file_system/downloads/cipd/"
      "direct_download_gibben.epub");
  BoxDownloadItemObserver download_item_observer(
      download_manager_observer()->GetLatestDownloadItem());

  download_item_observer.WaitForSignInConfirmationDialog();
  download_item_observer.sign_in_observer()->AcceptBoxSigninConfirmation();

  // Bypass the Box signin and authorize dialog.
  download_item_observer.sign_in_observer()->AuthorizeWithUserAndPasswordSFA(
      GetBoxAccountUserName(), GetBoxAccountPassword());
  EXPECT_FALSE(
      download_item_observer.fetch_access_token_observer()->WaitForFetch());

  // The sign in confirmation dialog will relaunch after the enterprise ID
  // mismatch error. Close the confirmation dialog.
  download_item_observer.WaitForSignInConfirmationDialog();
  download_item_observer.sign_in_observer()->CancelBoxSignInConfirmation();

  download_manager_observer()->WaitForDownloadToFinish();
  EXPECT_TRUE(
      download_item_observer.upload_observer()->WaitForTmpFileDeletion());

  // Check that the download shelf is displaying the expected "upload
  // cancelled" text.
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
  DownloadItemView* item_view = GetItemViewForLastDownload();
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_CANCELLED),
            item_view->GetStatusTextForTesting());
}

IN_PROC_BROWSER_TEST_F(BoxCapturedSitesInteractiveTest,
                       CancelSignInConfirmation) {
  SetCloudFSCPolicy(GetAllAllowedTestPolicy("797972721"));
  StartWprUsingFSCCaptureDir("box.com.cancel.sign.in.confirmation.wpr");

  StartDownloadByNavigatingToEmbeddedServerUrl(
      "/enterprise/connectors/file_system/downloads/"
      "small_download.zip");
  BoxDownloadItemObserver download_item_observer(
      download_manager_observer()->GetLatestDownloadItem());

  download_item_observer.WaitForSignInConfirmationDialog();
  download_item_observer.sign_in_observer()->CancelBoxSignInConfirmation();
  EXPECT_FALSE(
      download_item_observer.upload_observer()->WaitForUploadCompletion());
  EXPECT_TRUE(
      download_item_observer.upload_observer()->WaitForTmpFileDeletion());

  // Check that the download shelf is displaying the expected "upload
  // cancelled" text.
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
  DownloadItemView* item_view = GetItemViewForLastDownload();
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_CANCELLED),
            item_view->GetStatusTextForTesting());
}

IN_PROC_BROWSER_TEST_F(BoxCapturedSitesInteractiveTest, ExitSignInDialog) {
  SetCloudFSCPolicy(GetAllAllowedTestPolicy("797972721"));
  StartWprUsingFSCCaptureDir("box.com.sign.in.fail.wpr");

  StartDownloadByNavigatingToEmbeddedServerUrl(
      "/enterprise/connectors/file_system/downloads/"
      "small_download.zip");
  BoxDownloadItemObserver download_item_observer(
      download_manager_observer()->GetLatestDownloadItem());

  download_item_observer.WaitForSignInConfirmationDialog();
  download_item_observer.sign_in_observer()->AcceptBoxSigninConfirmation();
  download_item_observer.sign_in_observer()->SubmitInvalidSignInCredentials(
      GetBoxAccountUserName(), GetBoxAccountPassword());
  download_item_observer.sign_in_observer()->CloseSignInWidget();
  EXPECT_FALSE(
      download_item_observer.upload_observer()->WaitForUploadCompletion());
  EXPECT_TRUE(
      download_item_observer.upload_observer()->WaitForTmpFileDeletion());

  // Check that the download shelf is displaying the expected "upload
  // cancelled" text.
  EXPECT_TRUE(browser()->window()->IsDownloadShelfVisible());
  DownloadItemView* item_view = GetItemViewForLastDownload();
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_CANCELLED),
            item_view->GetStatusTextForTesting());
}

}  // namespace enterprise_connectors
