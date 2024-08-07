// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class pairs with DownloadController on Java side to forward requests
// for GET downloads to the current DownloadListener. POST downloads are
// handled on the native side.
//
// Both classes are Singleton classes. C++ object owns Java object.
//
// Call sequence
// GET downloads:
// DownloadController::CreateGETDownload() =>
// DownloadController.newHttpGetDownload() =>
// DownloadListener.onDownloadStart() /
// DownloadListener2.requestHttpGetDownload()
//

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_CONTROLLER_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_CONTROLLER_H_

#include <map>
#include <string>
#include <utility>

#include "base/android/scoped_java_ref.h"
#include "base/memory/singleton.h"
#include "chrome/browser/download/android/dangerous_download_dialog_bridge.h"
#include "chrome/browser/download/android/download_callback_validator.h"
#include "chrome/browser/download/android/download_controller_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_bridge.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_util.h"

class DownloadController : public DownloadControllerBase {
 public:
  static DownloadController* GetInstance();

  DownloadController(const DownloadController&) = delete;
  DownloadController& operator=(const DownloadController&) = delete;

  // DownloadControllerBase implementation.
  void AcquireFileAccessPermission(
      const content::WebContents::Getter& wc_getter,
      AcquireFileAccessPermissionCallback callback) override;
  void CreateAndroidDownload(const content::WebContents::Getter& wc_getter,
                             const DownloadInfo& info) override;

  // UMA histogram enum for download storage permission requests. Keep this
  // in sync with MobileDownloadStoragePermission in histograms.xml. This should
  // be append only.
  enum StoragePermissionType {
    STORAGE_PERMISSION_REQUESTED = 0,
    STORAGE_PERMISSION_NO_ACTION_NEEDED,
    STORAGE_PERMISSION_GRANTED,
    STORAGE_PERMISSION_DENIED,
    STORAGE_PERMISSION_NO_WEB_CONTENTS,
    STORAGE_PERMISSION_MAX
  };
  static void RecordStoragePermission(StoragePermissionType type);

  // Close the |web_contents| for |download|. |download| could be null
  // if the download is created by Android DownloadManager.
  static void CloseTabIfEmpty(content::WebContents* web_contents,
                              download::DownloadItem* download);

  // Callback when user permission prompt finishes. Args: whether file access
  // permission is acquired, which permission to update.
  using AcquirePermissionCallback =
      base::OnceCallback<void(bool, const std::string&)>;

  DownloadCallbackValidator* validator() { return &validator_; }

 private:
  friend struct base::DefaultSingletonTraits<DownloadController>;
  friend class DownloadControllerTest;
  DownloadController();
  ~DownloadController() override;

  // DownloadControllerBase implementation.
  void OnDownloadStarted(download::DownloadItem* download_item) override;
  void StartContextMenuDownload(const content::ContextMenuParams& params,
                                content::WebContents* web_contents,
                                bool is_link) override;

  // DownloadItem::Observer interface.
  void OnDownloadUpdated(download::DownloadItem* item) override;
  void OnDownloadDestroyed(download::DownloadItem* item) override;

  // The download item contains dangerous file types.
  void OnDangerousDownload(download::DownloadItem* item);

  // Helper methods to start android download on UI thread.
  void StartAndroidDownload(const content::WebContents::Getter& wc_getter,
                            const DownloadInfo& info);
  void StartAndroidDownloadInternal(
      const content::WebContents::Getter& wc_getter,
      const DownloadInfo& info,
      bool allowed);

  // Get profile key from download item.
  ProfileKey* GetProfileKey(download::DownloadItem* download_item);

  // Callback after we prompt the user to enable app verification.
  void EnableVerifyAppsDone(download::DownloadItem* item,
                            safe_browsing::VerifyAppsEnabledResult result);

  // Notify Java that download is complete, so the user can be informed.
  void OnDownloadComplete(download::DownloadItem* item);

  // Whether or not we should show an app verification prompt for `item`
  bool ShouldShowAppVerificationPrompt(download::DownloadItem* item);

  std::string default_file_name_;

  DownloadCallbackValidator validator_;

  std::unique_ptr<DangerousDownloadDialogBridge> dangerous_download_bridge_;

  // The item currently or previously doing an app verification
  // prompt. Because we show at most one at a time, this does not need
  // to be a set.
  raw_ptr<download::DownloadItem> app_verification_prompt_download_ = nullptr;
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_CONTROLLER_H_
