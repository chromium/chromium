// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/browsertest_helper.h"

#include "base/json/json_reader.h"
#include "base/json/string_escape.h"
#include "base/path_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/test_switches.h"
#include "services/network/public/cpp/network_switches.h"

namespace content {
class DownloadManagerDelegate;
}

namespace enterprise_connectors {

std::string GetAllAllowedTestPolicy(const char* enterprise_id) {
  const char kConnectorPolicyString[] = R"PREFIX(
        [ {
           "domain": "google.com",
           "enable": [ {
               "mime_types": [ "*" ],
               "url_list": [ "*" ]
           } ],
           "enterprise_id": "%s",
           "service_provider": "box"
        } ])PREFIX";
  return base::StringPrintf(kConnectorPolicyString, enterprise_id);
}

std::string GetTestPolicyWithEnabledFilter(const char* enterprise_id,
                                           const char* include_url,
                                           const char* include_mime) {
  const char kConnectorPolicyString[] = R"PREFIX(
        [ {
           "domain": "google.com",
           "enable": [ {
               "mime_types": [ "%s" ],
               "url_list": [ "%s" ]
           } ],
           "enterprise_id": "%s",
           "service_provider": "box"
        } ])PREFIX";
  return base::StringPrintf(kConnectorPolicyString, include_mime, include_url,
                            enterprise_id);
}

std::string GetTestPolicyWithDisabledFilter(const char* enterprise_id,
                                            const char* exclude_url,
                                            const char* exclude_mime) {
  const char kConnectorPolicyString[] = R"PREFIX(
        [ {
           "domain": "google.com",
           "enable": [ {
               "mime_types": [ "*" ],
               "url_list": [ "*" ]
           } ],
           "disable": [ {
               "mime_types": [ "%s" ],
               "url_list": [ "%s" ]
           } ],
           "enterprise_id": "%s",
           "service_provider": "box"
        } ])PREFIX";
  return base::StringPrintf(kConnectorPolicyString, exclude_mime, exclude_url,
                            enterprise_id);
}

// BoxSignInObserver
BoxSignInObserver::BoxSignInObserver(FileSystemRenameHandler* rename_handler) {
  InitForTesting(rename_handler);  // IN-TEST
}

BoxSignInObserver::~BoxSignInObserver() {
  if (sign_in_widget_)
    sign_in_widget_->RemoveObserver(this);
}

void BoxSignInObserver::AcceptSignInConfirmation() {
  ASSERT_TRUE(signin_confirmation_dlg_);
  signin_confirmation_dlg_->AcceptDialog();
  WaitForSignInDialogToShow();
}

void BoxSignInObserver::CancelSignInConfirmation() {
  ASSERT_TRUE(signin_confirmation_dlg_);
  signin_confirmation_dlg_->CancelDialog();
}

void BoxSignInObserver::AuthorizeWithUserAndPasswordSFA(
    const std::string& username,
    const std::string& password) {
  if (current_page_ != Page::kSignin)
    WaitForPageLoad();
  ASSERT_EQ(current_page_, Page::kSignin)
      << "The sign-in dialog did not load the account sign in page!";
  // Set username and password, then click the submit button.
  ASSERT_TRUE(content::ExecuteScript(
      web_contents(), GetSubmitAccountSignInScript(username, password)))
      << "Failed to execute script to sign in!";
  WaitForPageLoad();
  ASSERT_EQ(current_page_, Page::kAuth)
      << "The Sign In dialog did not load the authorization page!";
  ASSERT_NO_FATAL_FAILURE(WaitForSignInDialogToClose(base::BindOnce(
      [](const content::ToRenderFrameHost& adapter) {
        ASSERT_TRUE(content::ExecuteScript(adapter, GetClickAuthorizeScript()))
            << "Failed to execute script to authorize access!";
      },
      std::move(web_contents()))));
}

