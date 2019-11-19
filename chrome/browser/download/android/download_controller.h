// Copyright 2016 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/download/android/download_controller_base.h"

class DownloadController : public DownloadControllerBase {
 public:
  static DownloadController* GetInstance();

  // DownloadControllerBase implementation.
  void AcquireFileAccessPermission(
      const content::WebContents::Getter& wc_getter,
      AcquireFileAccessPermissionCallback callback) override;
  void CreateAndroidDownload(const content::WebContents::Getter& wc_getter,
                             const DownloadInfo& info) override;
  void AboutToResumeDownload(download::DownloadItem* download_item) override;

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

  // Callback when user permission prompt finishes. Args: whether file access
  // permission is acquired, which permission to update.
  using AcquirePermissionCallback =
      base::OnceCallback<void(bool, const std::string&)>;

 private:
  friend struct base::DefaultSingletonTraits<DownloadController>;
  DownloadController();
  ~DownloadController() override;

  // Helper method for implementing AcquireFileAccessPermission().
  bool HasFileAccessPermission();

  // DownloadControllerBase implementation.
  void OnDownloadStarted(download::DownloadItem* download_item) override;
  void StartContextMenuDownload(const content::ContextMenuParams& params,
                                content::WebContents* web_contents,
                                bool is_link,
                                const std::string& extra_headers) override;

  // DownloadItem::Observer interface.
  void OnDownloadUpdated(download::DownloadItem* item) override;

  // The download item contains dangerous file types.
  void OnDangerousDownload(download::DownloadItem* item);

  // Helper methods to start android download on UI thread.
  void StartAndroidDownload(const content::WebContents::Getter& wc_getter,
                            const DownloadInfo& info);
  void StartAndroidDownloadInternal(
      const content::WebContents::Getter& wc_getter,
      const DownloadInfo& info,
      bool allowed);

  // Check if an interrupted download item can be auto resumed.
  bool IsInterruptedDownloadAutoResumable(
      download::DownloadItem* download_item);

  std::string default_file_name_;

  using StrongValidatorsMap =
      std::map<std::string, std::pair<std::string, std::string>>;
  // Stores the previous strong validators before a download is resumed. If the
  // strong validators change after resumption starts, the download will restart
  // from the beginning and all downloaded data will be lost.
  StrongValidatorsMap strong_validators_map_;

  DISALLOW_COPY_AND_ASSIGN(DownloadController);
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_CONTROLLER_H_
