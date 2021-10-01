// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BROWSERTEST_HELPER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BROWSERTEST_HELPER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/task/current_thread.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/file_system/box_uploader.h"
#include "chrome/browser/enterprise/connectors/file_system/rename_handler.h"
#include "chrome/browser/enterprise/connectors/file_system/signin_experience.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/policy_constants.h"
#include "components/variations/variations_params_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace enterprise_connectors {

enum DownloadServiceProvider { kLocal, kBox, kUnknown };

std::string GetAllAllowedTestPolicy(const char* enterprise_id);
std::string GetTestPolicyWithEnabledFilter(const char* enterprise_id,
                                           const char* include_url,
                                           const char* include_mime);
std::string GetTestPolicyWithDisabledFilter(const char* enterprise_id,
                                            const char* exclude_url,
                                            const char* exclude_mime);

class BoxSignInObserver : public SigninExperienceTestObserver,
                          public content::WebContentsObserver,
                          public views::WidgetObserver {
 public:
  enum class Page { kSignin, kAuth, kUnknown };

  explicit BoxSignInObserver(FileSystemRenameHandler* rename_handler);
  ~BoxSignInObserver() override;

  // Accept the Sign in confirmation dialog to bring up the Box.com
  // sign in dialog.
  void AcceptSignInConfirmation();

  void CancelSignInConfirmation();

  // Bypass Single-Factor-Authentication sign in and authorize
  // Chrome to access Box.com resources.
  void AuthorizeWithUserAndPasswordSFA(const std::string& username,
                                       const std::string& password);

  // Bypass 2-Factor-Authentication sign in and authorize
  // Chrome to access Box.com resources.
  void AuthorizeWithUserAndPassword2FA(const std::string& username,
                                       const std::string& password,
                                       const std::string& sms_code,
                                       bool manually_submit_sms_code);

  void SubmitInvalidSignInCredentials(const std::string& username,
                                      const std::string& password);

  bool GetUserNameFromSignInPage(std::string* result);

  void CloseSignInWidget();
  void WaitForPageLoad();
  void WaitForSignInConfirmationDialog();
  void WaitForSignInDialogToShow();
  void WaitForSignInDialogToClose(base::OnceClosure trigger_close_action);

  // content::WebContentsObserver
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  // views::WidgetObserver
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;

  // SigninExperienceTestObserver
  void OnConfirmationDialogCreated(
      views::DialogDelegate* confirmation_dialog_delegate) override;

  void OnSignInDialogCreated(content::WebContents* dialog_web_content,
                             FileSystemSigninDialogDelegate* dialog_delegate,
                             views::Widget* dialog_widget) override;
  void BypassSignInWebsite(const GoogleServiceAuthError& status,
                           const std::string& access_token,
                           const std::string& refresh_token);

 private:
  static bool IsBoxSignInURI(const GURL& url);
  static bool IsBoxAuthorizeURI(const GURL& url);
  static std::string GetSubmitAccountSignInScript(const std::string& username,
                                                  const std::string& password);
  static std::string GetSubmitSmsCodeScript(const std::string& password);
  static std::string GetClickAuthorizeScript();

  Page current_page_ = Page::kUnknown;
  // This bool variable allows this class to differentiate an expected dialog
  // closure from unexpected dialog shutdown/crash/exits. Before triggering an
  // action to close the dialog, the test class will set this variable to true.
  bool expecting_dialog_shutdown_ = false;
  raw_ptr<views::DialogDelegate> signin_confirmation_dlg_ = nullptr;
  raw_ptr<views::Widget> sign_in_widget_ = nullptr;
  raw_ptr<FileSystemSigninDialogDelegate> sign_in_dlg_ = nullptr;
  std::unique_ptr<base::RunLoop> run_loop_;
  base::OnceClosure stop_waiting_for_signin_confirmation_;
  base::OnceClosure stop_waiting_for_page_load_;
  base::OnceClosure stop_waiting_for_widget_to_show_;
  base::OnceClosure stop_waiting_for_dialog_shutdown_;
  base::OnceClosure stop_waiting_for_authorization_completion_;
};

class BoxDownloadItemObserver : public download::DownloadItem::Observer {
 public:
  explicit BoxDownloadItemObserver(download::DownloadItem* item);
  ~BoxDownloadItemObserver() override;

  void OnDownloadDestroyed(download::DownloadItem* item) override;

  void OnRenameHandlerCreated(download::DownloadItem* item);

  void OnDownloadUpdated(download::DownloadItem* item) override;

  // Wait for when the reroute info on a download::DownloadItem is accurate.
  // If the DownloadItem downloads to a local file, then this method should
  // stop when the download completes.
  // If the DownloadItem will be rerouted, then this method should stop when
  // the DownloadItem's rename handler kicks off.
  void WaitForDownloadItemRerouteInfo();
  void WaitForRenameHandlerCreation();
  void WaitForSignInConfirmationDialog();

  DownloadServiceProvider GetServiceProvider();
  download::DownloadItem* download_item() { return download_item_; }
  BoxSignInObserver* sign_in_observer() { return sign_in_observer_.get(); }
  BoxFetchAccessTokenTestObserver* fetch_access_token_observer() {
    return fetch_access_token_observer_.get();
  }
  BoxUploader::TestObserver* upload_observer() {
    return upload_observer_.get();
  }

 private:
  raw_ptr<download::DownloadItem> download_item_ = nullptr;
  base::OnceClosure stop_waiting_for_rename_handler_creation_;
  base::OnceClosure stop_waiting_for_download_near_completion_;
  bool rename_handler_created_ = false;
  std::unique_ptr<RenameStartObserver> rename_start_observer_;
  std::unique_ptr<BoxSignInObserver> sign_in_observer_;
  std::unique_ptr<BoxFetchAccessTokenTestObserver> fetch_access_token_observer_;
  std::unique_ptr<BoxUploader::TestObserver> upload_observer_;
};

class FileSystemConnectorBrowserTestBase : public InProcessBrowserTest {
 public:
  FileSystemConnectorBrowserTestBase() = default;
  ~FileSystemConnectorBrowserTestBase() override = default;

 protected:
  void SetUpOnMainThread() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void TearDownOnMainThread() override;

  void SetCloudFSCPolicy(const std::string& policy_value);
  bool IsFSCEnabled();

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BROWSERTEST_HELPER_H_
