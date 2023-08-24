// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"

#include <algorithm>
#include <memory>
#include <numeric>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/analysis/analysis_settings.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_dialog.h"
#include "chrome/browser/enterprise/connectors/analysis/files_request_handler.h"
#include "chrome/browser/enterprise/connectors/analysis/page_print_analysis_request.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/file_util_service.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/file_analysis_request.h"
#include "chrome/browser/safe_browsing/download_protection/check_client_download_request.h"
#include "chrome/grit/generated_resources.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/policy/core/common/chrome_schema.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/url_matcher/url_matcher.h"
#include "content/public/browser/web_contents.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "net/base/mime_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_sdk_manager.h"  // nogncheck
#endif

using safe_browsing::BinaryUploadService;

namespace enterprise_connectors {

namespace {

// Global pointer of factory function (RepeatingCallback) used to create
// instances of ContentAnalysisDelegate in tests.  !is_null() only in tests.
ContentAnalysisDelegate::Factory* GetFactoryStorage() {
  static base::NoDestructor<ContentAnalysisDelegate::Factory> factory;
  return factory.get();
}

bool* UIEnabledStorage() {
  static bool enabled = true;
  return &enabled;
}

ContentAnalysisDelegate::OnAckAllRequestsCallback* OnAckAllRequestsStorage() {
  static base::NoDestructor<ContentAnalysisDelegate::OnAckAllRequestsCallback>
      callback;
  return callback.get();
}

}  // namespace

StringAnalysisRequest::StringAnalysisRequest(
    CloudOrLocalAnalysisSettings settings,
    std::string text,
    BinaryUploadService::ContentAnalysisCallback callback)
    : Request(std::move(callback), std::move(settings)) {
  DCHECK_GT(text.size(), 0u);
  data_.size = text.size();

  // Only remember strings less than the maximum allowed.
  if (text.size() < BinaryUploadService::kMaxUploadSizeBytes) {
    data_.contents = std::move(text);
    result_ = BinaryUploadService::Result::SUCCESS;
  }
  safe_browsing::IncrementCrashKey(
      safe_browsing::ScanningCrashKey::PENDING_TEXT_UPLOADS);
  safe_browsing::IncrementCrashKey(
      safe_browsing::ScanningCrashKey::TOTAL_TEXT_UPLOADS);
}

StringAnalysisRequest::~StringAnalysisRequest() {
  safe_browsing::DecrementCrashKey(
      safe_browsing::ScanningCrashKey::PENDING_TEXT_UPLOADS);
}

void StringAnalysisRequest::GetRequestData(DataCallback callback) {
  std::move(callback).Run(result_, data_);
}

ContentAnalysisDelegate::Data::Data() = default;
ContentAnalysisDelegate::Data::Data(Data&& other) = default;
ContentAnalysisDelegate::Data& ContentAnalysisDelegate::Data::operator=(
    ContentAnalysisDelegate::Data&& other) = default;
ContentAnalysisDelegate::Data::~Data() = default;

ContentAnalysisDelegate::Result::Result() = default;
ContentAnalysisDelegate::Result::Result(Result&& other) = default;
ContentAnalysisDelegate::Result::~Result() = default;

ContentAnalysisDelegate::~ContentAnalysisDelegate() = default;

void ContentAnalysisDelegate::BypassWarnings(
    absl::optional<std::u16string> user_justification) {
  if (callback_.is_null())
    return;

  // Mark the full text as complying and report a warning bypass.
  if (text_warning_) {
    std::fill(result_.text_results.begin(), result_.text_results.end(), true);

    int64_t content_size = 0;
    for (const std::string& entry : data_.text)
      content_size += entry.size();

    ReportAnalysisConnectorWarningBypass(
        profile_, url_, "", "", "Text data", std::string(), "text/plain",
        extensions::SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload,
        access_point_, content_size, text_response_, user_justification);
  }

  // Mark the full image as complying and report a warning bypass.
  if (image_warning_) {
    result_.image_result = true;

    ReportAnalysisConnectorWarningBypass(
        profile_, url_, "", "", "Image data", std::string(),
        /*mime_type*/ std::string(),
        extensions::SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload,
        access_point_, data_.image.size(), image_response_, user_justification);
  }

  if (!warned_file_indices_.empty()) {
    files_request_handler_->ReportWarningBypass(user_justification);
    // Mark every warned file as complying.
    for (size_t index : warned_file_indices_) {
      result_.paths_results[index] = true;
    }
  }

  // Mark the printed page as complying and report a warning bypass.
  if (page_warning_) {
    result_.page_result = true;

    ReportAnalysisConnectorWarningBypass(
        profile_, url_, "", /*destination*/ data_.printer_name, title_,
        /*sha256*/ std::string(),
        /*mime_type*/ std::string(),
        extensions::SafeBrowsingPrivateEventRouter::kTriggerPagePrint,
        access_point_, /*content_size*/ -1, page_response_, user_justification);
  }

  RunCallback();
}

void ContentAnalysisDelegate::Cancel(bool warning) {
  if (callback_.is_null())
    return;

  // Don't report this upload as cancelled if the user didn't bypass the
  // warning.
  if (!warning) {
    RecordDeepScanMetrics(
        data_.settings.cloud_or_local_settings.is_cloud_analysis(),
        access_point_, base::TimeTicks::Now() - upload_start_time_, 0,
        "CancelledByUser", false);
  }

  // Ask the binary upload service to cancel requests if it can.
  auto cancel = std::make_unique<BinaryUploadService::CancelRequests>(
      data_.settings.cloud_or_local_settings);
  cancel->set_user_action_id(user_action_id_);

  BinaryUploadService* upload_service = GetBinaryUploadService();
  if (upload_service)
    upload_service->MaybeCancelRequests(std::move(cancel));

  // Make sure to reject everything.
  FillAllResultsWith(false);
  RunCallback();
}

absl::optional<std::u16string> ContentAnalysisDelegate::GetCustomMessage()
    const {
  auto element = data_.settings.tags.find(final_result_tag_);
  if (element != data_.settings.tags.end() &&
      !element->second.custom_message.message.empty()) {
    return l10n_util::GetStringFUTF16(IDS_DEEP_SCANNING_DIALOG_CUSTOM_MESSAGE,
                                      element->second.custom_message.message);
  }

  return absl::nullopt;
}

absl::optional<GURL> ContentAnalysisDelegate::GetCustomLearnMoreUrl() const {
  auto element = data_.settings.tags.find(final_result_tag_);
  if (element != data_.settings.tags.end() &&
      element->second.custom_message.learn_more_url.is_valid() &&
      !element->second.custom_message.learn_more_url.is_empty()) {
    return element->second.custom_message.learn_more_url;
  }

  return absl::nullopt;
}

bool ContentAnalysisDelegate::BypassRequiresJustification() const {
  return data_.settings.tags.count(final_result_tag_) &&
         data_.settings.tags.at(final_result_tag_).requires_justification;
}

std::u16string ContentAnalysisDelegate::GetBypassJustificationLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_DEEP_SCANNING_DIALOG_UPLOAD_BYPASS_JUSTIFICATION_LABEL);
}

