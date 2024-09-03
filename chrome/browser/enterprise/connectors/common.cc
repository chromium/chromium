// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/common.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_downloads_delegate.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_features.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/user_manager/user.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/policy/core/common/policy_loader_lacros.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/enterprise/signin/enterprise_signin_prefs.h"
#include "components/prefs/pref_service.h"
#endif

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_dialog.h"

using safe_browsing::BinaryUploadService;
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace enterprise_connectors {

namespace {

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
bool ContentAnalysisActionAllowsDataUse(TriggeredRule::Action action) {
  switch (action) {
    case TriggeredRule::ACTION_UNSPECIFIED:
    case TriggeredRule::REPORT_ONLY:
      return true;
    case TriggeredRule::WARN:
    case TriggeredRule::BLOCK:
      return false;
  }
}

bool ShouldAllowDeepScanOnLargeOrEncryptedFiles(
    BinaryUploadService::Result result,
    bool block_large_files,
    bool block_password_protected_files) {
  return (result == BinaryUploadService::Result::FILE_TOO_LARGE &&
          !block_large_files) ||
         (result == BinaryUploadService::Result::FILE_ENCRYPTED &&
          !block_password_protected_files);
}
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
std::string EventResultToString(
    extensions::api::enterprise_reporting_private::EventResult event_result) {
  // Make sure the values returned by this function match the names in
  // google3/chrome/cros/reporting/api/proto/browser_events.proto
  if (event_result ==
      extensions::api::enterprise_reporting_private::EventResult::kNone) {
    return "EVENT_RESULT_UNKNOWN";
  }
  return ToString(event_result);
}

std::string DetectorTypeToString(
    extensions::api::enterprise_reporting_private::DetectorType detector_type) {
  // Make sure the values returned by this function match the names in
  // google3/chrome/cros/reporting/api/proto/browser_events.proto
  if (detector_type ==
      extensions::api::enterprise_reporting_private::DetectorType::kNone) {
    return "DETECTOR_TYPE_UNSPECIFIED";
  }
  return ToString(detector_type);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
bool ResultShouldAllowDataUse(const AnalysisSettings& settings,
                              BinaryUploadService::Result upload_result) {
  bool default_action_allow_data_use =
      settings.default_action == DefaultAction::kAllow;

  // Keep this implemented as a switch instead of a simpler if statement so that
  // new values added to BinaryUploadService::Result cause a compiler error.
  switch (upload_result) {
    case BinaryUploadService::Result::SUCCESS:
    // UNAUTHORIZED allows data usage since it's a result only obtained if the
    // browser is not authorized to perform deep scanning. It does not make
    // sense to block data in this situation since no actual scanning of the
    // data was performed, so it's allowed.
    case BinaryUploadService::Result::UNAUTHORIZED:
      return true;

    case BinaryUploadService::Result::UPLOAD_FAILURE:
    case BinaryUploadService::Result::TIMEOUT:
    case BinaryUploadService::Result::FAILED_TO_GET_TOKEN:
    case BinaryUploadService::Result::TOO_MANY_REQUESTS:
    case BinaryUploadService::Result::UNKNOWN:
    case BinaryUploadService::Result::INCOMPLETE_RESPONSE:
      DVLOG(1) << __func__
               << ": handled by fail-closed settings, "
                  "default_action_allow_data_use="
               << default_action_allow_data_use;
      return default_action_allow_data_use;

    case BinaryUploadService::Result::FILE_TOO_LARGE:
      return !settings.block_large_files;

    case BinaryUploadService::Result::FILE_ENCRYPTED:
      return !settings.block_password_protected_files;
  }
}

RequestHandlerResult CalculateRequestHandlerResult(
    const AnalysisSettings& settings,
    BinaryUploadService::Result upload_result,
    const ContentAnalysisResponse& response) {
  std::string tag;
  auto action = GetHighestPrecedenceAction(response, &tag);

  bool file_complies = ResultShouldAllowDataUse(settings, upload_result) &&
                       ContentAnalysisActionAllowsDataUse(action);

  RequestHandlerResult result;
  result.complies = file_complies;
  result.request_token = response.request_token();
  result.tag = tag;

  if (file_complies) {
    result.final_result = FinalContentAnalysisResult::SUCCESS;
    return result;
  }

  // If file is non-compliant, map it to the specific case.
  if (ResultIsFailClosed(upload_result)) {
    DVLOG(1) << __func__ << ": result mapped to fail-closed.";
    result.final_result = FinalContentAnalysisResult::FAIL_CLOSED;
  } else if (upload_result == BinaryUploadService::Result::FILE_TOO_LARGE) {
    result.final_result = FinalContentAnalysisResult::LARGE_FILES;
  } else if (upload_result == BinaryUploadService::Result::FILE_ENCRYPTED) {
    result.final_result = FinalContentAnalysisResult::ENCRYPTED_FILES;
  } else if (action == TriggeredRule::WARN) {
    result.final_result = FinalContentAnalysisResult::WARNING;
  } else {
    result.final_result = FinalContentAnalysisResult::FAILURE;
  }

  for (const auto& response_result : response.results()) {
    if (!response_result.has_status() ||
        response_result.status() != ContentAnalysisResponse::Result::SUCCESS) {
      continue;
    }
    for (const auto& rule : response_result.triggered_rules()) {
      // Ensures that lower precedence actions custom messages are skipped. The
      // message shown is arbitrary for rules with the same precedence.
      if (rule.action() == action && rule.has_custom_rule_message()) {
        result.custom_rule_message = rule.custom_rule_message();
      }
    }
  }
  return result;
}

safe_browsing::EventResult CalculateEventResult(
    const AnalysisSettings& settings,
    bool allowed_by_scan_result,
    bool should_warn) {
  bool wait_for_verdict =
      settings.block_until_verdict == BlockUntilVerdict::kBlock;
  return (allowed_by_scan_result || !wait_for_verdict)
             ? safe_browsing::EventResult::ALLOWED
             : (should_warn ? safe_browsing::EventResult::WARNED
                            : safe_browsing::EventResult::BLOCKED);
}
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

const char SavePackageScanningData::kKey[] =
    "enterprise_connectors.save_package_scanning_key";
SavePackageScanningData::SavePackageScanningData(
    content::SavePackageAllowedCallback callback)
    : callback(std::move(callback)) {}
SavePackageScanningData::~SavePackageScanningData() = default;

void RunSavePackageScanningCallback(download::DownloadItem* item,
                                    bool allowed) {
  DCHECK(item);

  auto* data = static_cast<SavePackageScanningData*>(
      item->GetUserData(SavePackageScanningData::kKey));
  if (data && !data->callback.is_null())
    std::move(data->callback).Run(allowed);
}

bool IncludeDeviceInfo(Profile* profile, bool per_profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  return user && user->IsAffiliated();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  return policy::PolicyLoaderLacros::IsMainUserAffiliated();
#else
  // A browser managed through the device can send device info.
  if (!per_profile) {
    return true;
  }

  // An unmanaged browser shouldn't share its device info for privacy reasons.
  if (!policy::GetDMToken(profile).is_valid()) {
    return false;
  }

  // A managed device can share its info with the profile if they are
  // affiliated.
  return enterprise_util::IsProfileAffiliated(profile);
#endif
}

bool ShouldPromptReviewForDownload(
    Profile* profile,
    const download::DownloadItem* download_item) {
  // Review dialog only appears if custom UI has been set by the admin or custom
  // rule message present in download item.
  if (!download_item) {
    return false;
  }
  download::DownloadDangerType danger_type = download_item->GetDangerType();
  if (danger_type == download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING ||
      danger_type == download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK) {
    return ConnectorsServiceFactory::GetForBrowserContext(profile)
               ->HasExtraUiToDisplay(AnalysisConnector::FILE_DOWNLOADED,
                                     kDlpTag) ||
           GetDownloadsCustomRuleMessage(download_item, danger_type);
  } else if (danger_type == download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE ||
             danger_type == download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL ||
             danger_type == download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT) {
    return ConnectorsServiceFactory::GetForBrowserContext(profile)
        ->HasExtraUiToDisplay(AnalysisConnector::FILE_DOWNLOADED, kMalwareTag);
  }
  return false;
}

std::string GetProfileEmail(Profile* profile) {
  if (!profile) {
    return std::string();
  }

  std::string email =
      GetProfileEmail(IdentityManagerFactory::GetForProfile(profile));

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  if (email.empty()) {
    email = profile->GetPrefs()->GetString(
        enterprise_signin::prefs::kProfileUserEmail);
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  return email;
}

std::string GetProfileEmail(signin::IdentityManager* identity_manager) {
  // If the profile is not signed in, GetPrimaryAccountInfo() returns an
  // empty account info.
  return identity_manager
             ? identity_manager
                   ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                   .email
             : std::string();
}

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
void ShowDownloadReviewDialog(const std::u16string& filename,
                              Profile* profile,
                              download::DownloadItem* download_item,
                              content::WebContents* web_contents,
                              base::OnceClosure keep_closure,
                              base::OnceClosure discard_closure) {
  auto state = FinalContentAnalysisResult::FAILURE;
  download::DownloadDangerType danger_type = download_item->GetDangerType();

  if (danger_type == download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING) {
    state = FinalContentAnalysisResult::WARNING;
  }

  const char* tag =
      (danger_type ==
                   download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_WARNING ||
               danger_type ==
                   download::DOWNLOAD_DANGER_TYPE_SENSITIVE_CONTENT_BLOCK
           ? kDlpTag
           : kMalwareTag);

  auto* connectors_service =
      ConnectorsServiceFactory::GetForBrowserContext(profile);

  std::u16string custom_message =
      connectors_service
          ->GetCustomMessage(AnalysisConnector::FILE_DOWNLOADED, tag)
          .value_or(u"");
  GURL learn_more_url =
      connectors_service
          ->GetLearnMoreUrl(AnalysisConnector::FILE_DOWNLOADED, tag)
          .value_or(GURL());

  bool bypass_justification_required =
      connectors_service->GetBypassJustificationRequired(
          AnalysisConnector::FILE_DOWNLOADED, tag);

  // This dialog opens itself, and is thereafter owned by constrained window
  // code.
  new ContentAnalysisDialog(
      std::make_unique<ContentAnalysisDownloadsDelegate>(
          filename, custom_message, learn_more_url,
          bypass_justification_required, std::move(keep_closure),
          std::move(discard_closure), download_item,
          GetDownloadsCustomRuleMessage(download_item, danger_type)
              .value_or(ContentAnalysisResponse::Result::TriggeredRule::
                            CustomRuleMessage())),
      true,  // Downloads are always cloud-based for now.
      web_contents, safe_browsing::DeepScanAccessPoint::DOWNLOAD,
      /* file_count */ 1, state, download_item);
}

bool IsResumableUpload(const BinaryUploadService::Request& request) {
  // Currently resumable upload doesn't support paste or LBUS. If one day we do,
  // we should update the logic here as well.
  return !safe_browsing::IsConsumerScanRequest(request) &&
         request.cloud_or_local_settings().is_cloud_analysis() &&
         request.content_analysis_request().analysis_connector() !=
             enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY &&
         IsResumableUploadEnabled();
}

bool CloudMultipartResultIsFailure(BinaryUploadService::Result result) {
  return result != BinaryUploadService::Result::SUCCESS;
}

bool CloudResumableResultIsFailure(BinaryUploadService::Result result,
                                   bool block_large_files,
                                   bool block_password_protected_files) {
  return result != BinaryUploadService::Result::SUCCESS &&
         !ShouldAllowDeepScanOnLargeOrEncryptedFiles(
             result, block_large_files, block_password_protected_files);
}

bool LocalResultIsFailure(BinaryUploadService::Result result) {
  return result != BinaryUploadService::Result::SUCCESS &&
         result != BinaryUploadService::Result::FILE_TOO_LARGE &&
         result != BinaryUploadService::Result::FILE_ENCRYPTED;
}

bool ResultIsFailClosed(BinaryUploadService::Result result) {
  return result == BinaryUploadService::Result::UPLOAD_FAILURE ||
         result == BinaryUploadService::Result::TIMEOUT ||
         result == BinaryUploadService::Result::FAILED_TO_GET_TOKEN ||
         result == BinaryUploadService::Result::TOO_MANY_REQUESTS ||
         result == BinaryUploadService::Result::UNKNOWN ||
         result == BinaryUploadService::Result::INCOMPLETE_RESPONSE;
}
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
Profile* GetMainProfileLacros() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return nullptr;
  auto profiles = g_browser_process->profile_manager()->GetLoadedProfiles();
  const auto main_it = base::ranges::find_if(profiles, &Profile::IsMainProfile);
  if (main_it == profiles.end())
    return nullptr;
  return *main_it;
}
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
void ReportDataMaskingEvent(
    content::BrowserContext* browser_context,
    extensions::api::enterprise_reporting_private::DataMaskingEvent
        data_masking_event) {
  CHECK(browser_context);

  auto* reporting_client =
      enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
          browser_context);
  std::optional<enterprise_connectors::ReportingSettings> settings =
      reporting_client->GetReportingSettings();
  if (!settings.has_value() ||
      !base::Contains(settings->enabled_event_names,
                      enterprise_connectors::kKeySensitiveDataEvent)) {
    return;
  }

  base::Value::Dict event;
  event.Set(extensions::SafeBrowsingPrivateEventRouter::kKeyUrl,
            data_masking_event.url);
  event.Set(extensions::SafeBrowsingPrivateEventRouter::kKeyTabUrl,
            std::move(data_masking_event.url));
  event.Set(extensions::SafeBrowsingPrivateEventRouter::kKeyEventResult,
            EventResultToString(data_masking_event.event_result));

  base::Value::List triggered_rule_info;
  triggered_rule_info.reserve(data_masking_event.triggered_rule_info.size());
  for (auto& rule : data_masking_event.triggered_rule_info) {
    base::Value::Dict triggered_rule;
    triggered_rule.Set(
        extensions::SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleId,
        std::move(rule.rule_id));
    triggered_rule.Set(
        extensions::SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleName,
        std::move(rule.rule_name));

    base::Value::List matched_detectors;
    for (auto& detector : rule.matched_detectors) {
      base::Value::Dict detector_value;
      detector_value.Set(kKeyDetectorId, std::move(detector.detector_id));
      detector_value.Set(kKeyDisplayName, std::move(detector.display_name));
      detector_value.Set(kKeyDetectorType,
                         DetectorTypeToString(detector.detector_type));
      matched_detectors.Append(std::move(detector_value));
    }
    triggered_rule.Set(kKeyMatchedDetectors, std::move(matched_detectors));

    triggered_rule_info.Append(std::move(triggered_rule));
  }
  event.Set(extensions::SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleInfo,
            std::move(triggered_rule_info));

  reporting_client->ReportRealtimeEvent(
      enterprise_connectors::kKeySensitiveDataEvent,
      std::move(settings.value()), std::move(event));
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace enterprise_connectors
