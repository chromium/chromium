// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_feedback_service.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/supports_user_data.h"
#include "base/task_runner.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/safe_browsing/download_protection/download_feedback.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

namespace {

const void* const kPingKey = &kPingKey;

class DownloadFeedbackPings : public base::SupportsUserData::Data {
 public:
  DownloadFeedbackPings(const std::string& ping_request,
                        const std::string& ping_response);

  // Stores the ping data in the given |download|.
  static void CreateForDownload(download::DownloadItem* download,
                                const std::string& ping_request,
                                const std::string& ping_response);

  // Returns the DownloadFeedbackPings object associated with |download|.  May
  // return NULL.
  static DownloadFeedbackPings* FromDownload(
      const download::DownloadItem& download);

  const std::string& ping_request() const { return ping_request_; }

  const std::string& ping_response() const { return ping_response_; }

 private:
  std::string ping_request_;
  std::string ping_response_;
};

DownloadFeedbackPings::DownloadFeedbackPings(const std::string& ping_request,
                                             const std::string& ping_response)
    : ping_request_(ping_request), ping_response_(ping_response) {}

// static
void DownloadFeedbackPings::CreateForDownload(
    download::DownloadItem* download,
    const std::string& ping_request,
    const std::string& ping_response) {
  download->SetUserData(kPingKey, std::make_unique<DownloadFeedbackPings>(
                                      ping_request, ping_response));
}

// static
DownloadFeedbackPings* DownloadFeedbackPings::FromDownload(
    const download::DownloadItem& download) {
  return static_cast<DownloadFeedbackPings*>(download.GetUserData(kPingKey));
}

}  // namespace

DownloadFeedbackService::DownloadFeedbackService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::TaskRunner* file_task_runner)
    : url_loader_factory_(url_loader_factory),
      file_task_runner_(file_task_runner) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

DownloadFeedbackService::~DownloadFeedbackService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

// static
void DownloadFeedbackService::MaybeStorePingsForDownload(
    DownloadCheckResult result,
    bool upload_requested,
    download::DownloadItem* download,
    const std::string& ping,
    const std::string& response) {
  // We never upload SAFE or WHITELISTED_BY_POLICY files.
  if (result == DownloadCheckResult::SAFE ||
      result == DownloadCheckResult::WHITELISTED_BY_POLICY) {
    return;
  }

  if (!upload_requested)
    return;

  if (download->GetReceivedBytes() > DownloadFeedback::kMaxUploadSize)
    return;

  DownloadFeedbackPings::CreateForDownload(download, ping, response);
}

// static
bool DownloadFeedbackService::IsEnabledForDownload(
    const download::DownloadItem& download) {
  return !!DownloadFeedbackPings::FromDownload(download);
}

// static
bool DownloadFeedbackService::GetPingsForDownloadForTesting(
    const download::DownloadItem& download,
    std::string* ping,
    std::string* response) {
  DownloadFeedbackPings* pings = DownloadFeedbackPings::FromDownload(download);
  if (!pings)
    return false;

  *ping = pings->ping_request();
  *response = pings->ping_response();
  return true;
}

// static
void DownloadFeedbackService::RecordEligibleDownloadShown(
    download::DownloadDangerType danger_type) {
  UMA_HISTOGRAM_ENUMERATION("SBDownloadFeedback.Eligible", danger_type,
                            download::DOWNLOAD_DANGER_TYPE_MAX);
}

void DownloadFeedbackService::BeginFeedbackForDownload(
    download::DownloadItem* download,
    DownloadCommands::Command download_command) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DownloadFeedbackPings* pings = DownloadFeedbackPings::FromDownload(*download);
  DCHECK(pings);

  download->StealDangerousDownload(
      download_command == DownloadCommands::DISCARD,
      base::Bind(&DownloadFeedbackService::BeginFeedbackOrDeleteFile,
                 file_task_runner_, weak_ptr_factory_.GetWeakPtr(),
                 pings->ping_request(), pings->ping_response()));
  if (download_command == DownloadCommands::KEEP) {
    DownloadItemModel model(download);
    DownloadCommands(&model).ExecuteCommand(download_command);
  }
}

// static
void DownloadFeedbackService::BeginFeedbackOrDeleteFile(
    const scoped_refptr<base::TaskRunner>& file_task_runner,
    const base::WeakPtr<DownloadFeedbackService>& service,
    const std::string& ping_request,
    const std::string& ping_response,
    const base::FilePath& path) {
  if (service) {
    if (path.empty())
      return;
    service->BeginFeedback(ping_request, ping_response, path);
  } else {
    file_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&base::DeleteFile), path, false));
  }
}

void DownloadFeedbackService::StartPendingFeedback() {
  DCHECK(!active_feedback_.empty());
  active_feedback_.front()->Start(base::Bind(
      &DownloadFeedbackService::FeedbackComplete, base::Unretained(this)));
}

void DownloadFeedbackService::BeginFeedback(const std::string& ping_request,
                                            const std::string& ping_response,
                                            const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<DownloadFeedback> feedback(
      DownloadFeedback::Create(url_loader_factory_, file_task_runner_.get(),
                               path, ping_request, ping_response));
  active_feedback_.push(std::move(feedback));
  UMA_HISTOGRAM_COUNTS_100("SBDownloadFeedback.ActiveFeedbacks",
                           active_feedback_.size());

  if (active_feedback_.size() == 1)
    StartPendingFeedback();
}

void DownloadFeedbackService::FeedbackComplete() {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!active_feedback_.empty());
  active_feedback_.pop();
  if (!active_feedback_.empty())
    StartPendingFeedback();
}

}  // namespace safe_browsing