absl::optional<std::u16string>
ContentAnalysisDelegate::OverrideCancelButtonText() const {
  return absl::nullopt;
}

// static
bool ContentAnalysisDelegate::IsEnabled(Profile* profile,
                                        GURL url,
                                        Data* data,
                                        AnalysisConnector connector) {
  auto* service = ConnectorsServiceFactory::GetForBrowserContext(profile);
  // If the corresponding Connector policy isn't set, don't perform scans.
  if (!service || !service->IsConnectorEnabled(connector))
    return false;

  // Check that |url| matches the appropriate URL patterns by getting settings.
  // No settings means no matches were found.
  auto settings = service->GetAnalysisSettings(url, connector);
  if (!settings.has_value()) {
    return false;
  }

  data->settings = std::move(settings.value());
  if (url.is_valid())
    data->url = url;

  return true;
}

// static
void ContentAnalysisDelegate::CreateForWebContents(
    content::WebContents* web_contents,
    Data data,
    CompletionCallback callback,
    safe_browsing::DeepScanAccessPoint access_point) {
  Factory* testing_factory = GetFactoryStorage();
  bool wait_for_verdict =
      data.settings.block_until_verdict == BlockUntilVerdict::kBlock;
  // Using new instead of std::make_unique<> to access non public constructor.
  auto delegate = testing_factory->is_null()
                      ? base::WrapUnique(new ContentAnalysisDelegate(
                            web_contents, std::move(data), std::move(callback),
                            access_point))
                      : testing_factory->Run(web_contents, std::move(data),
                                             std::move(callback));

  bool work_being_done = delegate->UploadData();

  // Only show UI if work is being done in the background, the user must
  // wait for a verdict.
  bool show_ui = work_being_done && wait_for_verdict && (*UIEnabledStorage());

  // If the UI is enabled, create the modal dialog.
  if (show_ui) {
    ContentAnalysisDelegate* delegate_ptr = delegate.get();
    int files_count = delegate_ptr->data_.paths.size();

    // This dialog is owned by the constrained_window code.
    delegate_ptr->dialog_ = new ContentAnalysisDialog(
        std::move(delegate),
        delegate_ptr->data_.settings.cloud_or_local_settings
            .is_cloud_analysis(),
        web_contents, access_point, files_count);
    return;
  }

  if (!wait_for_verdict || !work_being_done) {
    // The UI will not be shown but the policy is set to not wait for the
    // verdict, or no scans need to be performed.  Inform the caller that they
    // may proceed.
    //
    // Supporting "wait for verdict" while not showing a UI makes writing tests
    // for callers of this code easier.
    delegate->FillAllResultsWith(true);
    delegate->RunCallback();
  }

  // If all requests are already done, just let `delegate` go out of scope.
  if (delegate->all_work_done_) {
    return;
  }

  // ... otherwise, let the last response from the upload service callback
  // delete the delegate when there is no more work.
  if (work_being_done) {
    delegate.release();
  }
}

