// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_feedback.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/multipart_uploader.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

namespace {

// This enum is used by histograms.  Do not change the ordering or remove items.
enum UploadResultType {
  UPLOAD_SUCCESS = 0,
  UPLOAD_CANCELLED = 1,
  UPLOAD_METADATA_NET_ERROR = 2,
  UPLOAD_METADATA_RESPONSE_ERROR = 3,
  UPLOAD_FILE_NET_ERROR = 4,
  UPLOAD_FILE_RESPONSE_ERROR = 5,
  UPLOAD_COMPLETE_RESPONSE_ERROR = 6,
  // Memory space for histograms is determined by the max.
  // ALWAYS ADD NEW VALUES BEFORE THIS ONE.
  UPLOAD_RESULT_MAX = 7
};

// Handles the uploading of a single downloaded binary to the safebrowsing
// download feedback service.
class DownloadFeedbackImpl : public DownloadFeedback {
 public:
  DownloadFeedbackImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const base::FilePath& file_path,
      uint64_t file_size,
      const std::string& ping_request,
      const std::string& ping_response);

  DownloadFeedbackImpl(const DownloadFeedbackImpl&) = delete;
  DownloadFeedbackImpl& operator=(const DownloadFeedbackImpl&) = delete;

  ~DownloadFeedbackImpl() override;

  void Start(base::OnceClosure finish_callback) override;

  const std::string& GetPingRequestForTesting() const override {
    return ping_request_;
  }

  const std::string& GetPingResponseForTesting() const override {
    return ping_response_;
  }

 private:
  // Callback for TwoPhaseUploader completion.  Relays the result to the
  // |finish_callback|.
  void FinishedUpload(base::OnceClosure finish_callback,
                      bool success,
                      int response_code,
                      const std::string& response);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const base::FilePath file_path_;
  uint64_t file_size_;

  // The safebrowsing request and response of checking that this binary is
  // unsafe.
  std::string ping_request_;
  std::string ping_response_;

  std::unique_ptr<ConnectorUploadRequest> uploader_;
};

DownloadFeedbackImpl::DownloadFeedbackImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const base::FilePath& file_path,
    uint64_t file_size,
    const std::string& ping_request,
    const std::string& ping_response)
    : url_loader_factory_(url_loader_factory),
      file_path_(file_path),
      file_size_(file_size),
      ping_request_(ping_request),
      ping_response_(ping_response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << "DownloadFeedback constructed " << this << " for "
           << file_path.AsUTF8Unsafe();
}

DownloadFeedbackImpl::~DownloadFeedbackImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << "DownloadFeedback destructed " << this;

  if (uploader_) {
    // Destroy the uploader before attempting to delete the file.
    uploader_.reset();
  }

  base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE, base::GetDeleteFileCallback(file_path_));
}

void DownloadFeedbackImpl::Start(base::OnceClosure finish_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!uploader_);

  if (!url_loader_factory_) {
    std::move(finish_callback).Run();
    return;
  }

  ClientDownloadReport report_metadata;

  bool r = report_metadata.mutable_download_request()->ParseFromString(
      ping_request_);
  DCHECK(r);
  r = report_metadata.mutable_download_response()->ParseFromString(
      ping_response_);
  DCHECK(r);

  std::string metadata_string;
  bool ok = report_metadata.SerializeToString(&metadata_string);
  DCHECK(ok);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("safe_browsing_feedback", R"(
        semantics {
          sender: "Safe Browsing Download Protection Feedback"
          description:
            "When a user downloads a binary that Safe Browsing declares as "
            "suspicious, opted-in clients may upload that binary to Safe "
            "Browsing to improve the classification. This helps protect users "
            "from malware and unwanted software."
          trigger:
            "The browser will upload the binary to Google when a "
            "download-protection verdict is 'Not Safe', and the user is opted "
            "in to extended reporting."
          data:
            "The suspicious binary file."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: YES
          cookies_store: "Safe Browsing Cookie Store"
          setting:
            "Users can enable or disable this feature by stopping sending "
            "security incident reports to Google via disabling 'Automatically "
            "report details of possible security incidents to Google.' in "
            "Chrome's settings under Advanced Settings, Privacy. The feature "
            "is disabled by default."
          chrome_policy {
            SafeBrowsingExtendedReportingEnabled {
              policy_options {mode: MANDATORY}
              SafeBrowsingExtendedReportingEnabled: false
            }
          }
        })");

  uploader_ = MultipartUploadRequest::CreateFileRequest(
      url_loader_factory_, GURL(kSbFeedbackURL), metadata_string, file_path_,
      file_size_, traffic_annotation,
      base::BindOnce(&DownloadFeedbackImpl::FinishedUpload,
                     base::Unretained(this), std::move(finish_callback)));
  uploader_->Start();
}

void DownloadFeedbackImpl::FinishedUpload(base::OnceClosure finish_callback,
                                          bool success,
                                          int response_code,
                                          const std::string& response_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  uploader_.reset();

  std::move(finish_callback).Run();
  // We may be deleted here.
}

}  // namespace

// static
const int64_t DownloadFeedback::kMaxUploadSize = 50 * 1024 * 1024;

// static
const char DownloadFeedback::kSbFeedbackURL[] =
    "https://safebrowsing.google.com/safebrowsing/uploads/chrome";

// static
DownloadFeedbackFactory* DownloadFeedback::factory_ = nullptr;

// static
std::unique_ptr<DownloadFeedback> DownloadFeedback::Create(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const base::FilePath& file_path,
    uint64_t file_size,
    const std::string& ping_request,
    const std::string& ping_response) {
  if (!factory_) {
    return base::WrapUnique(new DownloadFeedbackImpl(
        url_loader_factory, file_path, file_size, ping_request, ping_response));
  }
  return DownloadFeedback::factory_->CreateDownloadFeedback(
      url_loader_factory, file_path, file_size, ping_request, ping_response);
}

}  // namespace safe_browsing
