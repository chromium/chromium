// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_FEEDBACK_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_FEEDBACK_SERVICE_H_

#include <memory>
#include <string>

#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "components/download/public/common/download_danger_type.h"

namespace base {
class TaskRunner;
}

namespace download {
class DownloadItem;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace safe_browsing {

class DownloadFeedback;

// Tracks active DownloadFeedback objects, provides interface for storing ping
// data for malicious downloads.
// Lives on the UI thread.
class DownloadFeedbackService {
 public:
  DownloadFeedbackService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::TaskRunner* file_task_runner);
  ~DownloadFeedbackService();

  // Stores the request and response ping data from the download check, if the
  // check result and file size are eligible. This must be called after a
  // download has been flagged as un-SAFE in order for the download to be
  // enabled for uploading. Some un-SAFE downloads can be marked for
  // upload by the server with |upload_requested| if it's needed for better
  // classification.
  static void MaybeStorePingsForDownload(DownloadCheckResult result,
                                         bool upload_requested,
                                         download::DownloadItem* download,
                                         const std::string& ping,
                                         const std::string& response);

  // Test if pings have been stored for |download|.
  static bool IsEnabledForDownload(const download::DownloadItem& download);

  // Get the ping values stored in |download|. Returns false if no ping values
  // are present.
  static bool GetPingsForDownloadForTesting(
      const download::DownloadItem& download,
      std::string* ping,
      std::string* response);

  // Records histogram for download feedback option shown to user.
  static void RecordEligibleDownloadShown(
      download::DownloadDangerType danger_type);

  // Begin download feedback for |download|. Then delete download file if
  // |download_command| is DISCARD, or run the KEEP command otherwise.This must
  // only be called if IsEnabledForDownload is true for |download|.
  void BeginFeedbackForDownload(download::DownloadItem* download,
                                DownloadCommands::Command download_command);

 private:
  static void BeginFeedbackOrDeleteFile(
      const scoped_refptr<base::TaskRunner>& file_task_runner,
      const base::WeakPtr<DownloadFeedbackService>& service,
      const std::string& ping_request,
      const std::string& ping_response,
      const base::FilePath& path);
  void StartPendingFeedback();
  void BeginFeedback(const std::string& ping_request,
                     const std::string& ping_response,
                     const base::FilePath& path);
  void FeedbackComplete();

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  scoped_refptr<base::TaskRunner> file_task_runner_;

  // Currently active & pending uploads. The first item is active, remaining
  // items are pending.
  base::queue<std::unique_ptr<DownloadFeedback>> active_feedback_;

  base::WeakPtrFactory<DownloadFeedbackService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DownloadFeedbackService);
};
}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_FEEDBACK_SERVICE_H_
