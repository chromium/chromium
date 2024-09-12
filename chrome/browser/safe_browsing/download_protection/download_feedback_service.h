// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_FEEDBACK_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_FEEDBACK_SERVICE_H_

#include <memory>
#include <string>

#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
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

namespace safe_browsing {

class DownloadFeedback;

// Tracks active DownloadFeedback objects, provides interface for storing ping
// data for malicious downloads.
// Lives on the UI thread.
class DownloadFeedbackService {
 public:
  DownloadFeedbackService(
      DownloadProtectionService* download_protection_service,
      base::TaskRunner* file_task_runner);

  DownloadFeedbackService(const DownloadFeedbackService&) = delete;
  DownloadFeedbackService& operator=(const DownloadFeedbackService&) = delete;

  virtual ~DownloadFeedbackService();

  // Begin download feedback for the given |download| in the given
  // |profile|.  This must only be called if IsEnabledForDownload is
  // true for |download|.
  virtual void BeginFeedbackForDownload(Profile* profile,
                                        download::DownloadItem* download,
                                        const std::string& ping_request,
                                        const std::string& ping_response);

 private:
  static void BeginFeedbackOrDeleteFile(
      const scoped_refptr<base::TaskRunner>& file_task_runner,
      const base::WeakPtr<DownloadFeedbackService>& service,
      Profile* profile,
      const std::string& ping_request,
      const std::string& ping_response,
      uint64_t file_size,
      const base::FilePath& path);
  void StartPendingFeedback();
  void BeginFeedback(Profile* profile,
                     const std::string& ping_request,
                     const std::string& ping_response,
                     const base::FilePath& path,
                     uint64_t file_size);
  void FeedbackComplete();

  // Safe because the DownloadProtectionService owns this.
  raw_ptr<DownloadProtectionService> download_protection_service_;
  scoped_refptr<base::TaskRunner> file_task_runner_;

  // Currently active & pending uploads. The first item is active, remaining
  // items are pending.
  base::queue<std::unique_ptr<DownloadFeedback>> active_feedback_;

  base::WeakPtrFactory<DownloadFeedbackService> weak_ptr_factory_{this};
};
}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_FEEDBACK_SERVICE_H_
