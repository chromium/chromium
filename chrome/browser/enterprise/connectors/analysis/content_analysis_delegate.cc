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
#include "chrome/browser/enterprise/connectors/analysis/clipboard_analysis_request.h"
#include "chrome/browser/enterprise/connectors/analysis/clipboard_request_handler.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_dialog_controller.h"
#include "chrome/browser/enterprise/connectors/analysis/files_request_handler.h"
#include "chrome/browser/enterprise/connectors/analysis/page_print_analysis_request.h"
#include "chrome/browser/enterprise/connectors/analysis/page_print_request_handler.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/referrer_cache_utils.h"
#include "chrome/browser/file_util_service.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_enterprise_url_lookup_service_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/file_analysis_request.h"
#include "chrome/browser/safe_browsing/download_protection/check_client_download_request.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/common/files_scan_data.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/enterprise/connectors/core/reporting_utils.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/policy/core/common/chrome_schema.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sessions/content/session_tab_helper.h"
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

void OnContentAnalysisComplete(
    std::unique_ptr<FilesScanData> files_scan_data,
    ContentAnalysisDelegate::ForFilesCompletionCallback callback,
    const ContentAnalysisDelegate::Data& data,
    ContentAnalysisDelegate::Result& result) {
  std::set<size_t> file_indexes_to_block =
      files_scan_data->IndexesToBlock(result.paths_results);

  std::vector<bool> allowed;
  allowed.reserve(files_scan_data->base_paths().size());
  for (size_t i = 0; i < files_scan_data->base_paths().size(); ++i) {
    allowed.push_back(file_indexes_to_block.count(i) == 0);
  }

  std::move(callback).Run(files_scan_data->take_base_paths(),
                          std::move(allowed));
}

void OnPathsExpanded(
    base::WeakPtr<content::WebContents> web_contents,
    DeepScanAccessPoint access_point,
    ContentAnalysisDelegate::Data data,
    std::unique_ptr<FilesScanData> files_scan_data,
    ContentAnalysisDelegate::ForFilesCompletionCallback callback) {
  if (!web_contents) {
    size_t size = files_scan_data->base_paths().size();
    std::move(callback).Run(files_scan_data->take_base_paths(),
                            std::vector<bool>(size, true));
    return;
  }

  data.paths = files_scan_data->expanded_paths();
  ContentAnalysisDelegate::CreateForWebContents(
      web_contents.get(), std::move(data),
      base::BindOnce(&OnContentAnalysisComplete, std::move(files_scan_data),
                     std::move(callback)),
      access_point);
}

}  // namespace

ContentAnalysisDelegate::Data::Data() = default;
ContentAnalysisDelegate::Data::Data(Data&& other) = default;
ContentAnalysisDelegate::Data& ContentAnalysisDelegate::Data::operator=(
    ContentAnalysisDelegate::Data&& other) = default;
ContentAnalysisDelegate::Data::~Data() = default;

void ContentAnalysisDelegate::Data::AddClipboardData(
    const content::ClipboardPasteData& clipboard_paste_data) {
  if (!clipboard_paste_data.text.empty()) {
    text.push_back(base::UTF16ToUTF8(clipboard_paste_data.text));
  }
  if (!clipboard_paste_data.html.empty()) {
    text.push_back(base::UTF16ToUTF8(clipboard_paste_data.html));
  }
  if (!clipboard_paste_data.svg.empty()) {
    text.push_back(base::UTF16ToUTF8(clipboard_paste_data.svg));
  }
  if (!clipboard_paste_data.rtf.empty()) {
    text.push_back(clipboard_paste_data.rtf);
  }
  if (!clipboard_paste_data.png.empty()) {
      image = std::string(clipboard_paste_data.png.begin(),
                          clipboard_paste_data.png.end());
  }
  if (!clipboard_paste_data.custom_data.empty()) {
    for (const auto& entry : clipboard_paste_data.custom_data) {
      text.push_back(base::UTF16ToUTF8(entry.second));
    }
  }
}

ContentAnalysisDelegate::Result::Result() = default;
ContentAnalysisDelegate::Result::Result(Result&& other) = default;
ContentAnalysisDelegate::Result::~Result() = default;

