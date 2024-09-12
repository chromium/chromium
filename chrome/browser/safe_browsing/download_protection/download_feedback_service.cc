// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_feedback_service.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/supports_user_data.h"
#include "base/task/task_runner.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/safe_browsing/download_protection/download_feedback.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

DownloadFeedbackService::DownloadFeedbackService(
    DownloadProtectionService* download_protection_service,
    base::TaskRunner* file_task_runner)
    : download_protection_service_(download_protection_service),
      file_task_runner_(file_task_runner) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

DownloadFeedbackService::~DownloadFeedbackService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void DownloadFeedbackService::BeginFeedbackForDownload(
    Profile* profile,
    download::DownloadItem* download,
    const std::string& ping_request,
    const std::string& ping_response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  download->CopyDownload(base::BindOnce(
      &DownloadFeedbackService::BeginFeedbackOrDeleteFile, file_task_runner_,
      weak_ptr_factory_.GetWeakPtr(), profile, ping_request, ping_response,
      base::checked_cast<uint64_t>(download->GetReceivedBytes())));
}

// static
void DownloadFeedbackService::BeginFeedbackOrDeleteFile(
    const scoped_refptr<base::TaskRunner>& file_task_runner,
    const base::WeakPtr<DownloadFeedbackService>& service,
    Profile* profile,
    const std::string& ping_request,
    const std::string& ping_response,
    uint64_t file_size,
    const base::FilePath& path) {
  if (service) {
    if (path.empty()) {
      return;
    }
    service->BeginFeedback(profile, ping_request, ping_response, path,
                           file_size);
  } else {
    file_task_runner->PostTask(FROM_HERE, base::GetDeleteFileCallback(path));
  }
}

void DownloadFeedbackService::StartPendingFeedback() {
  DCHECK(!active_feedback_.empty());
  active_feedback_.front()->Start(base::BindOnce(
      &DownloadFeedbackService::FeedbackComplete, base::Unretained(this)));
}

void DownloadFeedbackService::BeginFeedback(Profile* profile,
                                            const std::string& ping_request,
                                            const std::string& ping_response,
                                            const base::FilePath& path,
                                            uint64_t file_size) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<DownloadFeedback> feedback(DownloadFeedback::Create(
      download_protection_service_->GetURLLoaderFactory(profile), path,
      file_size, ping_request, ping_response));
  active_feedback_.push(std::move(feedback));

  if (active_feedback_.size() == 1) {
    StartPendingFeedback();
  }
}

void DownloadFeedbackService::FeedbackComplete() {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!active_feedback_.empty());
  active_feedback_.pop();
  if (!active_feedback_.empty()) {
    StartPendingFeedback();
  }
}

}  // namespace safe_browsing
