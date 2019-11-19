// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_event_log_uploader.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/load_flags.h"
#include "net/base/mime_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "ui/base/text/bytes_formatting.h"

namespace webrtc_event_logging {

namespace {
// TODO(crbug.com/817495): Eliminate the duplication with other uploaders.
const char kUploadContentType[] = "multipart/form-data";
const char kBoundary[] = "----**--yradnuoBgoLtrapitluMklaTelgooG--**----";

constexpr size_t kExpectedMimeOverheadBytes = 1000;  // Intentional overshot.

// TODO(crbug.com/817495): Eliminate the duplication with other uploaders.
#if defined(OS_WIN)
const char kProduct[] = "Chrome";
#elif defined(OS_MACOSX)
const char kProduct[] = "Chrome_Mac";
#elif defined(OS_LINUX)
const char kProduct[] = "Chrome_Linux";
#elif defined(OS_ANDROID)
const char kProduct[] = "Chrome_Android";
#elif defined(OS_CHROMEOS)
const char kProduct[] = "Chrome_ChromeOS";
#else
#error Platform not supported.
#endif

constexpr net::NetworkTrafficAnnotationTag
    kWebrtcEventLogUploaderTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("webrtc_event_log_uploader", R"(
      semantics {
        sender: "WebRTC Event Log uploader module"
        description:
          "Uploads a WebRTC event log to a server called Crash. These logs "
          "will not contain private information. They will be used to "
          "improve WebRTC (fix bugs, tune performance, etc.)."
        trigger:
          "A Google service (e.g. Hangouts/Meet) has requested a peer "
          "connection to be logged, and the resulting event log to be uploaded "
          "at a time deemed to cause the least interference to the user (i.e., "
          "when the user is not busy making other VoIP calls)."
        data:
          "WebRTC events such as the timing of audio playout (but not the "
          "content), timing and size of RTP packets sent/received, etc."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting: "Feature controlled only through Chrome policy; "
                 "no user-facing control surface."
        chrome_policy {
          WebRtcEventLogCollectionAllowed {
            WebRtcEventLogCollectionAllowed: false
          }
        }
      })");

void AddFileContents(const char* filename,
                     const std::string& file_contents,
                     const std::string& content_type,
                     std::string* post_data) {
  // net::AddMultipartValueForUpload does almost what we want to do here, except
  // that it does not add the "filename" attribute. We hack it to force it to.
  std::string mime_value_name =
      base::StringPrintf("%s\"; filename=\"%s\"", filename, filename);
  net::AddMultipartValueForUpload(mime_value_name, file_contents, kBoundary,
                                  content_type, post_data);
}

std::string MimeContentType() {
  const char kBoundaryKeywordAndMisc[] = "; boundary=";

  std::string content_type;
  content_type.reserve(sizeof(content_type) + sizeof(kBoundaryKeywordAndMisc) +
                       sizeof(kBoundary));

  content_type.append(kUploadContentType);
  content_type.append(kBoundaryKeywordAndMisc);
  content_type.append(kBoundary);

  return content_type;
}

void BindURLLoaderFactoryReceiver(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>
        url_loader_factory_receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory =
      g_browser_process->shared_url_loader_factory();
  DCHECK(shared_url_loader_factory);
  shared_url_loader_factory->Clone(std::move(url_loader_factory_receiver));
}

void OnURLLoadUploadProgress(uint64_t current, uint64_t total) {
  ui::DataUnits unit = ui::GetByteDisplayUnits(total);
  VLOG(1) << "WebRTC event log upload progress: "
          << FormatBytesWithUnits(current, unit, false) << " / "
          << FormatBytesWithUnits(total, unit, true) << ".";
}
}  // namespace

const char WebRtcEventLogUploaderImpl::kUploadURL[] =
    "https://clients2.google.com/cr/report";

std::unique_ptr<WebRtcEventLogUploader>
WebRtcEventLogUploaderImpl::Factory::Create(const WebRtcLogFileInfo& log_file,
                                            UploadResultCallback callback) {
  return std::make_unique<WebRtcEventLogUploaderImpl>(
      log_file, std::move(callback), kMaxRemoteLogFileSizeBytes);
}

std::unique_ptr<WebRtcEventLogUploader>
WebRtcEventLogUploaderImpl::Factory::CreateWithCustomMaxSizeForTesting(
    const WebRtcLogFileInfo& log_file,
    UploadResultCallback callback,
    size_t max_log_file_size_bytes) {
  return std::make_unique<WebRtcEventLogUploaderImpl>(
      log_file, std::move(callback), max_log_file_size_bytes);
}

WebRtcEventLogUploaderImpl::WebRtcEventLogUploaderImpl(
    const WebRtcLogFileInfo& log_file,
    UploadResultCallback callback,
    size_t max_log_file_size_bytes)
    : log_file_(log_file),
      callback_(std::move(callback)),
      max_log_file_size_bytes_(max_log_file_size_bytes),
      io_task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  history_file_writer_ = WebRtcEventLogHistoryFileWriter::Create(
      GetWebRtcEventLogHistoryFilePath(log_file_.path));
  if (!history_file_writer_) {
    // File either could not be created, or, if a different error occurred,
    // Create() will have tried to remove the file it has created.
    UmaRecordWebRtcEventLoggingUpload(
        WebRtcEventLoggingUploadUma::kHistoryFileCreationError);
    ReportResult(false);
    return;
  }