ContentAnalysisDelegate::~ContentAnalysisDelegate() = default;

void ContentAnalysisDelegate::BypassWarnings(
    std::optional<std::u16string> user_justification) {
  if (callback_.is_null()) {
    return;
  }

  // Mark the full text as complying and report a warning bypass.
  if (text_request_result_.final_result ==
      FinalContentAnalysisResult::WARNING) {
    std::fill(result_.text_results.begin(), result_.text_results.end(), true);

    text_request_handler_->ReportWarningBypass(user_justification);
  }

  // Mark the full image as complying and report a warning bypass.
  if (image_request_result_.final_result ==
      FinalContentAnalysisResult::WARNING) {
    result_.image_result = true;

    image_request_handler_->ReportWarningBypass(user_justification);
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
    page_print_request_handler_->ReportWarningBypass(user_justification);
  }

  RunCallback();
}

void ContentAnalysisDelegate::Cancel(bool warning) {
  if (callback_.is_null()) {
    return;
  }

  // Don't report this upload as cancelled if the user didn't bypass the
  // warning.
  if (!warning) {
    safe_browsing::RecordDeepScanMetrics(
        data_.settings.cloud_or_local_settings.is_cloud_analysis(),
        access_point_, base::TimeTicks::Now() - upload_start_time_, 0,
        "CancelledByUser", false);
  }

  // Ask the binary upload service to cancel requests if it can.
  auto cancel = std::make_unique<BinaryUploadService::CancelRequests>(
      data_.settings.cloud_or_local_settings);
  cancel->set_user_action_id(user_action_id_);

  BinaryUploadService* upload_service = GetBinaryUploadService();
  if (upload_service) {
    upload_service->MaybeCancelRequests(std::move(cancel));
  }

  // Make sure to reject everything.
  FillAllResultsWith(false);
  RunCallback();
}

std::optional<std::u16string> ContentAnalysisDelegate::GetCustomMessage()
    const {
  // Rule-based custom messages take precedence over policy-based.
  std::u16string custom_rule_message =
      GetCustomRuleString(custom_rule_message_);
  if (!custom_rule_message.empty()) {
    return l10n_util::GetStringFUTF16(IDS_DEEP_SCANNING_DIALOG_CUSTOM_MESSAGE,
                                      custom_rule_message);
  }

  auto element = data_.settings.tags.find(final_result_tag_);
  if (element != data_.settings.tags.end() &&
      !element->second.custom_message.message.empty()) {
    return l10n_util::GetStringFUTF16(IDS_DEEP_SCANNING_DIALOG_CUSTOM_MESSAGE,
                                      element->second.custom_message.message);
  }

  return std::nullopt;
}

std::optional<GURL> ContentAnalysisDelegate::GetCustomLearnMoreUrl() const {
  // Rule-based custom messages which don't have learn more urls take
  // precedence over policy-based.
  if (custom_rule_message_.message_segments().empty()) {
    auto element = data_.settings.tags.find(final_result_tag_);
    if (element != data_.settings.tags.end() &&
        element->second.custom_message.learn_more_url.is_valid() &&
        !element->second.custom_message.learn_more_url.is_empty()) {
      return element->second.custom_message.learn_more_url;
    }
  }

  return std::nullopt;
}

std::optional<std::vector<std::pair<gfx::Range, GURL>>>
ContentAnalysisDelegate::GetCustomRuleMessageRanges() const {
  size_t offset;
  l10n_util::GetStringFUTF16(IDS_DEEP_SCANNING_DIALOG_CUSTOM_MESSAGE,
                             std::u16string{}, &offset);
  std::vector<std::pair<gfx::Range, GURL>> custom_rule_message_ranges =
      GetCustomRuleStyles(custom_rule_message_, offset);
  if (!custom_rule_message_ranges.empty()) {
    return custom_rule_message_ranges;
  }
  return std::nullopt;
}

bool ContentAnalysisDelegate::BypassRequiresJustification() const {
  return data_.settings.tags.count(final_result_tag_) &&
         data_.settings.tags.at(final_result_tag_).requires_justification;
}