// static
void ContentAnalysisDelegate::SetFactoryForTesting(Factory factory) {
  *GetFactoryStorage() = factory;
}

// static
void ContentAnalysisDelegate::ResetFactoryForTesting() {
  if (GetFactoryStorage())
    GetFactoryStorage()->Reset();
}

// static
void ContentAnalysisDelegate::DisableUIForTesting() {
  *UIEnabledStorage() = false;
}

// TODO(b/283067315): Add this to all the test TearDown()s.
// static
void ContentAnalysisDelegate::EnableUIAfterTesting() {
  *UIEnabledStorage() = true;
}

// static
void ContentAnalysisDelegate::SetOnAckAllRequestsCallbackForTesting(
    OnAckAllRequestsCallback callback) {
  *OnAckAllRequestsStorage() = std::move(callback);
}

void ContentAnalysisDelegate::SetPageWarningForTesting(
    ContentAnalysisResponse page_response) {
  page_warning_ = true;
  page_response_ = std::move(page_response);
}

ContentAnalysisDelegate::ContentAnalysisDelegate(
    content::WebContents* web_contents,
    Data data,
    CompletionCallback callback,
    safe_browsing::DeepScanAccessPoint access_point)
    : data_(std::move(data)),
      callback_(std::move(callback)),
      access_point_(access_point) {
  DCHECK(web_contents);
  profile_ = Profile::FromBrowserContext(web_contents->GetBrowserContext());
  url_ = web_contents->GetLastCommittedURL();
  title_ = base::UTF16ToUTF8(web_contents->GetTitle());
  std::string user_action_token = base::RandBytesAsString(128);
  user_action_id_ =
      base::HexEncode(user_action_token.data(), user_action_token.size());
  page_content_type_ = web_contents->GetContentsMimeType();
  result_.text_results.resize(data_.text.size(), false);
  result_.image_result = false;
  result_.paths_results.resize(data_.paths.size(), false);
  result_.page_result = false;

  // This setter is technically redundant with other code in the class, but
  // is useful to make unit tests behave predictably so the ordering in which
  // each type of request is made doesn't matter.
  files_request_complete_ = data_.paths.empty();
}

