// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/box_uploader.h"

#include "base/files/file_util.h"
#include "base/i18n/time_formatting.h"
#include "base/i18n/unicodestring.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/file_system/account_info_utils.h"
#include "chrome/browser/enterprise/connectors/file_system/box_api_call_flow.h"
#include "chrome/browser/enterprise/connectors/file_system/box_api_call_response.h"
#include "chrome/browser/enterprise/connectors/file_system/box_upload_file_chunks_handler.h"
#include "chrome/browser/enterprise/connectors/file_system/rename_handler.h"
#include "chrome/browser/enterprise/connectors/file_system/uma_metrics_util.h"
#include "components/download/public/common/download_interrupt_reasons_utils.h"
#include "components/prefs/pref_service.h"
#include "net/http/http_status_code.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/i18n/unicode/calendar.h"

namespace {

constexpr auto kFileSuccess = base::File::Error::FILE_OK;
base::File::Error DeleteIfExists(base::FilePath path) {
  DCHECK(!path.empty());
  if (!base::PathExists(path)) {
    // If the file is deleted by some other thread, how can we be sure what we
    // read and uploaded was correct?! So report as error. Otherwise, it is
    // considered successful to
    // attempt to delete a file that does not exist by base::DeleteFile().
    return base::File::Error::FILE_ERROR_NOT_FOUND;
  }
  return base::DeleteFile(path) ? kFileSuccess : base::File::GetLastFileError();
}

using download::ConvertFileErrorToInterruptReason;
using download::ConvertNetErrorToInterruptReason;
using download::DownloadInterruptReasonToString;

const char kBoxErrorMessageFormat[] = "%d %s";         // 404 "not_found"
const char kBoxMoreMessageFormat[] = "Request ID %s";  // <request_id>

}  // namespace