std::u16string ContentAnalysisDelegate::GetBypassJustificationLabel() const {
  int id;
  switch (access_point_) {
    case DeepScanAccessPoint::UPLOAD:
    case DeepScanAccessPoint::DRAG_AND_DROP:
    case DeepScanAccessPoint::FILE_TRANSFER:
      id = IDS_DEEP_SCANNING_DIALOG_UPLOAD_BYPASS_JUSTIFICATION_LABEL;
      break;
    case DeepScanAccessPoint::DOWNLOAD:
      id = IDS_DEEP_SCANNING_DIALOG_DOWNLOAD_BYPASS_JUSTIFICATION_LABEL;
      break;
    case DeepScanAccessPoint::PASTE:
      id = IDS_DEEP_SCANNING_DIALOG_PASTE_BYPASS_JUSTIFICATION_LABEL;
      break;
    case DeepScanAccessPoint::PRINT:
      id = IDS_DEEP_SCANNING_DIALOG_PRINT_BYPASS_JUSTIFICATION_LABEL;
      break;
  }
  return l10n_util::GetStringUTF16(id);
}

std::optional<std::u16string>
ContentAnalysisDelegate::OverrideCancelButtonText() const {
  return std::nullopt;
}

// static
bool ContentAnalysisDelegate::IsEnabled(Profile* profile,
                                        GURL url,
                                        Data* data,
                                        AnalysisConnector connector) {
  auto* service = ConnectorsServiceFactory::GetForBrowserContext(profile);
  // If the corresponding Connector policy isn't set, don't perform scans.
  if (!service || !service->IsConnectorEnabled(connector)) {
    return false;
  }

  // Check that |url| matches the appropriate URL patterns by getting settings.
  // No settings means no matches were found.
  auto settings = service->GetAnalysisSettings(url, connector);
  if (!settings.has_value()) {
    return false;
  }

  data->settings = std::move(settings.value());
  if (url.is_valid()) {
    data->url = url;
  }

  return true;
}

// static
void ContentAnalysisDelegate::CreateForWebContents(
    content::WebContents* web_contents,
    Data data,
    CompletionCallback callback,
    DeepScanAccessPoint access_point) {
  Factory* testing_factory = GetFactoryStorage();
  bool wait_for_verdict =
      data.settings.block_until_verdict == BlockUntilVerdict::kBlock;
  bool should_allow_by_default =
      data.settings.default_action == DefaultAction::kAllow;
  DVLOG(1) << __func__
           << ": should_allow_by_default=" << should_allow_by_default;

  // Using new instead of std::make_unique<> to access non public constructor.
  auto delegate = testing_factory->is_null()
                      ? base::WrapUnique(new ContentAnalysisDelegate(
                            web_contents, std::move(data), std::move(callback),
                            access_point))
                      : testing_factory->Run(web_contents, std::move(data),
                                             std::move(callback));

  UploadDataStatus upload_data_status = delegate->UploadData();

  // Only show UI if one of the two conditions is met:
  // 1. work is ongoing in the background and that the user must wait for a
  // verdict.
  // 2. work is done and fail-closed conditions are met.
  bool show_in_progress_ui =
      upload_data_status == UploadDataStatus::kInProgress && wait_for_verdict &&
      (*UIEnabledStorage());
  bool show_fail_closed_ui =
      delegate->IsFailClosed(upload_data_status, should_allow_by_default) &&
      (*UIEnabledStorage());

  DVLOG(1) << __func__ << ": show_fail_closed_ui=" << show_fail_closed_ui;

  // If the UI is enabled, create the modal dialog.
  if (show_in_progress_ui || show_fail_closed_ui) {
    ContentAnalysisDelegate* delegate_ptr = delegate.get();
    int files_count = delegate_ptr->data_.paths.size();

    // Update the result early if fail-closed is determined, otherwise set it to
    // the default state.
    FinalContentAnalysisResult result =
        show_fail_closed_ui ? FinalContentAnalysisResult::FAIL_CLOSED
                            : FinalContentAnalysisResult::SUCCESS;

    // This dialog is owned by the constrained_window code.
    content::WebContents* top_web_contents =
        guest_view::GuestViewBase::GetTopLevelWebContents(
            web_contents->GetResponsibleWebContents());
    delegate_ptr->dialog_ = new ContentAnalysisDialogController(
        std::move(delegate),
        delegate_ptr->data_.settings.cloud_or_local_settings
            .is_cloud_analysis(),
        top_web_contents, access_point, files_count, result);
    return;
  }

  // If local client cannot be found, fail open on all the OS except on Windows
  // (available integration should be installed).
  if (upload_data_status == UploadDataStatus::kNoLocalClientFound) {
    bool should_fail_open =
        delegate->ShouldFailOpenWithoutLocalClient(should_allow_by_default);
    DVLOG(1) << __func__ << ": no local client found, should_fail_open="
             << should_fail_open;
    delegate->FillAllResultsWith(should_fail_open);
    delegate->RunCallback();
  }

  if (!wait_for_verdict || upload_data_status == UploadDataStatus::kComplete) {
    // The UI will not be shown but the policy is set to not wait for the
    // verdict, or no scans need to be performed.  Inform the caller that
    // they may proceed.
    //
    // Supporting "wait for verdict" while not showing a UI makes writing
    // tests for callers of this code easier.
    DCHECK(delegate->final_result_ != FinalContentAnalysisResult::FAIL_CLOSED);
    delegate->FillAllResultsWith(true);
    delegate->RunCallback();
  }

  // If all requests are already done, just let `delegate` go out of scope.
  if (delegate->all_work_done_) {
    return;
  }

  // ... otherwise, let the last response from the upload service callback
  // delete the delegate when there is no more work.
  if (upload_data_status == UploadDataStatus::kInProgress) {
    delegate.release();
  }
}