void ContentAnalysisDelegate::StringRequestCallback(
    BinaryUploadService::Result result,
    ContentAnalysisResponse response) {
  // Remember to send an ack for this response.
  if (result == safe_browsing::BinaryUploadService::Result::SUCCESS)
    final_actions_[response.request_token()] = GetAckFinalAction(response);

  int64_t content_size = 0;
  for (const std::string& entry : data_.text)
    content_size += entry.size();
  RecordDeepScanMetrics(
      data_.settings.cloud_or_local_settings.is_cloud_analysis(), access_point_,
      base::TimeTicks::Now() - upload_start_time_, content_size, result,
      response);

  text_request_complete_ = true;

  string_request_result_ =
      CalculateRequestHandlerResult(data_.settings, result, response);

  bool text_complies = string_request_result_.complies;
  bool should_warn = string_request_result_.final_result ==
                     FinalContentAnalysisResult::WARNING;

  std::fill(result_.text_results.begin(), result_.text_results.end(),
            text_complies);

  MaybeReportDeepScanningVerdict(
      profile_, url_, "", "", "Text data", std::string(), "text/plain",
      extensions::SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload,
      access_point_, content_size, result, response,
      CalculateEventResult(data_.settings, text_complies, should_warn));

  UpdateFinalResult(string_request_result_.final_result,
                    string_request_result_.tag);

  if (should_warn) {
    text_warning_ = true;
    text_response_ = std::move(response);
  }

  MaybeCompleteScanRequest();
}

void ContentAnalysisDelegate::ImageRequestCallback(
    BinaryUploadService::Result result,
    ContentAnalysisResponse response) {
  // Remember to send an ack for this response.
  if (result == safe_browsing::BinaryUploadService::Result::SUCCESS) {
    final_actions_[response.request_token()] = GetAckFinalAction(response);
  }

  RecordDeepScanMetrics(
      data_.settings.cloud_or_local_settings.is_cloud_analysis(), access_point_,
      base::TimeTicks::Now() - upload_start_time_, data_.image.size(), result,
      response);

  image_request_complete_ = true;

  image_request_result_ =
      CalculateRequestHandlerResult(data_.settings, result, response);

  bool image_complies = image_request_result_.complies;
  bool should_warn =
      image_request_result_.final_result == FinalContentAnalysisResult::WARNING;

  result_.image_result = image_complies;

  MaybeReportDeepScanningVerdict(
      profile_, url_, "", "", "Image data", std::string(),
      /*mime_type*/ std::string(),
      extensions::SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload,
      access_point_, data_.image.size(), result, response,
      CalculateEventResult(data_.settings, image_complies, should_warn));

  UpdateFinalResult(image_request_result_.final_result,
                    image_request_result_.tag);

  if (should_warn) {
    image_warning_ = true;
    image_response_ = std::move(response);
  }

  MaybeCompleteScanRequest();
}

void ContentAnalysisDelegate::FilesRequestCallback(
    std::vector<RequestHandlerResult> results) {
  // Remember to send acks for any responses.
  files_request_handler_->AppendFinalActionsTo(&final_actions_);

  // No reporting here, because the MultiFileRequestHandler does that.
  DCHECK_EQ(results.size(), result_.paths_results.size());
  for (size_t index = 0; index < results.size(); ++index) {
    FinalContentAnalysisResult result = results[index].final_result;
    result_.paths_results[index] = results[index].complies;
    if (result == FinalContentAnalysisResult::WARNING) {
      warned_file_indices_.push_back(index);
    }
    UpdateFinalResult(result, results[index].tag);
  }
  files_request_results_ = std::move(results);
  files_request_complete_ = true;

  MaybeCompleteScanRequest();
}

FilesRequestHandler*
ContentAnalysisDelegate::GetFilesRequestHandlerForTesting() {
  return files_request_handler_.get();
}

bool ContentAnalysisDelegate::ShowFinalResultInDialog() {
  if (!dialog_)
    return false;

  dialog_->ShowResult(final_result_);
  return true;
}

bool ContentAnalysisDelegate::CancelDialog() {
  if (!dialog_)
    return false;

  dialog_->CancelDialogAndDelete();
  return true;
}