namespace enterprise_connectors {

////////////////////////////////////////////////////////////////////////////////
// BoxUploader
////////////////////////////////////////////////////////////////////////////////

// static
const FileSystemServiceProvider BoxUploader::kServiceProvider = BOX;

// static
std::unique_ptr<BoxUploader> BoxUploader::Create(
    download::DownloadItem* download_item) {
  if (static_cast<size_t>(download_item->GetTotalBytes()) <
      BoxApiCallFlow::kChunkFileUploadMinSize) {
    return std::make_unique<BoxDirectUploader>(download_item);
  } else {
    return std::make_unique<BoxChunkedUploader>(download_item);
  }
}

// Box API reference:
// https://developer.box.com/guides/api-calls/permissions-and-errors/common-errors
BoxUploader::InterruptReason
BoxUploader::ConvertToInterruptReasonOrErrorMessage(BoxApiCallResponse response,
                                                    BoxInfo& reroute_info) {
  const auto code = response.net_or_http_code;
  DCHECK_NE(code, 0);
  if (code < 0) {  // net::Error's
    return ConvertNetErrorToInterruptReason(
        static_cast<net::Error>(code),
        download::DOWNLOAD_INTERRUPT_FROM_NETWORK);
  }
  // Otherwise it's HTTP errors, and we just display error message from Box.
  // Unless it's authentication errors, which are already handled in
  // EnsureSuccess().
  reroute_info.set_error_message(base::StringPrintf(
      kBoxErrorMessageFormat, code, response.box_error_code.c_str()));
  reroute_info.set_additional_message(base::StringPrintf(
      kBoxMoreMessageFormat, response.box_request_id.c_str()));
  return kServiceProviderUnknownError;
}

BoxUploader::BoxUploader(download::DownloadItem* download_item)
    : local_file_path_(
          // State would be COMPLETE iff it was already uploaded and had its
          // local file deleted successfully.
          (download_item->GetState() == download::DownloadItem::COMPLETE)
              ? base::FilePath()
              : download_item->GetFullPath()),
      target_file_name_(download_item->GetTargetFilePath().BaseName()),
      download_start_time_(download_item->GetStartTime()),
      uniquifier_(0) {
  bool is_completed =
      download_item->GetState() == download::DownloadItem::COMPLETE;
  const auto& reroute_info = download_item->GetRerouteInfo();

  if (reroute_info.IsInitialized()) {
    // If |reroute_info| is initialized, that means it was loaded from
    // databases, because the item was previously uploaded, or in the middle of
    // uploading. Therefore, as long as the state is consistent with file_id and
    // URL, |reroute_info| is valid.
    DCHECK_EQ(reroute_info.service_provider(), kServiceProvider);
    DCHECK_EQ(is_completed, reroute_info.box().has_file_id());
    reroute_info_ = reroute_info;
    DCHECK_EQ(is_completed, GetUploadedFileUrl().is_valid());
    DCHECK_EQ(reroute_info.box().has_folder_id(),
              GetDestinationFolderUrl().is_valid());
    // TODO(https://crbug.com/1213761) If |is_completed| == false, load info to
    // resume upload from where it left off.
  } else {
    // If |reroute_info| is not initialized, that means the item is either a new
    // download to be uploaded now, or the previous upload attempt didn't go
    // very far to have any information to recover from, therefore we start the
    // upload workflow from scratch.
    DCHECK(!is_completed) << download_item->GetState();
    reroute_info_.set_service_provider(kServiceProvider);
    reroute_info_.mutable_box();  // Set upload to BoxInfo.
  }
}

BoxUploader::~BoxUploader() {
  for (auto& observer : observers_)
    observer.OnDestruction();
  // TODO(https://crbug.com/1213761) May need to TerminateTask() to resume
  // later.
}

void BoxUploader::Init(
    base::RepeatingCallback<void(void)> authen_retry_callback,
    ProgressUpdateCallback progress_update_cb,
    UploadCompleteCallback upload_complete_cb,
    PrefService* prefs) {
  DCHECK(reroute_info().file_id().empty());
  prefs_ = prefs;
  authentication_retry_callback_ = std::move(authen_retry_callback);
  progress_update_cb_ = std::move(progress_update_cb);
  upload_complete_cb_ = std::move(upload_complete_cb);
  SendProgressUpdate();
  SetCurrentApiCall(GetFolderId().empty() ? MakeFindUpstreamFolderApiCall()
                                          : MakePreflightCheckApiCall());
}

void BoxUploader::TryTask(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& access_token) {
  url_loader_factory_ = std::move(url_loader_factory);
  access_token_ = access_token;
  TryCurrentApiCall();
}

void BoxUploader::TryCurrentApiCall() {
  DCHECK(authentication_retry_callback_);
  DCHECK(upload_complete_cb_);
  DCHECK(current_api_call_);
  StartCurrentApiCall();
}

void BoxUploader::TerminateTask(InterruptReason reason) {
  current_api_call_.reset(nullptr);
  // TODO(https://crbug.com/1213761) May need to resume upload later.
  OnApiCallFlowFailure(reason);
}

bool BoxUploader::EnsureSuccess(BoxApiCallResponse response) {
  if (response.success) {
    return true;
  }

  if (response.net_or_http_code == net::HTTP_UNAUTHORIZED) {
    // Authentication failure, so we need to redo authenticaction.
    authentication_retry_callback_.Run();
  } else {
    // Unexpected error. Clean up, then notify failure to download thread.
    OnApiCallFlowFailure(response);
  }
  return false;
}

void BoxUploader::StartCurrentApiCall() {
  current_api_call_->Start(url_loader_factory_, access_token_);
}

void BoxUploader::StartUpload() {
  LogUniquifierCountToUma();
  SendProgressUpdate();
  SetCurrentApiCall(MakeFileUploadApiCall());
  for (auto& observer : observers_)
    observer.OnUploadStart();
  TryCurrentApiCall();
}

void BoxUploader::OnFileError(base::File::Error error) {
  LOG(ERROR) << base::File::ErrorToString(error);
  const auto reason = ConvertFileErrorToInterruptReason(error);
  OnApiCallFlowFailure(reason);
}

void BoxUploader::OnApiCallFlowFailure(BoxApiCallResponse response) {
  const auto reason =
      ConvertToInterruptReasonOrErrorMessage(response, reroute_info());
  DLOG(ERROR) << "Request with id \"" << response.box_request_id
              << "\" failed with status " << response.net_or_http_code
              << " error \"" << response.box_error_code << "\" reason "
              << DownloadInterruptReasonToString(reason);
  OnApiCallFlowFailure(reason);
}

void BoxUploader::OnApiCallFlowFailure(InterruptReason reason) {
  OnApiCallFlowDone(reason, {});
}

void BoxUploader::OnApiCallFlowDone(InterruptReason reason,
                                    std::string file_id) {
  for (auto& observer : observers_)
    observer.OnUploadDone(reason == kSuccess);

  if (reason == kSuccess) {
    DCHECK(reroute_info().file_id().empty());
    DCHECK(!file_id.empty());
    reroute_info().set_file_id(file_id);
  } else {
    LOG(ERROR) << "Upload failed: " << DownloadInterruptReasonToString(reason);
  }

  // UMA Logging.
  switch (reason) {
    case kSuccess:
      UmaLogDownloadsRoutingStatus(
          EnterpriseFileSystemDownloadsRoutingStatus::ROUTING_SUCCEEDED);
      break;
    case kSignInCancellation:
      UmaLogDownloadsRoutingStatus(
          EnterpriseFileSystemDownloadsRoutingStatus::ROUTING_CANCELED);
      break;
    case kServiceProviderUnknownError:
    case download::DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE:
    case download::DOWNLOAD_INTERRUPT_REASON_SERVER_CONTENT_LENGTH_MISMATCH:
      UmaLogDownloadsRoutingStatus(EnterpriseFileSystemDownloadsRoutingStatus::
                                       ROUTING_FAILED_SERVICE_PROVIDER_ERROR);
      break;
    case kServiceProviderDown:
      UmaLogDownloadsRoutingStatus(EnterpriseFileSystemDownloadsRoutingStatus::
                                       ROUTING_FAILED_SERVICE_PROVIDER_OUTAGE);
      break;
    case kBrowserFailure:  // Same as `kCredentialUpdateFailure`
      UmaLogDownloadsRoutingStatus(EnterpriseFileSystemDownloadsRoutingStatus::
                                       ROUTING_FAILED_BROWSER_ERROR);
      break;
    // File errors converted by `ConvertFileErrorToInterruptReason`
    // Note that ConvertToInterruptReasonOrErrorMessage can also return some of
    // these codes for network errors caused by underlying file errors.
    case download::DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED:
    case download::DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR:
    case download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE:
    case download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED:
      UmaLogDownloadsRoutingStatus(EnterpriseFileSystemDownloadsRoutingStatus::
                                       ROUTING_FAILED_FILE_ERROR);
      break;
    // Network errors converted by `ConvertToInterruptReasonOrErrorMessage`
    case download::DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT:
    case download::DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED:
    case download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED:
      UmaLogDownloadsRoutingStatus(EnterpriseFileSystemDownloadsRoutingStatus::
                                       ROUTING_FAILED_NETWORK_ERROR);
      break;
    default:
      // We have covered all possible interrupt reasons above, if we encounter
      // any others the should be added to the appropriate fork.
      NOTREACHED() << "Unexpected error: " << reason;
  }

  SendProgressUpdate();

  // TODO(https://crbug.com/1165972): on upload failure, decide whether to
  // queue up the file to retry later, or also delete as usual. At this stage,
  // for trusted testers (TT), deleting as usual for now. Need to determine
  // how to communicate the failure/error to user.
  PostDeleteFileTask(reason);
}

void BoxUploader::NotifyResult(InterruptReason reason) {
  std::move(upload_complete_cb_).Run(reason, GetUploadFileName());
}

void BoxUploader::SendProgressUpdate() const {
  progress_update_cb_.Run(ProgressUpdate{GetUploadFileName(), reroute_info_});
}

void BoxUploader::OnFindUpstreamFolderResponse(BoxApiCallResponse response,
                                               const std::string& folder_id) {
  if (!EnsureSuccess(response)) {
    SetCurrentApiCall(MakeFindUpstreamFolderApiCall());
    return;
  }

  if (folder_id.empty()) {
    // Advance to create a new default download folder.
    SetCurrentApiCall(MakeCreateUpstreamFolderApiCall());
  } else {
    SetFolderId(folder_id);
    // Advance to preflight check.
    SetCurrentApiCall(MakePreflightCheckApiCall());
  }
  TryCurrentApiCall();
}

void BoxUploader::OnCreateUpstreamFolderResponse(BoxApiCallResponse response,
                                                 const std::string& folder_id) {
  if (!EnsureSuccess(response)) {
    SetCurrentApiCall(MakeCreateUpstreamFolderApiCall());
    return;
  }

  CHECK_EQ(folder_id.empty(), false);
  SetFolderId(folder_id);
  // Advance to preflight check.
  SetCurrentApiCall(MakePreflightCheckApiCall());
  TryCurrentApiCall();
}

void BoxUploader::LogUniquifierCountToUma() {
  base::UmaHistogramSparse(kUniquifierUmaLabel, uniquifier_);
}

void BoxUploader::OnPreflightCheckResponse(BoxApiCallResponse response) {
  if (response.success) {
    CHECK_EQ(response.net_or_http_code, net::HTTP_OK);
    StartUpload();
    return;
  }
  switch (response.net_or_http_code) {
    case net::HTTP_UNAUTHORIZED:
      // Authentication failure, we need to reauth and redo the preflight check.
      SetCurrentApiCall(MakePreflightCheckApiCall());
      authentication_retry_callback_.Run();
      break;
    case net::HTTP_NOT_FOUND:
      // Probably because folder id has changed or been deleted. Restart
      // from the beginning.
      SetFolderId(std::string());
      SetCurrentApiCall(MakeFindUpstreamFolderApiCall());
      TryCurrentApiCall();
      break;
    case net::HTTP_CONFLICT:
      if (uniquifier_ < UploadAttemptCount::kMaxRenamedWithSuffix) {
        ++uniquifier_;
      } else if (uniquifier_ == UploadAttemptCount::kMaxRenamedWithSuffix) {
        uniquifier_ = UploadAttemptCount::kTimestampBasedName;
      } else {
        uniquifier_ = UploadAttemptCount::kAbandonedUpload;
      }

      if (uniquifier_ < UploadAttemptCount::kAbandonedUpload) {
        SetCurrentApiCall(MakePreflightCheckApiCall());
        TryCurrentApiCall();
        break;
      }
      DLOG(WARNING) << "Box upload failed for file " << target_file_name_;
      LogUniquifierCountToUma();
      [[fallthrough]];  // Also OnOnApiCallFlowFailure() to surface this to
                        // user.
    default:
      // Unexpected error. Notify failure to download thread.
      OnApiCallFlowFailure(response);
  }
}

std::unique_ptr<OAuth2ApiCallFlow> BoxUploader::MakePreflightCheckApiCall() {
  return std::make_unique<BoxPreflightCheckApiCallFlow>(
      base::BindOnce(&BoxUploader::OnPreflightCheckResponse,
                     weak_factory_.GetWeakPtr()),
      GetUploadFileName(), GetFolderId());
}

std::unique_ptr<OAuth2ApiCallFlow>
BoxUploader::MakeFindUpstreamFolderApiCall() {
  return std::make_unique<BoxFindUpstreamFolderApiCallFlow>(base::BindOnce(
      &BoxUploader::OnFindUpstreamFolderResponse, weak_factory_.GetWeakPtr()));
}

std::unique_ptr<OAuth2ApiCallFlow>
BoxUploader::MakeCreateUpstreamFolderApiCall() {
  return std::make_unique<BoxCreateUpstreamFolderApiCallFlow>(
      base::BindOnce(&BoxUploader::OnCreateUpstreamFolderResponse,
                     weak_factory_.GetWeakPtr()));
}

// Getters & Setters ///////////////////////////////////////////////////////////

GURL BoxUploader::GetUploadedFileUrl() const {
  return BoxApiCallFlow::MakeUrlToShowFile(reroute_info().file_id());
}

GURL BoxUploader::GetDestinationFolderUrl() const {
  return BoxApiCallFlow::MakeUrlToShowFolder(GetFolderId());
}

const base::FilePath BoxUploader::GetLocalFilePath() const {
  return local_file_path_;
}

const base::FilePath BoxUploader::GetUploadFileName() const {
  if (uniquifier_ == UploadAttemptCount::kNotRenamed) {
    return target_file_name_;
  } else if (uniquifier_ <= UploadAttemptCount::kMaxRenamedWithSuffix) {
    return target_file_name_.InsertBeforeExtensionASCII(
        base::StringPrintf(" (%d)", uniquifier_));
  } else if (uniquifier_ == UploadAttemptCount::kTimestampBasedName) {
    // Generate an ISO8601 compliant local timestamp suffix that avoids
    // reserved characters that are forbidden on some OSes like Windows.
    base::Time::Exploded exploded;
    download_start_time_.LocalExplode(&exploded);

    // Instead of using the raw_offset, use the offset in effect now.
    // For instance, US Pacific Time, the offset shown will be -7 in summer
    // while it'll be -8 in winter. Time zone information appended to the format
    // generated by CreateUniqueFilename in
    // components/download/internal/common/download_path_reservation_tracker.cc
    int raw_offset, dst_offset;
    UDate now = icu::Calendar::getNow();
    std::unique_ptr<icu::TimeZone> zone(icu::TimeZone::createDefault());
    UErrorCode status = U_ZERO_ERROR;
    zone.get()->getOffset(now, false, raw_offset, dst_offset, status);
    DCHECK(U_SUCCESS(status));
    int offset = raw_offset + dst_offset;
    // |offset| is in msec.
    int minute_offset = offset / 60000;
    int hour_offset = minute_offset / 60;
    int min_remainder = std::abs(minute_offset) % 60;
    // Some timezones have a non-integral hour offset. So, we need to use hh:mm
    // form.
    std::string suffix = base::StringPrintf(
        " - %04d-%02d-%02dT%02d%02d%02d.%03d UTC%+dh%02d", exploded.year,
        exploded.month, exploded.day_of_month, exploded.hour, exploded.minute,
        exploded.second, exploded.millisecond, hour_offset, min_remainder);
    return target_file_name_.InsertBeforeExtensionASCII(suffix);
  } else {
    DCHECK_EQ(uniquifier_, UploadAttemptCount::kAbandonedUpload);
    return target_file_name_.InsertBeforeExtensionASCII(".abandoned");
  }
}

const std::string BoxUploader::GetFolderId() {
  if ((!reroute_info().has_folder_id() || reroute_info().folder_id().empty()) &&
      prefs_) {
    SetFolderId(
        GetDefaultFolderId(prefs_, kFileSystemServiceProviderPrefNameBox));
  }
  // TODO(https://crbug.com/1215847) Update to make API call to find folder id
  // if has file id.
  return reroute_info().folder_id();
}

const std::string BoxUploader::GetFolderId() const {
  return reroute_info().has_folder_id() ? reroute_info().folder_id()
                                        : std::string();
}

void BoxUploader::SetFolderId(std::string folder_id) {
  reroute_info().set_folder_id(folder_id);
  SetDefaultFolder(
      prefs_, kFileSystemServiceProviderPrefNameBox, folder_id,
      GetDefaultFolderName(prefs_, kFileSystemServiceProviderPrefNameBox));
  // TODO(https://crbug.com/1229831): use folder name obtained from api call.
}

void BoxUploader::SetCurrentApiCall(
    std::unique_ptr<OAuth2ApiCallFlow> api_call) {
  current_api_call_ = std::move(api_call);
}

// File Delete /////////////////////////////////////////////////////////////////

void BoxUploader::PostDeleteFileTask(InterruptReason upload_reason) {
  auto delete_file_task = base::BindOnce(&DeleteIfExists, GetLocalFilePath());
  auto delete_file_reply = base::BindOnce(
      &BoxUploader::OnFileDeleted, weak_factory_.GetWeakPtr(), upload_reason);
  for (auto& observer : observers_)
    observer.OnFileDeletionStart();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      std::move(delete_file_task), std::move(delete_file_reply));
}

