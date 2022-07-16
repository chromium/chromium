// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_SIGNIN_EXPERIENCE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_SIGNIN_EXPERIENCE_H_

#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/file_system/account_info_utils.h"
#include "chrome/browser/enterprise/connectors/file_system/signin_dialog_delegate.h"

namespace content {
class WebContents;
}

namespace enterprise_connectors {

class FileSystemRenameHandler;
class SigninExperienceTestObserver;

// Retrieve FileSystemSettings for |profile|; null if connector is disabled.
absl::optional<FileSystemSettings> GetFileSystemSettings(Profile* profile);

// Retrieve FileSystemSettings for |download_item|, considering its associated
// profile and originating URL; null if connector is disabled.
absl::optional<FileSystemSettings> GetFileSystemSettings(
    download::DownloadItem* download_item);

using AuthorizationCompletedCallback =
    FileSystemSigninDialogDelegate::AuthorizationCompletedCallback;

// Start the sign in experience as triggered by a download item.
void StartFileSystemConnectorSigninExperienceForDownloadItem(
    content::WebContents* web_contents,
    const FileSystemSettings& settings,
    PrefService* prefs,
    AuthorizationCompletedCallback callback,
    SigninExperienceTestObserver* test_observer = nullptr);

// If `enable_link` is true, start the sign in experience as triggered by
// settings page; else, unlink the existing account.
void SetFileSystemConnectorAccountLinkForSettingsPage(
    bool enable_link,
    Profile* profile,
    base::OnceCallback<void(bool)> callback,
    SigninExperienceTestObserver* test_observer = nullptr);

// Prefs for the settings page to observe to refresh the connection section.
std::vector<std::string> GetFileSystemConnectorPrefsForSettingsPage(
    Profile* profile);

absl::optional<AccountInfo> GetFileSystemConnectorLinkedAccountInfo(
    const FileSystemSettings& settings,
    PrefService* prefs);

// Run |callback| with a GoogleServiceAuthError that indicates cancellation.
void ReturnCancellation(AuthorizationCompletedCallback callback);

// Helper function/classes for testing.
class SigninExperienceTestObserver {
 public:
  SigninExperienceTestObserver();
  virtual ~SigninExperienceTestObserver();

  virtual void InitForTesting(FileSystemRenameHandler* rename_handler);
  virtual void OnConfirmationDialogCreated(
      views::DialogDelegate* dialog_delegate) {}
  virtual void OnSignInDialogCreated(
      content::WebContents* dialog_web_content,
      FileSystemSigninDialogDelegate* dialog_delegate,
      views::Widget* dialog_widget) {}

 private:
  base::WeakPtr<FileSystemRenameHandler> rename_handler_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_SIGNIN_EXPERIENCE_H_