// Bypass 2-Factor-Authentication sign in and authorize
// Chrome to access Box.com resources.
void BoxSignInObserver::AuthorizeWithUserAndPassword2FA(
    const std::string& username,
    const std::string& password,
    const std::string& sms_code,
    bool manually_submit_sms_code) {
  if (current_page_ != Page::kSignin)
    WaitForPageLoad();
  ASSERT_EQ(current_page_, Page::kSignin)
      << "The Sign In dialog did not load the account sign in page!";
  // Set username and password, then click the authorize button.
  ASSERT_TRUE(content::ExecuteScript(
      web_contents(), GetSubmitAccountSignInScript(username, password)))
      << "Failed to execute script to sign in!";
  WaitForPageLoad();

  ASSERT_EQ(current_page_, Page::kSignin)
      << "The Sign In dialog did not load the sms code page!";
  // In replay mode, supply the temporary password given as a parameter.
  if (manually_submit_sms_code) {
    // If test is running the recording mode or live mode, one must manually
    // supply a valid Short Message Service code.
    VLOG(0) << "Please submit the Box.com sms code into the signin dialog.";
  } else {
    ASSERT_TRUE(content::ExecuteScript(web_contents(),
                                       GetSubmitSmsCodeScript(sms_code)))
        << "Failed to execute script to submit the sms code!";
  }
  WaitForPageLoad();

  ASSERT_EQ(current_page_, Page::kAuth)
      << "The Sign In dialog did not load the authorization page!";
  ASSERT_NO_FATAL_FAILURE(WaitForSignInDialogToClose(base::BindOnce(
      [](const content::ToRenderFrameHost& adapter) {
        ASSERT_TRUE(content::ExecuteScript(adapter, GetClickAuthorizeScript()))
            << "Failed to execute script to authorize access!";
      },
      std::move(web_contents()))));
}

void BoxSignInObserver::SubmitInvalidSignInCredentials(
    const std::string& username,
    const std::string& password) {
  if (current_page_ != Page::kSignin)
    WaitForPageLoad();
  ASSERT_EQ(current_page_, Page::kSignin)
      << "The Sign In dialog did not load the account sign in page!";
  // Set username and password, then click the authorize button.
  ASSERT_TRUE(content::ExecuteScript(
      web_contents(), GetSubmitAccountSignInScript(username, password)))
      << "Failed to execute script to submit invalid credentials!";
  WaitForPageLoad();
  ASSERT_EQ(current_page_, Page::kSignin)
      << "The Sign In dialog did not reload the account sign in page!";
}

bool BoxSignInObserver::GetUserNameFromSignInPage(std::string* result) {
  DCHECK_EQ(current_page_, Page::kSignin);

  const std::string get_login_value = R"(
      window.domAutomationController.send(
          document.getElementById('login').value);
    )";
  return ExecuteScriptAndExtractString(web_contents()->GetMainFrame(),
                                       get_login_value, result);
}

void BoxSignInObserver::CloseSignInWidget() {
  WaitForSignInDialogToClose(base::BindOnce(
      [](views::Widget* dialog) { dialog->Close(); }, sign_in_widget_));
  sign_in_widget_ = nullptr;
}

void BoxSignInObserver::WaitForPageLoad() {
  base::RunLoop run_loop;
  stop_waiting_for_page_load_ = run_loop.QuitClosure();
  run_loop.Run();
}

void BoxSignInObserver::WaitForSignInConfirmationDialog() {
  if (signin_confirmation_dlg_)
    return;
  base::RunLoop run_loop;
  stop_waiting_for_signin_confirmation_ = run_loop.QuitClosure();
  run_loop.Run();
}

void BoxSignInObserver::WaitForSignInDialogToShow() {
  if (sign_in_widget_ == nullptr) {
    base::RunLoop run_loop;
    stop_waiting_for_widget_to_show_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  if (current_page_ != Page::kSignin)
    WaitForPageLoad();
}

void BoxSignInObserver::WaitForSignInDialogToClose(
    base::OnceClosure trigger_close_action) {
  base::RunLoop run_loop;
  stop_waiting_for_dialog_shutdown_ = run_loop.QuitClosure();
  expecting_dialog_shutdown_ = true;
  ASSERT_NO_FATAL_FAILURE(std::move(trigger_close_action).Run());
  run_loop.Run();
}

void BoxSignInObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  const GURL& url = navigation_handle->GetURL();
  if (IsBoxSignInURI(url))
    current_page_ = Page::kSignin;
  else if (IsBoxAuthorizeURI(url))
    current_page_ = Page::kAuth;
  else
    current_page_ = Page::kUnknown;
}

