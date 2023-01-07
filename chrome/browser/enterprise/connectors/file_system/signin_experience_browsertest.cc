// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/connectors/file_system/browsertest_helper.h"

#include "base/files/file_path.h"
#include "chrome/browser/enterprise/connectors/file_system/rename_handler.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/fake_download_item.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::Invoke;
using testing::Return;

namespace enterprise_connectors {

class DownloadItemForBrowserTest : public content::FakeDownloadItem {
 public:
  explicit DownloadItemForBrowserTest(base::FilePath::StringPieceType file_name)
      : path_(file_name) {}
  const base::FilePath& GetFullPath() const override { return path_; }
  MOCK_METHOD(download::DownloadItemRenameHandler*,
              GetRenameHandler,
              (),
              (override));

 protected:
  const base::FilePath path_;
};

class SigninExperienceForDownloadItemBrowserTest
    : public FileSystemConnectorBrowserTestBase {
 public:
  SigninExperienceForDownloadItemBrowserTest() = default;
  ~SigninExperienceForDownloadItemBrowserTest() override = default;

  void SetUpOnMainThread() override {
    FileSystemConnectorBrowserTestBase::SetUpOnMainThread();
    SetCloudFSCPolicy(GetAllAllowedTestPolicy("797972721"));

    test_item.SetURL(GURL("https://renameme.com"));
    test_item.SetMimeType("text/plain");
    test_item.SetState(download::DownloadItem::IN_PROGRESS);
    web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);
    content::DownloadItemUtils::AttachInfoForTesting(
        &test_item, browser()->profile(), web_contents);

    settings = GetFileSystemSettings(&test_item);
    ASSERT_TRUE(settings.has_value());

    rename_handler = FileSystemRenameHandler::CreateIfNeeded(&test_item);
    ASSERT_TRUE(rename_handler);
    EXPECT_CALL(test_item, GetRenameHandler())
        .WillRepeatedly(Return(rename_handler.get()));

    download_item_observer =
        std::make_unique<BoxDownloadItemObserver>(&test_item);
    download_item_observer->OnDownloadUpdated(&test_item);
    ASSERT_TRUE(download_item_observer->sign_in_observer());
  }

  void OnAuthorization(const GoogleServiceAuthError& status,
                       const std::string& access_token,
                       const std::string& refresh_token) {
    auth_error_state = status.state();
  }

 protected:
  raw_ptr<content::WebContents> web_contents;
  absl::optional<FileSystemSettings> settings;
  DownloadItemForBrowserTest test_item{FILE_PATH_LITERAL("file.txt")};
  std::unique_ptr<download::DownloadItemRenameHandler> rename_handler;
  std::unique_ptr<BoxDownloadItemObserver> download_item_observer;

  GoogleServiceAuthError::State auth_error_state =
      GoogleServiceAuthError::NUM_STATES;
};

IN_PROC_BROWSER_TEST_F(SigninExperienceForDownloadItemBrowserTest,
                       CancelledConfirmationModal) {
  StartFileSystemConnectorSigninExperienceForDownloadItem(
      web_contents, settings.value(), browser()->profile()->GetPrefs(),
      base::BindOnce(
          &SigninExperienceForDownloadItemBrowserTest::OnAuthorization,
          base::Unretained(this)),
      download_item_observer->sign_in_observer());

  download_item_observer->sign_in_observer()->WaitForSignInConfirmationDialog();
  download_item_observer->sign_in_observer()->CancelSignInConfirmation();
  ASSERT_EQ(auth_error_state, GoogleServiceAuthError::REQUEST_CANCELED);
}

IN_PROC_BROWSER_TEST_F(SigninExperienceForDownloadItemBrowserTest,
                       CancelledSigninDialog) {
  StartFileSystemConnectorSigninExperienceForDownloadItem(
      web_contents, settings.value(), browser()->profile()->GetPrefs(),
      base::BindOnce(
          &SigninExperienceForDownloadItemBrowserTest::OnAuthorization,
          base::Unretained(this)),
      download_item_observer->sign_in_observer());

  download_item_observer->sign_in_observer()->WaitForSignInConfirmationDialog();
  ASSERT_NO_FATAL_FAILURE(
      download_item_observer->sign_in_observer()->AcceptSignInConfirmation());
  ASSERT_NO_FATAL_FAILURE(
      download_item_observer->sign_in_observer()->CloseSignInWidget());
  ASSERT_EQ(auth_error_state, GoogleServiceAuthError::REQUEST_CANCELED);
}

IN_PROC_BROWSER_TEST_F(SigninExperienceForDownloadItemBrowserTest,
                       ConfirmedModalAndAccessGranted) {
  StartFileSystemConnectorSigninExperienceForDownloadItem(
      web_contents, settings.value(), browser()->profile()->GetPrefs(),
      base::BindOnce(
          &SigninExperienceForDownloadItemBrowserTest::OnAuthorization,
          base::Unretained(this)),
      download_item_observer->sign_in_observer());

  download_item_observer->sign_in_observer()->WaitForSignInConfirmationDialog();
  ASSERT_NO_FATAL_FAILURE(
      download_item_observer->sign_in_observer()->AcceptSignInConfirmation());

  // Bypass the sign-in page and authorize dialog just to test that result from
  // gets passed back properly.
  ASSERT_NO_FATAL_FAILURE(
      download_item_observer->sign_in_observer()->BypassSignInWebsite(
          GoogleServiceAuthError::FromUnexpectedServiceResponse("testing"),
          "AToken", "RToken"));
  ASSERT_EQ(auth_error_state,
            GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE);
}

}  // namespace enterprise_connectors
