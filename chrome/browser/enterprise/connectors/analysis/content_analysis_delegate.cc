// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"

#include <algorithm>
#include <numeric>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_dialog.h"
#include "chrome/browser/enterprise/connectors/analysis/page_print_analysis_request.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/file_util_service.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/file_analysis_request.h"
#include "chrome/browser/safe_browsing/download_protection/check_client_download_request.h"
#include "chrome/grit/generated_resources.h"
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

using safe_browsing::BinaryUploadService;

namespace enterprise_connectors {

namespace {

// Global pointer of factory function (RepeatingCallback) used to create
// instances of ContentAnalysisDelegate in tests.  !is_null() only in tests.
ContentAnalysisDelegate::Factory* GetFactoryStorage() {
  static base::NoDestructor<ContentAnalysisDelegate::Factory> factory;
  return factory.get();
}

// A BinaryUploadService::Request implementation that gets the data to scan
// from a string.
class StringAnalysisRequest : public BinaryUploadService::Request {
 public:
  StringAnalysisRequest(GURL analysis_url,
                        std::string text,
                        BinaryUploadService::ContentAnalysisCallback callback);
  ~StringAnalysisRequest() override;

  StringAnalysisRequest(const StringAnalysisRequest&) = delete;
  StringAnalysisRequest& operator=(const StringAnalysisRequest&) = delete;

  // BinaryUploadService::Request implementation.
  void GetRequestData(DataCallback callback) override;

 private:
  Data data_;
  BinaryUploadService::Result result_ =
      BinaryUploadService::Result::FILE_TOO_LARGE;
};

StringAnalysisRequest::StringAnalysisRequest(
    GURL analysis_url,
    std::string text,
    BinaryUploadService::ContentAnalysisCallback callback)
    : Request(std::move(callback), analysis_url) {
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
  std::move(callback).Run(result_, std::move(data_));
}

bool ContentAnalysisActionAllowsDataUse(
    enterprise_connectors::TriggeredRule::Action action) {
  switch (action) {
    case enterprise_connectors::TriggeredRule::ACTION_UNSPECIFIED:
    case enterprise_connectors::TriggeredRule::REPORT_ONLY:
      return true;
    case enterprise_connectors::TriggeredRule::WARN:
    case enterprise_connectors::TriggeredRule::BLOCK:
      return false;
  }
}

bool* UIEnabledStorage() {
  static bool enabled = true;
  return &enabled;
}

safe_browsing::EventResult CalculateEventResult(
    const enterprise_connectors::AnalysisSettings& settings,
    bool allowed_by_scan_result,
    bool should_warn) {
  bool wait_for_verdict = settings.block_until_verdict ==
                          enterprise_connectors::BlockUntilVerdict::BLOCK;
  return (allowed_by_scan_result || !wait_for_verdict)
             ? safe_browsing::EventResult::ALLOWED
             : (should_warn ? safe_browsing::EventResult::WARNED
                            : safe_browsing::EventResult::BLOCKED);
}

}  // namespace

ContentAnalysisDelegate::Data::Data() = default;
ContentAnalysisDelegate::Data::Data(Data&& other) = default;
ContentAnalysisDelegate::Data::~Data() = default;

ContentAnalysisDelegate::Result::Result() = default;
ContentAnalysisDelegate::Result::Result(Result&& other) = default;
ContentAnalysisDelegate::Result::~Result() = default;

ContentAnalysisDelegate::FileInfo::FileInfo() = default;
ContentAnalysisDelegate::FileInfo::FileInfo(FileInfo&& other) = default;
ContentAnalysisDelegate::FileInfo::~FileInfo() = default;

ContentAnalysisDelegate::FileContents::FileContents() = default;
ContentAnalysisDelegate::FileContents::FileContents(
    BinaryUploadService::Result result)
    : result(result) {}

ContentAnalysisDelegate::FileContents::FileContents(FileContents&& other) =
    default;
ContentAnalysisDelegate::FileContents&
ContentAnalysisDelegate::FileContents::operator=(
    ContentAnalysisDelegate::FileContents&& other) = default;

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
        profile_, url_, "Text data", std::string(), "text/plain",
        extensions::SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload,
        access_point_, content_size, text_response_, user_justification);
  }

  // Mark every "warning" file as complying and report a warning bypass.
  for (const auto& warning : file_warnings_) {
    size_t index = warning.first;
    result_.paths_results[index] = true;

    ReportAnalysisConnectorWarningBypass(
        profile_, url_, data_.paths[index].AsUTF8Unsafe(),
        file_info_[index].sha256, file_info_[index].mime_type,
        extensions::SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
        access_point_, file_info_[index].size, warning.second,
        user_justification);
  }

  // Mark the printed page as complying and report a warning bypass.
  if (page_warning_) {
    result_.page_result = true;

    ReportAnalysisConnectorWarningBypass(
        profile_, url_, "Printed page", /*sha256*/ std::string(),
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
    RecordDeepScanMetrics(access_point_,
                          base::TimeTicks::Now() - upload_start_time_, 0,
                          "CancelledByUser", false);
  }

  // Make sure to reject everything.
  FillAllResultsWith(false);
  RunCallback();
}