  const base::Time now = std::max(base::Time::Now(), log_file.last_modified);
  if (!history_file_writer_->WriteCaptureTime(log_file.last_modified) ||
      !history_file_writer_->WriteUploadTime(now)) {
    LOG(ERROR) << "Writing to history file failed.";
    UmaRecordWebRtcEventLoggingUpload(
        WebRtcEventLoggingUploadUma::kHistoryFileWriteError);
    DeleteHistoryFile();  // Avoid partial, potentially-corrupt history files.
    ReportResult(false);
    return;
  }

  std::string upload_data;
  if (!PrepareUploadData(&upload_data)) {
    // History file will reflect a failed upload attempt.
    ReportResult(false);  // UMA recorded by PrepareUploadData().
    return;
  }

  StartUpload(upload_data);
}

WebRtcEventLogUploaderImpl::~WebRtcEventLogUploaderImpl() {
  // WebRtcEventLogUploaderImpl objects' deletion scenarios:
  // 1. Upload started and finished - |url_loader_| should have been reset
  //    so that we would be able to DCHECK and demonstrate that the determinant
  //    is maintained.
  // 2. Upload started and cancelled - behave similarly to a finished upload.
  // 3. The upload was never started, due to an early failure (e.g. file not
  //    found). In that case, |url_loader_| will not have been set.
  // 4. Chrome shutdown.
  if (io_task_runner_->RunsTasksInCurrentSequence()) {  // Scenarios 1-3.
    DCHECK(!url_loader_);
  } else {  // # Scenario #4 - Chrome shutdown.
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    bool will_delete =
        io_task_runner_->DeleteSoon(FROM_HERE, url_loader_.release());
    DCHECK(!will_delete)
        << "Task runners must have been stopped by this stage of shutdown.";
  }
}

const WebRtcLogFileInfo& WebRtcEventLogUploaderImpl::GetWebRtcLogFileInfo()
    const {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  return log_file_;
}

bool WebRtcEventLogUploaderImpl::Cancel() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  // The upload could already have been completed, or maybe was never properly
  // started (due to a file read failure, etc.).
  const bool upload_was_active = (url_loader_.get() != nullptr);

  // Note that in this case, it might still be that the last bytes hit the
  // wire right as we attempt to cancel the upload. OnURLFetchComplete, however,
  // will not be called.
  url_loader_.reset();

  DeleteLogFile();
  DeleteHistoryFile();

  if (upload_was_active) {
    UmaRecordWebRtcEventLoggingUpload(
        WebRtcEventLoggingUploadUma::kUploadCancelled);
  }

  return upload_was_active;
}

bool WebRtcEventLogUploaderImpl::PrepareUploadData(std::string* upload_data) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  std::string log_file_contents;
  if (!base::ReadFileToStringWithMaxSize(log_file_.path, &log_file_contents,
                                         max_log_file_size_bytes_)) {
    LOG(WARNING) << "Couldn't read event log file, or max file size exceeded.";
    UmaRecordWebRtcEventLoggingUpload(
        WebRtcEventLoggingUploadUma::kLogFileReadError);
    return false;
  }

  DCHECK(upload_data->empty());
  upload_data->reserve(log_file_contents.size() + kExpectedMimeOverheadBytes);

  const std::string filename_str = log_file_.path.BaseName().MaybeAsASCII();
  if (filename_str.empty()) {
    LOG(WARNING) << "Log filename is not according to acceptable format.";
    UmaRecordWebRtcEventLoggingUpload(
        WebRtcEventLoggingUploadUma::kLogFileNameError);
    return false;
  }

  const char* filename = filename_str.c_str();

  net::AddMultipartValueForUpload("prod", kProduct, kBoundary, std::string(),
                                  upload_data);
  net::AddMultipartValueForUpload("ver",
                                  version_info::GetVersionNumber() + "-webrtc",
                                  kBoundary, std::string(), upload_data);
  net::AddMultipartValueForUpload("guid", "0", kBoundary, std::string(),
                                  upload_data);
  net::AddMultipartValueForUpload("type", filename, kBoundary, std::string(),
                                  upload_data);
  AddFileContents(filename, log_file_contents, "application/log", upload_data);
  net::AddMultipartFinalDelimiterForUpload(kBoundary, upload_data);

  return true;
}