// static
void ContentAnalysisDelegate::CreateForFilesInWebContents(
    content::WebContents* web_contents,
    Data data,
    ForFilesCompletionCallback callback,
    DeepScanAccessPoint access_point) {
  DCHECK(data.text.empty());
  DCHECK(data.image.empty());
  DCHECK(!data.page.IsValid());

  auto files_scan_data = std::make_unique<FilesScanData>(std::move(data.paths));
  auto* files_scan_data_ptr = files_scan_data.get();
  files_scan_data_ptr->ExpandPaths(base::BindOnce(
      &OnPathsExpanded, web_contents->GetWeakPtr(), access_point,
      std::move(data), std::move(files_scan_data), std::move(callback)));
}

// static
void ContentAnalysisDelegate::SetFactoryForTesting(Factory factory) {
  *GetFactoryStorage() = factory;
}

// static
void ContentAnalysisDelegate::ResetFactoryForTesting() {
  if (GetFactoryStorage()) {
    GetFactoryStorage()->Reset();
  }
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

void ContentAnalysisDelegate::SetPageWarningForTesting() {
  page_warning_ = true;
}

ContentAnalysisDelegate::ContentAnalysisDelegate(
    content::WebContents* web_contents,
    Data data,
    CompletionCallback callback,
    DeepScanAccessPoint access_point)
    : data_(std::move(data)),
      callback_(std::move(callback)),
      access_point_(access_point),
      web_contents_(web_contents->GetWeakPtr()) {
  CHECK(web_contents);
  profile_ = Profile::FromBrowserContext(web_contents->GetBrowserContext());
  url_ = web_contents->GetLastCommittedURL();
  frame_url_chain_ = CollectFrameUrls(web_contents, access_point_);
  title_ = base::UTF16ToUTF8(web_contents->GetTitle());
  user_action_id_ = base::HexEncode(base::RandBytesAsVector(128));
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

void ContentAnalysisDelegate::TextRequestCallback(RequestHandlerResult result) {
  DCHECK(text_request_handler_);

  text_request_result_ = std::move(result);

  text_request_handler_->AppendFinalActionsTo(&final_actions_);

  DVLOG(1) << __func__ << ": string result=" << text_request_result_.complies;
  std::fill(result_.text_results.begin(), result_.text_results.end(),
            text_request_result_.complies);

  UpdateFinalResult(text_request_result_.final_result, text_request_result_.tag,
                    text_request_result_.custom_rule_message);

  text_request_complete_ = true;

  MaybeCompleteScanRequest();
}

void ContentAnalysisDelegate::ImageRequestCallback(
    RequestHandlerResult result) {
  DCHECK(image_request_handler_);

  image_request_result_ = std::move(result);

  image_request_handler_->AppendFinalActionsTo(&final_actions_);

  DVLOG(1) << __func__ << ": image result=" << image_request_result_.complies;
  result_.image_result = image_request_result_.complies;

  UpdateFinalResult(image_request_result_.final_result,
                    image_request_result_.tag,
                    image_request_result_.custom_rule_message);

  image_request_complete_ = true;

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
    DVLOG(1) << __func__ << ": file index=" << index
             << " result=" << results[index].complies;
    if (result == FinalContentAnalysisResult::WARNING) {
      warned_file_indices_.push_back(index);
    }
    UpdateFinalResult(result, results[index].tag,
                      results[index].custom_rule_message);
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
  if (!dialog_) {
    return false;
  }

  dialog_->ShowResult(final_result_);
  return true;
}

bool ContentAnalysisDelegate::CancelDialog() {
  if (!dialog_) {
    return false;
  }

  dialog_->CancelDialogAndDelete();
  return true;
}

void ContentAnalysisDelegate::PageRequestCallback(RequestHandlerResult result) {
  DCHECK(page_print_request_handler_);

  page_print_request_result_ = std::move(result);

  page_print_request_handler_->AppendFinalActionsTo(&final_actions_);

  result_.page_result = page_print_request_result_.complies;

  UpdateFinalResult(page_print_request_result_.final_result,
                    page_print_request_result_.tag,
                    page_print_request_result_.custom_rule_message);

  page_warning_ = page_print_request_result_.final_result ==
                  FinalContentAnalysisResult::WARNING;

  page_request_complete_ = true;

  MaybeCompleteScanRequest();
}

ContentAnalysisDelegate::UploadDataStatus
ContentAnalysisDelegate::UploadData() {
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
      return UploadDataStatus::kNoLocalClientFound;
    }
  }
#endif

  DVLOG(1) << __func__ << ": prepare requests for analysis";

  // Create a text request, an image request, a page request and a file request
  // for each file.
  PrepareTextRequest();
  PrepareImageRequest();
  PreparePageRequest();

  if (!data_.paths.empty()) {
    // Passing the settings using a reference is safe here, because
    // MultiFileRequestHandler is owned by this class.
    files_request_handler_ = FilesRequestHandler::Create(
        this, GetBinaryUploadService(), profile_, url_, "", "",
        GetContentTransferMethod(), access_point_, data_.paths,
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

  return text_request_complete_ && image_request_complete_ &&
                 files_request_complete_ && page_request_complete_
             ? UploadDataStatus::kComplete
             : UploadDataStatus::kInProgress;
}

bool ContentAnalysisDelegate::IsFailClosed(UploadDataStatus upload_data_status,
                                           bool should_allow_by_default) {
  // Fail-closed can be triggered in two cases:
  //   1. The final scan result is already updated to fail-closed (when LBUS or
  //   CBUS cannot upload data and exceed max retry).
  //   2. LCAC cannot connect to the local agent on Windows.
  return final_result_ == FinalContentAnalysisResult::FAIL_CLOSED ||
         (upload_data_status == UploadDataStatus::kNoLocalClientFound &&
          !ShouldFailOpenWithoutLocalClient(should_allow_by_default));
}

bool ContentAnalysisDelegate::ShouldFailOpenWithoutLocalClient(
    bool should_allow_by_default) {
// Fail-closed settings should only be applied to Windows, otherwise it should
// fail open.
#if BUILDFLAG(IS_WIN)
  return should_allow_by_default;
#else
  return true;
#endif
}

void ContentAnalysisDelegate::PrepareTextRequest() {
  std::string full_text;
  for (const std::string& text : data_.text) {
    full_text.append(text);
  }

  text_request_complete_ = !text_request_required();

  if (!full_text.empty()) {
    base::UmaHistogramCustomCounts("Enterprise.OnBulkDataEntry.DataSize",
                                   full_text.size(),
                                   /*min=*/1,
                                   /*max=*/51 * 1024 * 1024,
                                   /*buckets=*/50);
  }

  if (text_request_complete_) {
    // When no text scan is required, mark `result_.text_results` as true so
    // caller code doesn't interpret text data as being blocked. Note that the
    // paste might still be blocked if the same paste action has its image
    // request blocked.
    std::fill(result_.text_results.begin(), result_.text_results.end(), true);
  } else {
    text_request_handler_ = ClipboardRequestHandler::Create(
        this, GetBinaryUploadService(), profile_, url_,
        ClipboardRequestHandler::Type::kText, access_point_,
        data_.clipboard_source, data_.source_content_area_email,
        GetContentTransferMethod(), std::move(full_text),
        base::BindOnce(&ContentAnalysisDelegate::TextRequestCallback,
                       weak_ptr_factory_.GetWeakPtr()));

    text_request_handler_->UploadData();
  }
}

bool ContentAnalysisDelegate::ShouldNotUploadLargePage(size_t page_size) {
  return data_.settings.cloud_or_local_settings.is_cloud_analysis() &&
         page_size > BinaryUploadService::kMaxUploadSizeBytes &&
         data_.settings.block_large_files;
}

void ContentAnalysisDelegate::PrepareImageRequest() {
  image_request_complete_ = !image_request_required();

  if (!data_.image.empty()) {
    base::UmaHistogramCustomCounts("Enterprise.OnBulkDataEntry.DataSize",
                                   data_.image.size(),
                                   /*min=*/1,
                                   /*max=*/51 * 1024 * 1024,
                                   /*buckets=*/50);
  }

  if (image_request_complete_) {
    // When no image scan is required, mark `result_.image_result` as true so
    // caller code doesn't interpret the image as being blocked. Note that the
    // paste might still be blocked if the same paste action has its text
    // request blocked.
    result_.image_result = true;
  } else {
    image_request_handler_ = ClipboardRequestHandler::Create(
        this, GetBinaryUploadService(), profile_, url_,
        ClipboardRequestHandler::Type::kImage, access_point_,
        data_.clipboard_source, data_.source_content_area_email,
        GetContentTransferMethod(), data_.image,
        base::BindOnce(&ContentAnalysisDelegate::ImageRequestCallback,
                       weak_ptr_factory_.GetWeakPtr()));

    image_request_handler_->UploadData();
  }
}

void ContentAnalysisDelegate::PreparePageRequest() {
  // The request is considered complete if the mapped region is invalid since it
  // prevents scanning.
  page_request_complete_ = !data_.page.IsValid();

  if (page_request_complete_) {
    // When no print scan is required, mark `result_.page_result` as true so
    // caller code doesn't interpret the print action as being blocked.
    result_.page_result = true;
  } else {
    page_print_request_handler_ = PagePrintRequestHandler::Create(
        this, GetBinaryUploadService(), profile_, url_, data_.printer_name,
        page_content_type_, std::move(data_.page),
        base::BindOnce(&ContentAnalysisDelegate::PageRequestCallback,
                       weak_ptr_factory_.GetWeakPtr()));
    page_print_request_handler_->UploadData();
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

safe_browsing::SafeBrowsingNavigationObserverManager*
ContentAnalysisDelegate::GetNavigationObserverManager() const {
  return safe_browsing::SafeBrowsingNavigationObserverManagerFactory::
      GetForBrowserContext(profile_);
}

bool ContentAnalysisDelegate::UpdateDialog() {
  // In the case of fail-closed, show the final result UI regardless of cloud or
  // local analysis. Otherwise, only show the result for cloud analysis.
  bool show_ui = final_result_ == FinalContentAnalysisResult::FAIL_CLOSED ||
                 data_.settings.cloud_or_local_settings.is_cloud_analysis();

  DVLOG(1) << __func__ << ": show_ui=" << show_ui;
  return show_ui ? ShowFinalResultInDialog() : CancelDialog();
}

void ContentAnalysisDelegate::MaybeCompleteScanRequest() {
  if (!text_request_complete_ || !image_request_complete_ ||
      !files_request_complete_ || !page_request_complete_) {
    DVLOG(1) << __func__ << ": scan request is incomplete.";
    return;
  }

  // If showing the warning message, wait before running the callback. The
  // callback will be called either in BypassWarnings or Cancel.
  if (final_result_ != FinalContentAnalysisResult::WARNING) {
    DVLOG(1) << __func__ << ": calling RunCallback()";
    RunCallback();
  }

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
    DVLOG(1) << __func__ << ": about to delete `this` to clean up.";
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
  if (!text_request_result_.request_token.empty() &&
      !image_request_result_.request_token.empty()) {
    if (!result_.image_result) {
      final_actions_[text_request_result_.request_token] =
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
    const std::string& tag,
    const ContentAnalysisResponse::Result::TriggeredRule::CustomRuleMessage&
        custom_rule_message) {
  if (result < final_result_) {
    final_result_ = result;
    final_result_tag_ = tag;
    custom_rule_message_ = custom_rule_message;
  }
}

void ContentAnalysisDelegate::AckAllRequests() {
  if (!OnAckAllRequestsStorage()->is_null()) {
    std::move(*OnAckAllRequestsStorage()).Run(final_actions_);
  }

  BinaryUploadService* upload_service = GetBinaryUploadService();
  if (!upload_service) {
    return;
  }

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

std::string ContentAnalysisDelegate::GetContentTransferMethod() const {
  switch (data_.reason) {
    case enterprise_connectors::ContentAnalysisRequest::UNKNOWN:
    case enterprise_connectors::ContentAnalysisRequest::PRINT_PREVIEW_PRINT:
    case enterprise_connectors::ContentAnalysisRequest::SYSTEM_DIALOG_PRINT:
    case enterprise_connectors::ContentAnalysisRequest::NORMAL_DOWNLOAD:
    case enterprise_connectors::ContentAnalysisRequest::SAVE_AS_DOWNLOAD:
      return "";

    case enterprise_connectors::ContentAnalysisRequest::CLIPBOARD_PASTE:
      if (!data_.paths.empty()) {
        return kContentTransferMethodFilePaste;
      }
      break;
    case enterprise_connectors::ContentAnalysisRequest::DRAG_AND_DROP:
      return kContentTransferMethodDragAndDrop;
    case enterprise_connectors::ContentAnalysisRequest::FILE_PICKER_DIALOG:
      return kContentTransferMethodFilePicker;
  }

  return "";
}

bool ContentAnalysisDelegate::text_request_required() const {
  size_t total = 0;
  for (const std::string& text : data_.text) {
    total += text.size();
  }

  return total != 0 && total >= data_.settings.minimum_data_size;
}

bool ContentAnalysisDelegate::image_request_required() const {
  if (data_.settings.cloud_or_local_settings.is_local_analysis() ||
      base::FeatureList::IsEnabled(
          enterprise_connectors::kDlpScanPastedImages)) {
    return !data_.image.empty() &&
           data_.image.size() <=
               data_.settings.cloud_or_local_settings.max_file_size();
  }

  return false;
}

const AnalysisSettings& ContentAnalysisDelegate::settings() const {
  return data_.settings;
}

signin::IdentityManager* ContentAnalysisDelegate::identity_manager() const {
  return IdentityManagerFactory::GetForProfile(profile_);
}

int ContentAnalysisDelegate::user_action_requests_count() const {
  int count = data_.paths.size();
  if (data_.page.IsValid()) {
    ++count;
  }
  if (image_request_required()) {
    ++count;
  }
  if (text_request_required()) {
    ++count;
  }
  return count;
}

std::string ContentAnalysisDelegate::tab_title() const {
  return title_;
}

std::string ContentAnalysisDelegate::user_action_id() const {
  return user_action_id_;
}

std::string ContentAnalysisDelegate::email() const {
  return GetProfileEmail(profile_);
}

const GURL& ContentAnalysisDelegate::url() const {
  return url_;
}

const GURL& ContentAnalysisDelegate::tab_url() const {
  return url_;
}

ContentAnalysisRequest::Reason ContentAnalysisDelegate::reason() const {
  return data_.reason;
}

google::protobuf::RepeatedPtrField<safe_browsing::ReferrerChainEntry>
ContentAnalysisDelegate::referrer_chain() const {
  if (!web_contents_) {
    return {};
  }
  return GetReferrerChain(url_, *web_contents_);
}

google::protobuf::RepeatedPtrField<std::string>
ContentAnalysisDelegate::frame_url_chain() const {
  return frame_url_chain_;
}

content::WebContents* ContentAnalysisDelegate::web_contents() const {
  return web_contents_.get();
}

}  // namespace enterprise_connectors