absl::optional<std::u16string> ContentAnalysisDelegate::GetCustomMessage()
    const {
  auto element = data_.settings.custom_message_data.find(final_result_tag_);
  if (element != data_.settings.custom_message_data.end() &&
      !element->second.message.empty()) {
    return l10n_util::GetStringFUTF16(IDS_DEEP_SCANNING_DIALOG_CUSTOM_MESSAGE,
                                      element->second.message);
  }

  return absl::nullopt;
}

absl::optional<GURL> ContentAnalysisDelegate::GetCustomLearnMoreUrl() const {
  auto element = data_.settings.custom_message_data.find(final_result_tag_);
  if (element != data_.settings.custom_message_data.end() &&
      element->second.learn_more_url.is_valid() &&
      !element->second.learn_more_url.is_empty()) {
    return element->second.learn_more_url;
  }

  return absl::nullopt;
}

bool ContentAnalysisDelegate::BypassRequiresJustification() const {
  if (!base::FeatureList::IsEnabled(kBypassJustificationEnabled))
    return false;

  return data_.settings.tags_requiring_justification.count(final_result_tag_) >
         0;
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
bool ContentAnalysisDelegate::ResultShouldAllowDataUse(
    BinaryUploadService::Result result,
    const enterprise_connectors::AnalysisSettings& settings) {
  // Keep this implemented as a switch instead of a simpler if statement so that
  // new values added to BinaryUploadService::Result cause a compiler error.
  switch (result) {
    case BinaryUploadService::Result::SUCCESS:
    case BinaryUploadService::Result::UPLOAD_FAILURE:
    case BinaryUploadService::Result::TIMEOUT:
    case BinaryUploadService::Result::FAILED_TO_GET_TOKEN:
    case BinaryUploadService::Result::TOO_MANY_REQUESTS:
    // UNAUTHORIZED allows data usage since it's a result only obtained if the
    // browser is not authorized to perform deep scanning. It does not make
    // sense to block data in this situation since no actual scanning of the
    // data was performed, so it's allowed.
    case BinaryUploadService::Result::UNAUTHORIZED:
    case BinaryUploadService::Result::UNKNOWN:
      return true;

    case BinaryUploadService::Result::FILE_TOO_LARGE:
      return !settings.block_large_files;

    case BinaryUploadService::Result::FILE_ENCRYPTED:
      return !settings.block_password_protected_files;

    case BinaryUploadService::Result::DLP_SCAN_UNSUPPORTED_FILE_TYPE:
      return !settings.block_unsupported_file_types;
  }
}

// static
bool ContentAnalysisDelegate::IsEnabled(
    Profile* profile,
    GURL url,
    Data* data,
    enterprise_connectors::AnalysisConnector connector) {
  auto* service =
      enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
          profile);
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
  bool wait_for_verdict = data.settings.block_until_verdict ==
                          enterprise_connectors::BlockUntilVerdict::BLOCK;
  // Using new instead of std::make_unique<> to access non public constructor.
  auto delegate =
      testing_factory->is_null()
          ? std::unique_ptr<ContentAnalysisDelegate>(
                new ContentAnalysisDelegate(web_contents, std::move(data),
                                            std::move(callback), access_point))
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
    delegate_ptr->dialog_ =
        new ContentAnalysisDialog(std::move(delegate), web_contents,
                                  std::move(access_point), files_count);
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

  // Upload service callback will delete the delegate.
  if (work_being_done)
    delegate.release();
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
  result_.text_results.resize(data_.text.size(), false);
  result_.paths_results.resize(data_.paths.size(), false);
  result_.page_result = false;
  file_info_.resize(data_.paths.size());
}