void BoxSignInObserver::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (stop_waiting_for_page_load_) {
    std::move(stop_waiting_for_page_load_).Run();
    stop_waiting_for_page_load_.Reset();
  }
}

void BoxSignInObserver::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(sign_in_widget_, widget);
  // Report unexpected shut down as an error.
  // Note that ASSERT macros can only be called on subroutines that executes
  // in the same thread as the test body function. This subroutine executes
  // in a separate thread.
  // If the sign in dialog closes unexpectedly, the main test body will catch
  // the error and abort.
  EXPECT_TRUE(expecting_dialog_shutdown_)
      << R"(Detected unexpected shutdown of the sign in dialog!
              Test should abort)";
  sign_in_widget_ = nullptr;
  if (stop_waiting_for_dialog_shutdown_)
    std::move(stop_waiting_for_dialog_shutdown_).Run();
}

void BoxSignInObserver::OnWidgetVisibilityChanged(views::Widget* widget,
                                                  bool visible) {
  DCHECK_EQ(sign_in_widget_, widget);
  if (visible && stop_waiting_for_widget_to_show_)
    std::move(stop_waiting_for_widget_to_show_).Run();
}

void BoxSignInObserver::OnConfirmationDialogCreated(
    views::DialogDelegate* confirmation_dialog_delegate) {
  signin_confirmation_dlg_ = confirmation_dialog_delegate;
  if (stop_waiting_for_signin_confirmation_)
    std::move(stop_waiting_for_signin_confirmation_).Run();
}

void BoxSignInObserver::OnSignInDialogCreated(
    content::WebContents* dialog_web_content,
    FileSystemSigninDialogDelegate* dialog_delegate,
    views::Widget* dialog_widget) {
  this->Observe(dialog_web_content);
  dialog_widget->AddObserver(this);
  sign_in_widget_ = dialog_widget;
  sign_in_dlg_ = dialog_delegate;
}

void BoxSignInObserver::BypassSignInWebsite(
    const GoogleServiceAuthError& status,
    const std::string& access_token,
    const std::string& refresh_token) {
  expecting_dialog_shutdown_ = true;
  sign_in_dlg_->OnGotOAuthTokens(status, access_token, refresh_token);
}

// static
bool BoxSignInObserver::IsBoxSignInURI(const GURL& url) {
  return url.host() == "account.box.com" &&
         url.path() == "/api/oauth2/authorize";
}

// static
bool BoxSignInObserver::IsBoxAuthorizeURI(const GURL& url) {
  return url.host() == "app.box.com" && url.path() == "/api/oauth2/authorize";
}

// static
std::string BoxSignInObserver::GetSubmitAccountSignInScript(
    const std::string& username,
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

// static
std::string BoxSignInObserver::GetSubmitSmsCodeScript(
    const std::string& password) {
  return base::StringPrintf(
      R"PREFIX(
        (function() {
          document.getElementById('2fa_sms_code').value = `%s`;
          document.querySelector('button[type="submit"]').click();
        })();
        )PREFIX",
      password.c_str());
}

// static
std::string BoxSignInObserver::GetClickAuthorizeScript() {
  return R"PREFIX(
        (function() {
          document.getElementById('consent_accept_button').click();
        })();
        )PREFIX";
}

// BoxDownloadItemObserver
BoxDownloadItemObserver::BoxDownloadItemObserver(download::DownloadItem* item)
    : download_item_(item) {
  download_item_->AddObserver(this);
}

BoxDownloadItemObserver::~BoxDownloadItemObserver() {
  if (download_item_)
    download_item_->RemoveObserver(this);
}

void BoxDownloadItemObserver::OnDownloadDestroyed(
    download::DownloadItem* item) {
  DCHECK_EQ(item, download_item_);
  download_item_ = nullptr;
}