void BoxUploader::OnFileDeleted(InterruptReason upload_reason,
                                base::File::Error delete_status) {
  auto final_reason = upload_reason;
  if (upload_reason == kSuccess &&
      delete_status != base::File::Error::FILE_OK) {
    final_reason = ConvertFileErrorToInterruptReason(delete_status);
    DLOG(ERROR) << "Failed to delete local temp file " << GetLocalFilePath()
                << "; possible reason: "
                << base::File::ErrorToString(delete_status);
  }
  NotifyResult(final_reason);
  for (auto& observer : observers_)
    observer.OnFileDeletionDone(delete_status == base::File::Error::FILE_OK);
}

// Helper methods for tests ////////////////////////////////////////////////////

std::string BoxUploader::GetFolderIdForTesting() const {
  return GetFolderId();
}

void BoxUploader::NotifyOAuth2ErrorForTesting() {
  authentication_retry_callback_.Run();
}

void BoxUploader::SetUploadApiCallFlowDoneForTesting(InterruptReason reason,
                                                     std::string file_id) {
  OnApiCallFlowDone(reason, file_id);
}

////////////////////////////////////////////////////////////////////////////////
// BoxDirectUploader
////////////////////////////////////////////////////////////////////////////////

// For possible MIME types:
// https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types/Common_types
BoxDirectUploader::BoxDirectUploader(download::DownloadItem* download_item)
    : BoxUploader(download_item), mime_type_(download_item->GetMimeType()) {}