void ContentAnalysisDelegate::StringRequestCallback(
    BinaryUploadService::Result result,
    enterprise_connectors::ContentAnalysisResponse response) {
  int64_t content_size = 0;
  for (const std::string& entry : data_.text)
    content_size += entry.size();
  RecordDeepScanMetrics(access_point_,
                        base::TimeTicks::Now() - upload_start_time_,
                        content_size, result, response);

  text_request_complete_ = true;
  std::string tag;
  auto action =
      enterprise_connectors::GetHighestPrecedenceAction(response, &tag);
  bool text_complies = ResultShouldAllowDataUse(result, data_.settings) &&
                       ContentAnalysisActionAllowsDataUse(action);
  bool should_warn = action == enterprise_connectors::ContentAnalysisResponse::
                                   Result::TriggeredRule::WARN;

  std::fill(result_.text_results.begin(), result_.text_results.end(),
            text_complies);

  MaybeReportDeepScanningVerdict(
      profile_, url_, "Text data", std::string(), "text/plain",
      extensions::SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload,
      access_point_, content_size, result, response,
      CalculateEventResult(data_.settings, text_complies, should_warn));

  if (!text_complies) {
    if (should_warn) {
      text_warning_ = true;
      text_response_ = std::move(response);
      UpdateFinalResult(ContentAnalysisDelegateBase::FinalResult::WARNING, tag);
    } else {
      UpdateFinalResult(ContentAnalysisDelegateBase::FinalResult::FAILURE, tag);
    }
  }

  MaybeCompleteScanRequest();
}

void ContentAnalysisDelegate::FileRequestCallback(
    base::FilePath path,
    BinaryUploadService::Result result,
    enterprise_connectors::ContentAnalysisResponse response) {
  if (result == BinaryUploadService::Result::TOO_MANY_REQUESTS)
    throttled_ = true;

  // Find the path in the set of files that are being scanned.
  auto it = std::find(data_.paths.begin(), data_.paths.end(), path);
  DCHECK(it != data_.paths.end());
  size_t index = std::distance(data_.paths.begin(), it);

  RecordDeepScanMetrics(access_point_,
                        base::TimeTicks::Now() - upload_start_time_,
                        file_info_[index].size, result, response);

  std::string tag;
  auto action = GetHighestPrecedenceAction(response, &tag);
  bool file_complies = ResultShouldAllowDataUse(result, data_.settings) &&
                       ContentAnalysisActionAllowsDataUse(action);
  bool should_warn = action == enterprise_connectors::TriggeredRule::WARN;
  result_.paths_results[index] = file_complies;

  MaybeReportDeepScanningVerdict(
      profile_, url_, path.AsUTF8Unsafe(), file_info_[index].sha256,
      file_info_[index].mime_type,
      extensions::SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
      access_point_, file_info_[index].size, result, response,
      CalculateEventResult(data_.settings, file_complies, should_warn));

  ++file_result_count_;

  if (!file_complies) {
    if (result == BinaryUploadService::Result::FILE_TOO_LARGE) {
      UpdateFinalResult(ContentAnalysisDelegateBase::FinalResult::LARGE_FILES,
                        tag);
    } else if (result == BinaryUploadService::Result::FILE_ENCRYPTED) {
      UpdateFinalResult(
          ContentAnalysisDelegateBase::FinalResult::ENCRYPTED_FILES, tag);
    } else if (should_warn) {
      file_warnings_[index] = std::move(response);
      UpdateFinalResult(ContentAnalysisDelegateBase::FinalResult::WARNING, tag);
    } else {
      UpdateFinalResult(ContentAnalysisDelegateBase::FinalResult::FAILURE, tag);
    }
  }

  safe_browsing::DecrementCrashKey(
      safe_browsing::ScanningCrashKey::PENDING_FILE_UPLOADS);
  MaybeCompleteScanRequest();
}

