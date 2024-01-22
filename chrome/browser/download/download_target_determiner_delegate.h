// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TARGET_DETERMINER_DELEGATE_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TARGET_DETERMINER_DELEGATE_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "chrome/browser/download/download_confirmation_reason.h"
#include "chrome/browser/download/download_confirmation_result.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_path_reservation_tracker.h"
#include "components/download/public/common/download_utils.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace base {
class FilePath;
}

// Delegate for DownloadTargetDeterminer. The delegate isn't owned by
// DownloadTargetDeterminer and is expected to outlive it.
class DownloadTargetDeterminerDelegate {
 public:
  // Callback to be invoked after GetInsecureDownloadStatus() completes. The
  // |should_block| bool represents whether the download should be aborted.
  using GetInsecureDownloadStatusCallback = base::OnceCallback<void(
      download::DownloadItem::InsecureDownloadStatus status)>;

  // Callback to be invoked after NotifyExtensions() completes. The
  // |new_virtual_path| should be set to a new path if an extension wishes to
  // override the download path. |conflict_action| should be set to the action
  // to take if a file exists at |new_virtual_path|. If |new_virtual_path| is
  // empty, then the download target will be unchanged and |conflict_action| is
  // ignored.
  typedef base::OnceCallback<void(
      const base::FilePath& new_virtual_path,
      download::DownloadPathReservationTracker::FilenameConflictAction
          conflict_action)>
      NotifyExtensionsCallback;

  // Callback to be invoked when ReserveVirtualPath() completes.
  using ReservedPathCallback =
      download::DownloadPathReservationTracker::ReservedPathCallback;

  // Callback to be invoked when RequestConfirmation() completes.
  // |selected_file_info|: The file chosen by the user, or a value with an empty
  // path if the user cancels the file selection.
  using ConfirmationCallback =
      base::OnceCallback<void(DownloadConfirmationResult,
                              const ui::SelectedFileInfo& selected_file_info)>;

  // Callback to be invoked when RequestIncognitoWarningConfirmation()
  // completes.
  // |accepted|: boolean saying if user accepted or the prompt was
  // dismissed.
  using IncognitoWarningConfirmationCallback =
      base::OnceCallback<void(bool /*accepted*/)>;

  // Callback to be invoked after CheckDownloadUrl() completes. The parameter to
  // the callback should indicate the danger type of the download based on the
  // results of the URL check.
  using CheckDownloadUrlCallback =
      base::OnceCallback<void(download::DownloadDangerType danger_type)>;

  // Callback to be invoked after GetFileMimeType() completes. The parameter
  // should be the MIME type of the requested file. If no MIME type can be
  // determined, it should be set to the empty string.
  typedef base::OnceCallback<void(const std::string&)> GetFileMimeTypeCallback;

  // Returns whether the download should be warned/blocked based on its insecure
  // download status, and if so, what kind of warning/blocking should be used.
  virtual void GetInsecureDownloadStatus(
      download::DownloadItem* download,
      const base::FilePath& virtual_path,
      GetInsecureDownloadStatusCallback callback) = 0;

  // Notifies extensions of the impending filename determination. |virtual_path|
  // is the current suggested virtual path. The |callback| should be invoked to
  // indicate whether any extensions wish to override the path.
  virtual void NotifyExtensions(download::DownloadItem* download,
                                const base::FilePath& virtual_path,
                                NotifyExtensionsCallback callback) = 0;

  // Reserve |virtual_path|. This is expected to check the following:
  // - Whether |virtual_path| can be written to by the user. If not, the
  //   |virtual_path| can be changed to writeable path if necessary.
  // - If |conflict_action| is UNIQUIFY then |virtual_path| should be
  //   modified so that the new path is writeable and unique. If
  //   |conflict_action| is PROMPT, then in the event of a conflict,
  //   |callback| should be invoked with |success| set to |false| in
  //   order to force a prompt. |virtual_path| may or may not be
  //   modified in the latter case.
  // - If |create_directory| is true, then the parent directory of
  //   |virtual_path| should be created if it doesn't exist.
  //
  // |callback| should be invoked on completion with the results.
  virtual void ReserveVirtualPath(
      download::DownloadItem* download,
      const base::FilePath& virtual_path,
      bool create_directory,
      download::DownloadPathReservationTracker::FilenameConflictAction
          conflict_action,
      ReservedPathCallback callback) = 0;

  // Display a prompt to the user requesting that a download target be chosen.
  // Should invoke |callback| upon completion.
  virtual void RequestConfirmation(download::DownloadItem* download,
                                   const base::FilePath& virtual_path,
                                   DownloadConfirmationReason reason,
                                   ConfirmationCallback callback) = 0;
#if BUILDFLAG(IS_ANDROID)
  // Display a message prompt to the user containing an incognito warning.
  // Should invoke |callback| upon completion.
  virtual void RequestIncognitoWarningConfirmation(
      IncognitoWarningConfirmationCallback callback) = 0;
#endif
  // If |virtual_path| is not a local path, should return a possibly temporary
  // local path to use for storing the downloaded file. If |virtual_path| is
  // already local, then it should return the same path. |callback| should be
  // invoked to return the path.
  virtual void DetermineLocalPath(download::DownloadItem* download,
                                  const base::FilePath& virtual_path,
                                  download::LocalPathCallback callback) = 0;

  // Check whether the download URL is malicious and invoke |callback| with a
  // suggested danger type for the download.
  virtual void CheckDownloadUrl(download::DownloadItem* download,
                                const base::FilePath& virtual_path,
                                CheckDownloadUrlCallback callback) = 0;

  // Get the MIME type for the given file.
  virtual void GetFileMimeType(const base::FilePath& path,
                               GetFileMimeTypeCallback callback) = 0;

 protected:
  virtual ~DownloadTargetDeterminerDelegate();
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_TARGET_DETERMINER_DELEGATE_H_