BoxDirectUploader::~BoxDirectUploader() = default;

std::unique_ptr<OAuth2ApiCallFlow> BoxDirectUploader::MakeFileUploadApiCall() {
  return std::make_unique<BoxWholeFileUploadApiCallFlow>(
      base::BindOnce(&BoxDirectUploader::OnWholeFileUploadResponse,
                     weak_factory_.GetWeakPtr()),
      GetFolderId(), mime_type_, GetUploadFileName(), GetLocalFilePath());
}

void BoxDirectUploader::OnWholeFileUploadResponse(BoxApiCallResponse response,
                                                  const std::string& file_id) {
  if (!response.net_or_http_code) {
    OnFileError(base::File::Error::FILE_ERROR_IO);
    return;
  }
  if (!EnsureSuccess(response)) {
    SetCurrentApiCall(MakeFileUploadApiCall());
    return;
  }

  // Report upload success back to the download thread.
  OnApiCallFlowDone(kSuccess, file_id);
}

////////////////////////////////////////////////////////////////////////////////
// BoxChunkedUploader
////////////////////////////////////////////////////////////////////////////////

BoxChunkedUploader::BoxChunkedUploader(download::DownloadItem* download_item)
    : BoxUploader(download_item), file_size_(download_item->GetTotalBytes()) {}