void ContentAnalysisDelegate::PageRequestCallback(
    BinaryUploadService::Result result,
    ContentAnalysisResponse response) {
  // Remember to send an ack for this response.
  if (result == safe_browsing::BinaryUploadService::Result::SUCCESS)
    final_actions_[response.request_token()] = GetAckFinalAction(response);

  RecordDeepScanMetrics(
      data_.settings.cloud_or_local_settings.is_cloud_analysis(), access_point_,
      base::TimeTicks::Now() - upload_start_time_, page_size_bytes_, result,
      response);

  page_request_complete_ = true;

  RequestHandlerResult request_handler_result =
      CalculateRequestHandlerResult(data_.settings, result, response);

  result_.page_result = request_handler_result.complies;
  bool should_warn = request_handler_result.final_result ==
                     FinalContentAnalysisResult::WARNING;

  MaybeReportDeepScanningVerdict(
      profile_, url_, "", /*destination*/ data_.printer_name, title_,
      /*sha256*/ std::string(),
      /*mime_type*/ std::string(),
      extensions::SafeBrowsingPrivateEventRouter::kTriggerPagePrint,
      access_point_, /*content_size*/ -1, result, response,
      CalculateEventResult(data_.settings, result_.page_result, should_warn));

  UpdateFinalResult(request_handler_result.final_result,
                    request_handler_result.tag);

  if (should_warn) {
    page_warning_ = true;
    page_response_ = std::move(response);
  }

  MaybeCompleteScanRequest();
}

bool ContentAnalysisDelegate::UploadData() {
  upload_start_time_ = base::TimeTicks::Now();

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
  // If this is a local content analysis, check if the local agent is ready.
  // If not, abort early.  This is to prevent doing a lot of work, like reading
  // files into memory or calcuating SHA256 hashes and prevent a flash of the
  // in-progress dialog.
  const CloudOrLocalAnalysisSettings& cloud_or_local =
      data_.settings.cloud_or_local_settings;
  if (cloud_or_local.is_local_analysis()) {
    auto client = ContentAnalysisSdkManager::Get()->GetClient(
        {cloud_or_local.local_path(), cloud_or_local.user_specific()});
    if (!client) {
      return false;
    }
  }
#endif

  // Create a text request, an image request, a page request and a file request
  // for each file.
  PrepareTextRequest();
  PrepareImageRequest();
  PreparePageRequest();

  if (!data_.paths.empty()) {
    // Passing the settings using a reference is safe here, because
    // MultiFileRequestHandler is owned by this class.
    files_request_handler_ = FilesRequestHandler::Create(
        GetBinaryUploadService(), profile_, data_.settings, url_, "", "",
        user_action_id_, title_, access_point_, data_.reason, data_.paths,
        base::BindOnce(&ContentAnalysisDelegate::FilesRequestCallback,
                       GetWeakPtr()));
    files_request_complete_ = !files_request_handler_->UploadData();
  } else {
    // If no files should be uploaded, the file request is complete.
    files_request_complete_ = true;
  }
  data_uploaded_ = true;
  // Do not add code under this comment. The above line should be the last thing
  // this function does before the return statement.

  return !text_request_complete_ || !image_request_complete_ ||
         !files_request_complete_ || !page_request_complete_;
}

void ContentAnalysisDelegate::PrepareTextRequest() {
  std::string full_text;
  for (const std::string& text : data_.text)
    full_text.append(text);

  // The request is considered complete if there is no text or if the text is
  // too small compared to the minimum size. This means a minimum_data_size of
  // 0 is equivalent to no minimum, as the second part of the "or" will always
  // be false.
  text_request_complete_ =
      full_text.empty() || full_text.size() < data_.settings.minimum_data_size;

  if (!full_text.empty()) {
    base::UmaHistogramCustomCounts("Enterprise.OnBulkDataEntry.DataSize",
                                   full_text.size(),
                                   /*min=*/1,
                                   /*max=*/51 * 1024 * 1024,
                                   /*buckets=*/50);
  }

  if (!text_request_complete_) {
    auto request = std::make_unique<StringAnalysisRequest>(
        data_.settings.cloud_or_local_settings, std::move(full_text),
        base::BindOnce(&ContentAnalysisDelegate::StringRequestCallback,
                       weak_ptr_factory_.GetWeakPtr()));

    PrepareRequest(BULK_DATA_ENTRY, request.get());
    UploadTextForDeepScanning(std::move(request));
  }
}

