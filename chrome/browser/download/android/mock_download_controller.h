// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_MOCK_DOWNLOAD_CONTROLLER_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_MOCK_DOWNLOAD_CONTROLLER_H_

#include "base/functional/callback.h"
#include "chrome/browser/download/android/download_controller_base.h"

namespace android {

// Mock implementation of the DownloadController.
class MockDownloadController : public DownloadControllerBase {
 public:
  MockDownloadController();

  MockDownloadController(const MockDownloadController&) = delete;
  MockDownloadController& operator=(const MockDownloadController&) = delete;

  ~MockDownloadController() override;

  // DownloadControllerBase implementation.
  void OnDownloadStarted(download::DownloadItem* download_item) override;
  void StartContextMenuDownload(const content::ContextMenuParams& params,
                                content::WebContents* web_contents,
                                bool is_link) override;
  void AcquireFileAccessPermission(
      const content::WebContents::Getter& wc_getter,
      AcquireFileAccessPermissionCallback callback) override;
  void SetApproveFileAccessRequestForTesting(bool approve) override;
  void CreateAndroidDownload(const content::WebContents::Getter& wc_getter,
                             const DownloadInfo& info) override;

 private:
  bool approve_file_access_request_;
};

}  // namespace android

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_MOCK_DOWNLOAD_CONTROLLER_H_