BoxChunkedUploader::~BoxChunkedUploader() = default;

std::unique_ptr<OAuth2ApiCallFlow> BoxChunkedUploader::MakeFileUploadApiCall() {
  return MakeCreateUploadSessionApiCall();
}

std::unique_ptr<OAuth2ApiCallFlow>
BoxChunkedUploader::MakeCreateUploadSessionApiCall() {
  return std::make_unique<BoxCreateUploadSessionApiCallFlow>(
      base::BindOnce(&BoxChunkedUploader::OnCreateUploadSessionResponse,
                     weak_factory_.GetWeakPtr()),
      GetFolderId(), file_size_, GetUploadFileName());
}

std::unique_ptr<OAuth2ApiCallFlow>
BoxChunkedUploader::MakePartFileUploadApiCall() {
  return std::make_unique<BoxPartFileUploadApiCallFlow>(
      base::BindOnce(&BoxChunkedUploader::OnPartFileUploadResponse,
                     weak_factory_.GetWeakPtr()),
      session_endpoints_.FindPath("upload_part")->GetString(),
      curr_part_.content, curr_part_.byte_from, curr_part_.byte_to, file_size_);
}

std::unique_ptr<OAuth2ApiCallFlow>
BoxChunkedUploader::MakeCommitUploadSessionApiCall() {
  return std::make_unique<BoxCommitUploadSessionApiCallFlow>(
      base::BindOnce(&BoxChunkedUploader::OnCommitUploadSessionResponse,
                     weak_factory_.GetWeakPtr()),
      session_endpoints_.FindPath("commit")->GetString(), uploaded_parts_,
      sha1_digest_);
}