void ContentAnalysisDelegate::PrepareImageRequest() {
  // The request is considered complete if there is no image or if the image is
  // too large compared to the maximum size.
  image_request_complete_ =
      data_.image.empty() ||
      data_.image.size() >
          data_.settings.cloud_or_local_settings.max_file_size();

  if (!data_.image.empty()) {
    base::UmaHistogramCustomCounts("Enterprise.OnBulkDataEntry.DataSize",
                                   data_.image.size(),
                                   /*min=*/1,
                                   /*max=*/51 * 1024 * 1024,
                                   /*buckets=*/50);
  }

  if (!image_request_complete_) {
    auto request = std::make_unique<StringAnalysisRequest>(
        data_.settings.cloud_or_local_settings, data_.image,
        base::BindOnce(&ContentAnalysisDelegate::ImageRequestCallback,
                       weak_ptr_factory_.GetWeakPtr()));

    PrepareRequest(BULK_DATA_ENTRY, request.get());
    UploadImageForDeepScanning(std::move(request));
  }
}

void ContentAnalysisDelegate::PreparePageRequest() {
  // The request is considered complete if the mapped region is invalid since it
  // prevents scanning.
  page_request_complete_ = !data_.page.IsValid();

  if (!page_request_complete_) {
    page_size_bytes_ = data_.page.GetSize();
    auto request = std::make_unique<PagePrintAnalysisRequest>(
        data_.settings, std::move(data_.page),
        base::BindOnce(&ContentAnalysisDelegate::PageRequestCallback,
                       weak_ptr_factory_.GetWeakPtr()));

    PrepareRequest(PRINT, request.get());
    request->set_filename(title_);
    if (!data_.printer_name.empty()) {
      request->set_printer_name(data_.printer_name);
    }
    if (!page_content_type_.empty()) {
      request->set_content_type(page_content_type_);
    }
    UploadPageForDeepScanning(std::move(request));
  }
}

// This method only prepares requests for print and paste events. File events
// are handled by
// chrome/browser/enterprise/connectors/analysis/files_request_handler.h
void ContentAnalysisDelegate::PrepareRequest(
    AnalysisConnector connector,
    BinaryUploadService::Request* request) {
  if (data_.settings.cloud_or_local_settings.is_cloud_analysis()) {
    request->set_device_token(
        data_.settings.cloud_or_local_settings.dm_token());
  }

  // Include tab page title, user action id, and count of requests per user
  // action in local content analysis requests.
  if (data_.settings.cloud_or_local_settings.is_local_analysis()) {
    // Increment the total number of user action requests by 1.
    total_requests_count_++;
    request->set_tab_title(title_);
    request->set_user_action_id(user_action_id_);
  }

  request->set_analysis_connector(connector);
  request->set_email(safe_browsing::GetProfileEmail(profile_));
  request->set_url(data_.url.spec());
  request->set_tab_url(data_.url);
  request->set_per_profile_request(data_.settings.per_profile);

  for (const auto& tag : data_.settings.tags) {
    request->add_tag(tag.first);
  }

  if (data_.settings.client_metadata) {
    request->set_client_metadata(*data_.settings.client_metadata);
  }

  if (data_.reason != ContentAnalysisRequest::UNKNOWN) {
    request->set_reason(data_.reason);
  }
}

void ContentAnalysisDelegate::FillAllResultsWith(bool status) {
  std::fill(result_.text_results.begin(), result_.text_results.end(), status);
  result_.image_result = status;
  std::fill(result_.paths_results.begin(), result_.paths_results.end(), status);
  result_.page_result = status;
}

BinaryUploadService* ContentAnalysisDelegate::GetBinaryUploadService() {
  return safe_browsing::BinaryUploadService::GetForProfile(profile_,
                                                           data_.settings);
}

void ContentAnalysisDelegate::UploadTextForDeepScanning(
    std::unique_ptr<BinaryUploadService::Request> request) {
  request->set_user_action_requests_count(total_requests_count_);
  BinaryUploadService* upload_service = GetBinaryUploadService();
  if (upload_service) {
    upload_service->MaybeUploadForDeepScanning(std::move(request));
  }
}

void ContentAnalysisDelegate::UploadImageForDeepScanning(
    std::unique_ptr<BinaryUploadService::Request> request) {
  BinaryUploadService* upload_service = GetBinaryUploadService();
  request->set_user_action_requests_count(total_requests_count_);
  if (upload_service)
    upload_service->MaybeUploadForDeepScanning(std::move(request));
}