void BoxDownloadItemObserver::OnRenameHandlerCreated(
    download::DownloadItem* item) {
  ASSERT_TRUE(item->GetRenameHandler());
  rename_handler_created_ = true;
  FileSystemRenameHandler* rename_handler =
      static_cast<FileSystemRenameHandler*>(item->GetRenameHandler());
  rename_start_observer_ =
      std::make_unique<RenameStartObserver>(rename_handler);
  sign_in_observer_ = std::make_unique<BoxSignInObserver>(rename_handler);
  fetch_access_token_observer_ =
      std::make_unique<BoxFetchAccessTokenTestObserver>(rename_handler);
  upload_observer_ =
      std::make_unique<BoxUploader::TestObserver>(rename_handler);

  if (!stop_waiting_for_rename_handler_creation_.is_null()) {
    std::move(stop_waiting_for_rename_handler_creation_).Run();
    stop_waiting_for_rename_handler_creation_.Reset();
  }

  if (!stop_waiting_for_download_near_completion_.is_null()) {
    std::move(stop_waiting_for_download_near_completion_).Run();
    stop_waiting_for_download_near_completion_.Reset();
  }
}

void BoxDownloadItemObserver::OnDownloadUpdated(download::DownloadItem* item) {
  DCHECK_EQ(item, download_item_);

  if ((item->GetState() !=
       download::DownloadItem::DownloadState::IN_PROGRESS) &&
      (!stop_waiting_for_download_near_completion_.is_null())) {
    // The download is no longer in progress (either because download
    // completed, or because it was interrupted or cancelled). We can exit the
    // wait loop for download to be near completion.
    std::move(stop_waiting_for_download_near_completion_).Run();
    stop_waiting_for_download_near_completion_.Reset();
  }

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

  OnRenameHandlerCreated(item);
}

void BoxDownloadItemObserver::WaitForDownloadItemRerouteInfo() {
  if ((download_item_->GetState() ==
       download::DownloadItem::DownloadState::IN_PROGRESS) &&
      (!rename_handler_created_)) {
    base::RunLoop run_loop;
    stop_waiting_for_download_near_completion_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  if (rename_start_observer_) {
    rename_start_observer_->WaitForStart();
  }
}

void BoxDownloadItemObserver::WaitForRenameHandlerCreation() {
  if (!rename_handler_created_) {
    base::RunLoop run_loop;
    stop_waiting_for_rename_handler_creation_ = run_loop.QuitClosure();
    run_loop.Run();
  }
}

void BoxDownloadItemObserver::WaitForSignInConfirmationDialog() {
  WaitForRenameHandlerCreation();
  ASSERT_TRUE(sign_in_observer_);
  sign_in_observer_->WaitForSignInConfirmationDialog();
}

DownloadServiceProvider BoxDownloadItemObserver::GetServiceProvider() {
  auto& reroute_info = download_item_->GetRerouteInfo();
  if (!reroute_info.has_service_provider())
    return DownloadServiceProvider::kLocal;
  if (reroute_info.service_provider() == FileSystemServiceProvider::BOX)
    return DownloadServiceProvider::kBox;
  return DownloadServiceProvider::kUnknown;
}

// FileSystemConnectorBrowserTestBase
void FileSystemConnectorBrowserTestBase::SetUpOnMainThread() {
  // Set up a localhost server to serve files for download.
  base::FilePath test_file_directory;
  ASSERT_TRUE(
      base::PathService::Get(chrome::DIR_TEST_DATA, &test_file_directory));
  embedded_test_server()->ServeFilesFromDirectory(test_file_directory);
  ASSERT_TRUE(embedded_test_server()->Start());
}

void FileSystemConnectorBrowserTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kFileSystemConnectorEnabled},
      /*disabled_features=*/{});
}

void FileSystemConnectorBrowserTestBase::TearDownOnMainThread() {
  // Make sure any pending requests have finished
  base::RunLoop().RunUntilIdle();
}

void FileSystemConnectorBrowserTestBase::SetCloudFSCPolicy(
    const std::string& policy_value) {
  browser()->profile()->GetPrefs()->Set(
      ConnectorPref(FileSystemConnector::SEND_DOWNLOAD_TO_CLOUD),
      *base::JSONReader::Read(policy_value.c_str()));
  // Verify that the FSC is enabled.
  ASSERT_TRUE(IsFSCEnabled());
}

bool FileSystemConnectorBrowserTestBase::IsFSCEnabled() {
  auto* service =
      ConnectorsServiceFactory::GetForBrowserContext(browser()->profile());
  return service->IsConnectorEnabled(
      FileSystemConnector::SEND_DOWNLOAD_TO_CLOUD);
}

}  // namespace enterprise_connectors