std::unique_ptr<OAuth2ApiCallFlow>
BoxChunkedUploader::MakeAbortUploadSessionApiCall(InterruptReason reason) {
  return std::make_unique<BoxAbortUploadSessionApiCallFlow>(
      base::BindOnce(&BoxChunkedUploader::OnAbortUploadSessionResponse,
                     weak_factory_.GetWeakPtr(), reason),
      session_endpoints_.FindPath("abort")->GetString());
}

void BoxChunkedUploader::OnCreateUploadSessionResponse(
    BoxApiCallResponse response,
    base::Value session_endpoints,
    size_t part_size) {
  if (!EnsureSuccess(response)) {
    if (response.net_or_http_code == net::HTTP_NOT_FOUND) {
      // Folder not found: clear locally stored folder id.
      LOG(ERROR) << "Folder id = " << GetFolderId() << " not found; clearing";
      // TODO(https://crbug.com/1190396): May be removed with Preflight Check?
      SetFolderId(std::string());
    }
    SetCurrentApiCall(MakeCreateUploadSessionApiCall());
    return;
  }

  session_endpoints_ = std::move(session_endpoints);
  chunks_handler_ = std::make_unique<FileChunksHandler>(GetLocalFilePath(),
                                                        file_size_, part_size);
  chunks_handler_->StartReading(
      base::BindRepeating(&BoxChunkedUploader::OnFileChunkRead,
                          weak_factory_.GetWeakPtr()),
      base::BindOnce(&BoxChunkedUploader::OnFileCompletelyUploaded,
                     weak_factory_.GetWeakPtr()));
}