void ContentAnalysisDelegate::UploadPageForDeepScanning(
    std::unique_ptr<BinaryUploadService::Request> request) {
  BinaryUploadService* upload_service = GetBinaryUploadService();
  request->set_user_action_requests_count(total_requests_count_);
  if (upload_service)
    upload_service->MaybeUploadForDeepScanning(std::move(request));
}

bool ContentAnalysisDelegate::UpdateDialog() {
  // Only show final result UI in the case of a cloud analysis.
  // In the local case, the local agent does that.
  return data_.settings.cloud_or_local_settings.is_cloud_analysis()
             ? ShowFinalResultInDialog()
             : CancelDialog();
}

void ContentAnalysisDelegate::MaybeCompleteScanRequest() {
  if (!text_request_complete_ || !image_request_complete_ ||
      !files_request_complete_ || !page_request_complete_) {
    return;
  }

  // If showing the warning message, wait before running the callback. The
  // callback will be called either in BypassWarnings or Cancel.
  if (final_result_ != FinalContentAnalysisResult::WARNING)
    RunCallback();

  AckAllRequests();

  if (callback_running_ && !dialog_ && *UIEnabledStorage()) {
    // This code path implies that RunCallback has already been called,
    // and that we are racing against a non-blocking scan. In such a
    // case, we let the other caller handle deletion of `this`, and let
    // them know no more work is needed.
    all_work_done_ = true;
    return;
  }

  if (!UpdateDialog() && data_uploaded_) {
    // No UI was shown.  Delete |this| to cleanup, unless UploadData isn't done
    // yet.
    delete this;
  }
}

void ContentAnalysisDelegate::RunCallback() {
  DCHECK(!callback_running_);
  if (callback_.is_null()) {
    return;
  }

  callback_running_ = true;
  std::move(callback_).Run(data_, result_);
  callback_running_ = false;

  // Since `result_` might have been tweaked by `callback_`, `final_actions_`
  // need to be updated before Acks are sent.
  for (size_t i = 0; i < result_.paths_results.size(); ++i) {
    if (!result_.paths_results[i] && files_request_results_.size() > i &&
        final_actions_.count(files_request_results_[i].request_token) &&
        final_actions_[files_request_results_[i].request_token] ==
            ContentAnalysisAcknowledgement::ALLOW) {
      final_actions_[files_request_results_[i].request_token] =
          ContentAnalysisAcknowledgement::BLOCK;
    }
  }

  // If both image and text are present, synchronize their ack statuses if
  // needed.
  if (!string_request_result_.request_token.empty() &&
      !image_request_result_.request_token.empty()) {
    if (!result_.image_result) {
      final_actions_[string_request_result_.request_token] =
          ContentAnalysisAcknowledgement::BLOCK;
    }
    // text_results is uniformly updated in StringRequestCallback(), so the
    // values should be consistent.
    if (!result_.text_results[0]) {
      final_actions_[image_request_result_.request_token] =
          ContentAnalysisAcknowledgement::BLOCK;
    }
  }
}

void ContentAnalysisDelegate::UpdateFinalResult(
    FinalContentAnalysisResult result,
    const std::string& tag) {
  if (result < final_result_) {
    final_result_ = result;
    final_result_tag_ = tag;
  }
}

void ContentAnalysisDelegate::AckAllRequests() {
  if (!OnAckAllRequestsStorage()->is_null())
    std::move(*OnAckAllRequestsStorage()).Run(final_actions_);

  BinaryUploadService* upload_service = GetBinaryUploadService();
  if (!upload_service)
    return;

  for (const auto& token_and_action : final_actions_) {
    // Only have files that have a request token. Not having one implies that
    // the agent never received the request for some reason (size, encryption,
    // etc.) so it doesn't make sense to send an ack.
    if (!token_and_action.first.empty()) {
      auto ack = std::make_unique<safe_browsing::BinaryUploadService::Ack>(
          data_.settings.cloud_or_local_settings);
      ack->set_request_token(token_and_action.first);
      ack->set_status(ContentAnalysisAcknowledgement::SUCCESS);
      ack->set_final_action(token_and_action.second);
      upload_service->MaybeAcknowledge(std::move(ack));
    }
  }
}

}  // namespace enterprise_connectors
