// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_feedback.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task_runner.h"
#include "chrome/browser/safe_browsing/download_protection/two_phase_uploader.h"
#include "components/safe_browsing/proto/csd.pb.h"
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
      base::TaskRunner* file_task_runner,
      const base::FilePath& file_path,
      const std::string& ping_request,
      const std::string& ping_response);
  ~DownloadFeedbackImpl() override;

  void Start(const base::Closure& finish_callback) override;

  const std::string& GetPingRequestForTesting() const override {
    return ping_request_;
  }

  const std::string& GetPingResponseForTesting() const override {
    return ping_response_;
  }

 private:
  // Callback for TwoPhaseUploader completion.  Relays the result to the
  // |finish_callback|.
  void FinishedUpload(base::Closure finish_callback,
                      TwoPhaseUploader::State state,
                      int net_error,
                      int response_code,
                      const std::string& response);

  void RecordUploadResult(UploadResultType result);
  void RecordErrorCodes(TwoPhaseUploader::State state,
                        int net_error,
                        int response_code);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  scoped_refptr<base::TaskRunner> file_task_runner_;
  const base::FilePath file_path_;
  int64_t file_size_;

  // The safebrowsing request and response of checking that this binary is
  // unsafe.
  std::string ping_request_;
  std::string ping_response_;

  std::unique_ptr<TwoPhaseUploader> uploader_;

  // The time at which we started uploading. Used for metrics.
  base::Time uploader_start_time_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFeedbackImpl);
};

DownloadFeedbackImpl::DownloadFeedbackImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::TaskRunner* file_task_runner,
    const base::FilePath& file_path,
    const std::string& ping_request,
    const std::string& ping_response)
    : url_loader_factory_(url_loader_factory),
      file_task_runner_(file_task_runner),
      file_path_(file_path),
      file_size_(-1),
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
    RecordUploadResult(UPLOAD_CANCELLED);
    // Destroy the uploader before attempting to delete the file.
    uploader_.reset();
  }

  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&base::DeleteFile), file_path_, false));
}

void DownloadFeedbackImpl::Start(const base::Closure& finish_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!uploader_);

  ClientDownloadReport report_metadata;

  bool r = report_metadata.mutable_download_request()->ParseFromString(
      ping_request_);
  DCHECK(r);
  r = report_metadata.mutable_download_response()->ParseFromString(
      ping_response_);
  DCHECK(r);
  file_size_ = report_metadata.download_request().length();

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
            SafeBrowsingExtendedReportingOptInAllowed {
              policy_options {mode: MANDATORY}
              SafeBrowsingExtendedReportingOptInAllowed: false
            }
          }
        })");

  uploader_ = TwoPhaseUploader::Create(
      url_loader_factory_, file_task_runner_.get(), GURL(kSbFeedbackURL),
      metadata_string, file_path_,
      base::Bind(&DownloadFeedbackImpl::FinishedUpload, base::Unretained(this),
                 finish_callback),
      traffic_annotation);
  uploader_->Start();
  uploader_start_time_ = base::Time::Now();
}

void DownloadFeedbackImpl::FinishedUpload(base::Closure finish_callback,
                                          TwoPhaseUploader::State state,
                                          int net_error,
                                          int response_code,
                                          const std::string& response_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << __func__ << " " << state << " rlen=" << response_data.size();

  switch (state) {
    case TwoPhaseUploader::STATE_SUCCESS: {
      ClientUploadResponse response;
      if (!response.ParseFromString(response_data) ||
          response.status() != ClientUploadResponse::SUCCESS) {
        RecordUploadResult(UPLOAD_COMPLETE_RESPONSE_ERROR);
      } else {
        RecordUploadResult(UPLOAD_SUCCESS);
      }
      break;
    }
    case TwoPhaseUploader::UPLOAD_FILE:
      RecordUploadResult(net_error != net::OK ? UPLOAD_FILE_NET_ERROR
                                              : UPLOAD_FILE_RESPONSE_ERROR);
      break;
    case TwoPhaseUploader::UPLOAD_METADATA:
      RecordUploadResult(net_error != net::OK ? UPLOAD_METADATA_NET_ERROR
                                              : UPLOAD_METADATA_RESPONSE_ERROR);
      break;
    case TwoPhaseUploader::STATE_NONE:
      NOTREACHED();
      break;
  }

  RecordErrorCodes(state, net_error, response_code);
  UMA_HISTOGRAM_LONG_TIMES("SBDownloadFeedback.UploadDuration",
                           base::Time::Now() - uploader_start_time_);

  uploader_.reset();

  finish_callback.Run();
  // We may be deleted here.
}

void DownloadFeedbackImpl::RecordUploadResult(UploadResultType result) {
  if (result == UPLOAD_SUCCESS) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("SBDownloadFeedback.SizeSuccess", file_size_, 1,
                                kMaxUploadSize, 50);
  } else {
    UMA_HISTOGRAM_CUSTOM_COUNTS("SBDownloadFeedback.SizeFailure", file_size_, 1,
                                kMaxUploadSize, 50);
  }
  UMA_HISTOGRAM_ENUMERATION("SBDownloadFeedback.UploadResult", result,
                            UPLOAD_RESULT_MAX);
}

void DownloadFeedbackImpl::RecordErrorCodes(TwoPhaseUploader::State state,
                                            int net_error,
                                            int response_code) {
  if (state == TwoPhaseUploader::UPLOAD_FILE) {
    base::UmaHistogramSparse("SBDownloadFeedback.FileErrors",
                             net_error == net::OK ? response_code : net_error);
  } else if (state == TwoPhaseUploader::UPLOAD_METADATA) {
    base::UmaHistogramSparse("SBDownloadFeedback.MetadataErrors",
                             net_error == net::OK ? response_code : net_error);
  }
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
    base::TaskRunner* file_task_runner,
    const base::FilePath& file_path,
    const std::string& ping_request,
    const std::string& ping_response) {
  if (!factory_) {
    return base::WrapUnique(
        new DownloadFeedbackImpl(url_loader_factory, file_task_runner,
                                 file_path, ping_request, ping_response));
  }
  return DownloadFeedback::factory_->CreateDownloadFeedback(
      url_loader_factory, file_task_runner, file_path, ping_request,
      ping_response);
}

}  // namespace safe_browsing