void BoxChunkedUploader::OnFileChunkRead(PartInfo part_info) {
  if (part_info.content.empty()) {
    OnFileError(part_info.error);
    return;
  }
  // Advance to upload the file part.
  curr_part_ = std::move(part_info);
  SetCurrentApiCall(MakePartFileUploadApiCall());
  TryCurrentApiCall();
}

void BoxChunkedUploader::OnPartFileUploadResponse(BoxApiCallResponse response,
                                                  base::Value part_info) {
  if (!EnsureSuccess(response)) {
    if (response.net_or_http_code == net::HTTP_UNAUTHORIZED) {
      // Setup current_api_call_ to retry upload the file part.
      SetCurrentApiCall(MakePartFileUploadApiCall());
    }  // else don't overwrite, since OnApiCallFlowFailure() was triggered in
       // EnsureSuccess() and abortion is in-progress.
    return;
  }
  uploaded_parts_.Append(std::move(part_info));
  chunks_handler_->ContinueToReadChunk(
      uploaded_parts_.GetListDeprecated().size() + 1);
}

void BoxChunkedUploader::OnFileCompletelyUploaded(
    const std::string& sha1_digest) {
  DCHECK(sha1_digest.size());
  sha1_digest_ = sha1_digest;
  SetCurrentApiCall(MakeCommitUploadSessionApiCall());
  TryCurrentApiCall();
}

void BoxChunkedUploader::OnCommitUploadSessionResponse(
    BoxApiCallResponse response,
    base::TimeDelta retry_after,
    const std::string& file_id) {
  if (!EnsureSuccess(response)) {
    if (response.net_or_http_code == net::HTTP_UNAUTHORIZED) {
      SetCurrentApiCall(MakeCommitUploadSessionApiCall());
    }
    return;
  }

  if (response.net_or_http_code == net::HTTP_ACCEPTED) {
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&BoxChunkedUploader::OnFileCompletelyUploaded,
                       weak_factory_.GetWeakPtr(), sha1_digest_),
        retry_after);
  } else {
    OnApiCallFlowDone(kSuccess, file_id);
  }
}