void ContentAnalysisDelegate::PageRequestCallback(
    BinaryUploadService::Result result,
    enterprise_connectors::ContentAnalysisResponse response) {
  RecordDeepScanMetrics(access_point_,
                        base::TimeTicks::Now() - upload_start_time_,
                        page_size_bytes_, result, response);

  page_request_complete_ = true;
  std::string tag;
  auto action =
      enterprise_connectors::GetHighestPrecedenceAction(response, &tag);
  result_.page_result = ResultShouldAllowDataUse(result, data_.settings) &&
                        ContentAnalysisActionAllowsDataUse(action);
  bool should_warn = action == enterprise_connectors::ContentAnalysisResponse::
                                   Result::TriggeredRule::WARN;

  MaybeReportDeepScanningVerdict(
      profile_, url_, "Printed page", /*sha256*/ std::string(),
      /*mime_type*/ std::string(),
      extensions::SafeBrowsingPrivateEventRouter::kTriggerPagePrint,
      access_point_, /*content_size*/ -1, result, response,
      CalculateEventResult(data_.settings, result_.page_result, should_warn));

  if (!result_.page_result) {
    if (result == BinaryUploadService::Result::FILE_TOO_LARGE) {
      UpdateFinalResult(ContentAnalysisDelegateBase::FinalResult::LARGE_FILES,
                        tag);
    } else if (should_warn) {
      page_warning_ = true;
      page_response_ = std::move(response);
      UpdateFinalResult(ContentAnalysisDelegateBase::FinalResult::WARNING, tag);
    } else {
      UpdateFinalResult(ContentAnalysisDelegateBase::FinalResult::FAILURE, tag);
    }
  }

  MaybeCompleteScanRequest();
}

bool ContentAnalysisDelegate::UploadData() {
  upload_start_time_ = base::TimeTicks::Now();

  // Create a text request, a page request and a file request for each file.
  PrepareTextRequest();
  PreparePageRequest();
  safe_browsing::IncrementCrashKey(
      safe_browsing::ScanningCrashKey::PENDING_FILE_UPLOADS,
      data_.paths.size());
  if (!data_.paths.empty()) {
    safe_browsing::IncrementCrashKey(
        safe_browsing::ScanningCrashKey::TOTAL_FILE_UPLOADS,
        data_.paths.size());

    std::vector<safe_browsing::FileOpeningJob::FileOpeningTask> tasks(
        data_.paths.size());
    for (size_t i = 0; i < data_.paths.size(); ++i)
      tasks[i].request = PrepareFileRequest(data_.paths[i]);

    file_opening_job_ =
        std::make_unique<safe_browsing::FileOpeningJob>(std::move(tasks));
  }

  data_uploaded_ = true;
  // Do not add code under this comment. The above line should be the last thing
  // this function does before the return statement.

  return !text_request_complete_ || file_result_count_ != data_.paths.size() ||
         !page_request_complete_;
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
        data_.settings.analysis_url, std::move(full_text),
        base::BindOnce(&ContentAnalysisDelegate::StringRequestCallback,
                       weak_ptr_factory_.GetWeakPtr()));

    PrepareRequest(enterprise_connectors::BULK_DATA_ENTRY, request.get());
    UploadTextForDeepScanning(std::move(request));
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

    PrepareRequest(enterprise_connectors::PRINT, request.get());
    UploadPageForDeepScanning(std::move(request));
  }
}

safe_browsing::FileAnalysisRequest* ContentAnalysisDelegate::PrepareFileRequest(
    const base::FilePath& path) {
  auto request = std::make_unique<safe_browsing::FileAnalysisRequest>(
      data_.settings, path, path.BaseName(), /*mime_type*/ "",
      /* delay_opening_file */ true,
      base::BindOnce(&ContentAnalysisDelegate::FileRequestCallback,
                     weak_ptr_factory_.GetWeakPtr(), path));
  safe_browsing::FileAnalysisRequest* request_raw = request.get();
  PrepareRequest(enterprise_connectors::FILE_ATTACHED, request_raw);
  request_raw->GetRequestData(
      base::BindOnce(&ContentAnalysisDelegate::OnGotFileInfo,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request), path));

  return request_raw;
}