void WebRtcEventLogUploaderImpl::StartUpload(const std::string& upload_data) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kUploadURL);
  resource_request->method = "POST";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  // Create a new mojo pipe. It's safe to pass this around and use
  // immediately, even though it needs to finish initialization on the UI
  // thread.
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_remote;
  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(BindURLLoaderFactoryReceiver,
                     url_loader_factory_remote.BindNewPipeAndPassReceiver()));

  url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), kWebrtcEventLogUploaderTrafficAnnotation);
  url_loader_->AttachStringForUpload(upload_data, MimeContentType());
  url_loader_->SetOnUploadProgressCallback(
      base::BindRepeating(OnURLLoadUploadProgress));

  // See comment in destructor for an explanation about why using
  // base::Unretained(this) is safe here.
  url_loader_->DownloadToString(
      url_loader_factory_remote.get(),
      base::BindOnce(&WebRtcEventLogUploaderImpl::OnURLLoadComplete,
                     base::Unretained(this)),
      kWebRtcEventLogMaxUploadIdBytes);
}

void WebRtcEventLogUploaderImpl::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(url_loader_);

  if (response_body.get() != nullptr && response_body->empty()) {
    LOG(WARNING) << "SimpleURLLoader reported upload successful, "
                 << "but report ID unknown.";
  }

  const bool upload_successful =
      (response_body.get() != nullptr && !response_body->empty());

  // NetError() is 0 when no error occurred.
  UmaRecordWebRtcEventLoggingNetErrorType(url_loader_->NetError());

  DCHECK(history_file_writer_);
  if (upload_successful) {
    if (!history_file_writer_->WriteUploadId(*response_body)) {
      // Discard the incomplete, potentially now corrupt history file, but the
      // upload is still considered successful.
      LOG(ERROR) << "Failed to write upload ID to history file.";
      DeleteHistoryFile();
    }
  } else {
    LOG(WARNING) << "Upload unsuccessful.";
    // By not writing an UploadId to the history file, it is inferrable that
    // the upload was initiated, but did not end successfully.
  }

  UmaRecordWebRtcEventLoggingUpload(
      upload_successful ? WebRtcEventLoggingUploadUma::kSuccess
                        : WebRtcEventLoggingUploadUma::kUploadFailure);

  url_loader_.reset();  // Explicitly maintain determinant.

  ReportResult(upload_successful);
}

void WebRtcEventLogUploaderImpl::ReportResult(bool result) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  // * If the upload was successful, the file is no longer needed.
  // * If the upload failed, we don't want to retry, because we run the risk of
  //   uploading significant amounts of data once again, only for the upload to
  //   fail again after (as an example) wasting 50MBs of upload bandwidth.
  // * If the file was not found, this will simply have no effect (other than
  //   to LOG() an error).
  // TODO(crbug.com/775415): Provide refined retrial behavior.
  DeleteLogFile();

  // Release hold of history file, allowing it to be read, moved or deleted.
  history_file_writer_.reset();

  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), log_file_.path, result));
}

void WebRtcEventLogUploaderImpl::DeleteLogFile() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  const bool deletion_successful =
      base::DeleteFile(log_file_.path, /*recursive=*/false);
  if (!deletion_successful) {
    // This is a somewhat serious (though unlikely) error, because now we'll
    // try to upload this file again next time Chrome launches.
    LOG(ERROR) << "Could not delete pending WebRTC event log file.";
  }
}

void WebRtcEventLogUploaderImpl::DeleteHistoryFile() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (!history_file_writer_) {
    LOG(ERROR) << "Deletion of history file attempted after uploader "
               << "has relinquished ownership of it.";
    return;
  }
  history_file_writer_->Delete();
  history_file_writer_.reset();
}

}  // namespace webrtc_event_logging