void BoxChunkedUploader::OnAbortUploadSessionResponse(
    InterruptReason reason,
    BoxApiCallResponse response) {
  session_endpoints_.DictClear();  // Clear dict here to avoid infinite retry.
  if (EnsureSuccess(response)) {
    OnApiCallFlowFailure(reason);
  } else {
    // OnApiCallFlowFailure() already triggered in EnsureSuccess().
    LOG(ERROR) << "Unexpected response after aborting upload session for "
               << GetUploadFileName() << " after upload failure "
               << DownloadInterruptReasonToString(reason);
  }
}

void BoxChunkedUploader::OnApiCallFlowFailure(InterruptReason reason) {
  // Dict would've been cleared if aborted already; otherwise, try abort.
  if (session_endpoints_.is_dict() && !session_endpoints_.DictEmpty()) {
    chunks_handler_.reset();
    SetCurrentApiCall(MakeAbortUploadSessionApiCall(reason));
    TryCurrentApiCall();
  } else {
    BoxUploader::OnApiCallFlowFailure(reason);
  }
}

// BoxUploader::TestObserver
BoxUploader::TestObserver::TestObserver(FileSystemRenameHandler* rename_handler)
    : uploader_(
          FileSystemRenameHandler::TestObserver::GetBoxUploader(rename_handler)
              ->weak_factory_.GetWeakPtr()) {
  uploader_->observers_.AddObserver(this);
}

BoxUploader::TestObserver::~TestObserver() {
  uploader_->observers_.RemoveObserver(this);
}

void BoxUploader::TestObserver::OnUploadStart() {
  upload_status_ = Status::kInProgress;
  if (stop_waiting_for_upload_to_start_)
    std::move(stop_waiting_for_upload_to_start_).Run();
}

void BoxUploader::TestObserver::OnUploadDone(bool succeeded) {
  upload_status_ = succeeded ? Status::kSucceeded : Status::kFailed;
  if (stop_waiting_for_upload_to_complete_)
    std::move(stop_waiting_for_upload_to_complete_).Run();
}

void BoxUploader::TestObserver::OnFileDeletionStart() {
  tmp_file_deletion_status_ = Status::kInProgress;
}

void BoxUploader::TestObserver::OnFileDeletionDone(bool succeeded) {
  tmp_file_deletion_status_ = succeeded ? Status::kSucceeded : Status::kFailed;
  if (stop_waiting_for_deletion_to_complete_)
    std::move(stop_waiting_for_deletion_to_complete_).Run();
}

void BoxUploader::TestObserver::OnDestruction() {
  uploader_.reset();
}

void BoxUploader::TestObserver::WaitForUploadStart() {
  if (upload_status_ == Status::kInProgress)
    return;
  base::RunLoop run_loop;
  stop_waiting_for_upload_to_start_ = run_loop.QuitClosure();
  run_loop.Run();
}

bool BoxUploader::TestObserver::WaitForUploadCompletion() {
  if (upload_status_ == Status::kSucceeded || upload_status_ == Status::kFailed)
    return true;
  base::RunLoop run_loop;
  stop_waiting_for_upload_to_complete_ = run_loop.QuitClosure();
  run_loop.Run();
  return upload_status_ == Status::kSucceeded;
}

bool BoxUploader::TestObserver::WaitForTmpFileDeletion() {
  if ((tmp_file_deletion_status_ == Status::kSucceeded) ||
      (tmp_file_deletion_status_ == Status::kFailed))
    return true;
  base::RunLoop run_loop;
  stop_waiting_for_deletion_to_complete_ = run_loop.QuitClosure();
  run_loop.Run();
  return tmp_file_deletion_status_ == Status::kSucceeded;
}

GURL BoxUploader::TestObserver::GetFileUrl() {
  return uploader_->GetUploadedFileUrl();
}

}  // namespace enterprise_connectors