void ContentAnalysisDelegate::PrepareRequest(
    enterprise_connectors::AnalysisConnector connector,
    BinaryUploadService::Request* request) {
  request->set_device_token(data_.settings.dm_token);
  request->set_analysis_connector(connector);
  request->set_email(safe_browsing::GetProfileEmail(profile_));
  request->set_url(data_.url.spec());
  request->set_tab_url(data_.url);
  request->set_per_profile_request(data_.settings.per_profile);
  for (const std::string& tag : data_.settings.tags)
    request->add_tag(tag);
  if (data_.settings.client_metadata)
    request->set_client_metadata(*data_.settings.client_metadata);
}

void ContentAnalysisDelegate::FillAllResultsWith(bool status) {
  std::fill(result_.text_results.begin(), result_.text_results.end(), status);
  std::fill(result_.paths_results.begin(), result_.paths_results.end(), status);
  result_.page_result = status;
}

BinaryUploadService* ContentAnalysisDelegate::GetBinaryUploadService() {
  return safe_browsing::BinaryUploadServiceFactory::GetForProfile(profile_);
}

void ContentAnalysisDelegate::UploadTextForDeepScanning(
    std::unique_ptr<BinaryUploadService::Request> request) {
  BinaryUploadService* upload_service = GetBinaryUploadService();
  if (upload_service)
    upload_service->MaybeUploadForDeepScanning(std::move(request));
}

void ContentAnalysisDelegate::UploadFileForDeepScanning(
    BinaryUploadService::Result result,
    const base::FilePath& path,
    std::unique_ptr<BinaryUploadService::Request> request) {
  BinaryUploadService* upload_service = GetBinaryUploadService();
  if (upload_service)
    upload_service->MaybeUploadForDeepScanning(std::move(request));
}

void ContentAnalysisDelegate::UploadPageForDeepScanning(
    std::unique_ptr<BinaryUploadService::Request> request) {
  BinaryUploadService* upload_service = GetBinaryUploadService();
  if (upload_service)
    upload_service->MaybeUploadForDeepScanning(std::move(request));
}

bool ContentAnalysisDelegate::UpdateDialog() {
  if (!dialog_)
    return false;

  dialog_->ShowResult(final_result_);
  return true;
}

void ContentAnalysisDelegate::MaybeCompleteScanRequest() {
  if (!text_request_complete_ || file_result_count_ < data_.paths.size() ||
      !page_request_complete_) {
    return;
  }

  // If showing the warning message, wait before running the callback. The
  // callback will be called either in BypassWarnings or Cancel.
  if (final_result_ != ContentAnalysisDelegateBase::FinalResult::WARNING)
    RunCallback();

  if (!UpdateDialog() && data_uploaded_) {
    // No UI was shown.  Delete |this| to cleanup, unless UploadData isn't done
    // yet.
    delete this;
  }
}

void ContentAnalysisDelegate::RunCallback() {
  if (!callback_.is_null())
    std::move(callback_).Run(data_, result_);
}

void ContentAnalysisDelegate::OnGotFileInfo(
    std::unique_ptr<BinaryUploadService::Request> request,
    const base::FilePath& path,
    BinaryUploadService::Result result,
    BinaryUploadService::Request::Data data) {
  auto it = std::find(data_.paths.begin(), data_.paths.end(), path);
  DCHECK(it != data_.paths.end());
  size_t index = std::distance(data_.paths.begin(), it);
  file_info_[index].sha256 = data.hash;
  file_info_[index].size = data.size;
  file_info_[index].mime_type = data.mime_type;

  // If a non-SUCCESS result was previously obtained, it means the file has some
  // property (too large, unsupported file type, encrypted, ...) that make its
  // upload pointless, so the request should finish early.
  if (result != BinaryUploadService::Result::SUCCESS) {
    request->FinishRequest(result,
                           enterprise_connectors::ContentAnalysisResponse());
    return;
  }

  // If |throttled_| is true, then the file shouldn't be upload since the server
  // is receiving too many requests.
  if (throttled_) {
    request->FinishRequest(BinaryUploadService::Result::TOO_MANY_REQUESTS,
                           enterprise_connectors::ContentAnalysisResponse());
    return;
  }

  UploadFileForDeepScanning(result, data_.paths[index], std::move(request));
}

void ContentAnalysisDelegate::UpdateFinalResult(
    ContentAnalysisDelegateBase::FinalResult result,
    const std::string& tag) {
  if (result < final_result_) {
    final_result_ = result;
    final_result_tag_ = tag;
  }
}

}  // namespace enterprise_connectors
