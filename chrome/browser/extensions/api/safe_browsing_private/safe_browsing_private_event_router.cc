// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/json/values_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/common/extensions/api/safe_browsing_private.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/enterprise/connectors/core/reporting_service_settings.h"
#include "components/enterprise/connectors/core/reporting_utils.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/url_matcher/url_matcher.h"
#include "components/url_matcher/url_util.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#else
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#endif

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#endif

#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
#include "base/containers/contains.h"
#endif  // BUILDFLAG(ENTERPRISE_DATA_CONTROLS)

namespace extensions {

namespace {

std::string MalwareRuleToThreatType(const std::string& rule_name) {
  if (rule_name == "uws") {
    return "POTENTIALLY_UNWANTED";
  } else if (rule_name == "malware") {
    return "DANGEROUS";
  } else {
    return "UNKNOWN";
  }
}

std::string DangerTypeToThreatType(download::DownloadDangerType danger_type) {
  switch (danger_type) {
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE:
      return "DANGEROUS_FILE_TYPE";
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL:
      return "DANGEROUS_URL";
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
      return "DANGEROUS";
    case download::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT:
      return "UNCOMMON";
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST:
      return "DANGEROUS_HOST";
    case download::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED:
      return "POTENTIALLY_UNWANTED";
    case download::DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE:
      return "DANGEROUS_ACCOUNT_COMPROMISE";
    default:
      // This can be reached when reporting an opened download that doesn't have
      // a verdict yet.
      return "UNKNOWN";
  }
}

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
void AddAnalysisConnectorVerdictToEvent(
    const enterprise_connectors::ContentAnalysisResponse::Result& result,
    base::Value::Dict& event) {
  base::Value::List triggered_rule_info;
  for (const enterprise_connectors::TriggeredRule& trigger :
       result.triggered_rules()) {
    base::Value::Dict triggered_rule;
    triggered_rule.Set(SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleName,
                       trigger.rule_name());
    int rule_id_int = 0;
    if (base::StringToInt(trigger.rule_id(), &rule_id_int)) {
      triggered_rule.Set(SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleId,
                         rule_id_int);
    }
    triggered_rule.Set(SafeBrowsingPrivateEventRouter::kKeyUrlCategory,
                       trigger.url_category());

    triggered_rule_info.Append(std::move(triggered_rule));
  }
  event.Set(SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleInfo,
            std::move(triggered_rule_info));
}
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)

}  // namespace

// Key names used with when building the dictionary to pass to the real-time
// reporting API.
const char SafeBrowsingPrivateEventRouter::kKeyUrl[] = "url";
const char SafeBrowsingPrivateEventRouter::kKeySource[] = "source";
const char SafeBrowsingPrivateEventRouter::kKeyDestination[] = "destination";
const char SafeBrowsingPrivateEventRouter::kKeyUserName[] = "userName";
const char SafeBrowsingPrivateEventRouter::kKeyIsPhishingUrl[] =
    "isPhishingUrl";
const char SafeBrowsingPrivateEventRouter::kKeyProfileUserName[] =
    "profileUserName";
const char SafeBrowsingPrivateEventRouter::kKeyFileName[] = "fileName";
const char SafeBrowsingPrivateEventRouter::kKeyDownloadDigestSha256[] =
    "downloadDigestSha256";
const char SafeBrowsingPrivateEventRouter::kKeyReason[] = "reason";
const char SafeBrowsingPrivateEventRouter::kKeyNetErrorCode[] = "netErrorCode";
const char SafeBrowsingPrivateEventRouter::kKeyClickedThrough[] =
    "clickedThrough";
const char SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleName[] = "ruleName";
const char SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleId[] = "ruleId";
const char SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleInfo[] =
    "triggeredRuleInfo";
const char SafeBrowsingPrivateEventRouter::kKeyThreatType[] = "threatType";
const char SafeBrowsingPrivateEventRouter::kKeyContentType[] = "contentType";
const char SafeBrowsingPrivateEventRouter::kKeyContentSize[] = "contentSize";
const char SafeBrowsingPrivateEventRouter::kKeyTrigger[] = "trigger";
const char SafeBrowsingPrivateEventRouter::kKeyReferrers[] = "referrers";
const char SafeBrowsingPrivateEventRouter::kKeyEventResult[] = "eventResult";
const char SafeBrowsingPrivateEventRouter::kKeyScanId[] = "scanId";
const char SafeBrowsingPrivateEventRouter::kKeyIsFederated[] = "isFederated";
const char SafeBrowsingPrivateEventRouter::kKeyFederatedOrigin[] =
    "federatedOrigin";
const char SafeBrowsingPrivateEventRouter::kKeyLoginUserName[] =
    "loginUserName";
const char SafeBrowsingPrivateEventRouter::kKeyUserJustification[] =
    "userJustification";
const char SafeBrowsingPrivateEventRouter::kKeyUrlCategory[] = "urlCategory";
const char SafeBrowsingPrivateEventRouter::kKeyAction[] = "action";
const char SafeBrowsingPrivateEventRouter::kKeyUnscannedReason[] =
    "unscannedReason";
const char SafeBrowsingPrivateEventRouter::kKeyTabUrl[] = "tabUrl";

const char SafeBrowsingPrivateEventRouter::kTriggerFileDownload[] =
    "FILE_DOWNLOAD";
const char SafeBrowsingPrivateEventRouter::kTriggerFileUpload[] = "FILE_UPLOAD";
const char SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload[] =
    "WEB_CONTENT_UPLOAD";
const char SafeBrowsingPrivateEventRouter::kTriggerPagePrint[] = "PAGE_PRINT";
const char SafeBrowsingPrivateEventRouter::kTriggerFileTransfer[] =
    "FILE_TRANSFER";
const char SafeBrowsingPrivateEventRouter::kTriggerClipboardCopy[] =
    "CLIPBOARD_COPY";

SafeBrowsingPrivateEventRouter::SafeBrowsingPrivateEventRouter(
    content::BrowserContext* context)
    : context_(context) {
  event_router_ = EventRouter::Get(context_);

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
  reporting_client_ =
      enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
          context);
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
}

SafeBrowsingPrivateEventRouter::~SafeBrowsingPrivateEventRouter() = default;

// static
std::string SafeBrowsingPrivateEventRouter::GetFileName(
    const std::string& filename,
    const bool include_full_path) {
  base::FilePath::StringType os_filename;
#if BUILDFLAG(IS_WIN)
  os_filename = base::UTF8ToWide(filename);
#else
  os_filename = filename;
#endif

  return include_full_path
             ? filename
             : base::FilePath(os_filename).BaseName().AsUTF8Unsafe();
}

void SafeBrowsingPrivateEventRouter::OnPolicySpecifiedPasswordReuseDetected(
    const GURL& url,
    const std::string& user_name,
    bool is_phishing_url,
    bool warning_shown) {
  api::safe_browsing_private::PolicySpecifiedPasswordReuse params;
  params.url = url.spec();
  params.user_name = user_name;
  params.is_phishing_url = is_phishing_url;

  // |event_router_| can be null in tests.
  if (event_router_) {
    base::Value::List event_value;
    event_value.Append(params.ToValue());

    auto extension_event = std::make_unique<Event>(
        events::
            SAFE_BROWSING_PRIVATE_ON_POLICY_SPECIFIED_PASSWORD_REUSE_DETECTED,
        api::safe_browsing_private::OnPolicySpecifiedPasswordReuseDetected::
            kEventName,
        std::move(event_value));
    event_router_->BroadcastEvent(std::move(extension_event));
  }
}

void SafeBrowsingPrivateEventRouter::OnPolicySpecifiedPasswordChanged(
    const std::string& user_name) {
  // |event_router_| can be null in tests.
  if (event_router_) {
    base::Value::List event_value;
    event_value.Append(user_name);
    auto extension_event = std::make_unique<Event>(
        events::SAFE_BROWSING_PRIVATE_ON_POLICY_SPECIFIED_PASSWORD_CHANGED,
        api::safe_browsing_private::OnPolicySpecifiedPasswordChanged::
            kEventName,
        std::move(event_value));
    event_router_->BroadcastEvent(std::move(extension_event));
  }
}

void SafeBrowsingPrivateEventRouter::OnDangerousDownloadOpened(
    const GURL& url,
    const GURL& tab_url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& scan_id,
    const download::DownloadDangerType danger_type,
    const int64_t content_size,
    const safe_browsing::ReferrerChain& referrer_chain) {
  api::safe_browsing_private::DangerousDownloadInfo params;
  params.url = url.spec();
  params.file_name = file_name;
  params.download_digest_sha256 = download_digest_sha256;
  params.user_name = reporting_client_->GetProfileUserName();

  // |event_router_| can be null in tests.
  if (event_router_) {
    base::Value::List event_value;
    event_value.Append(params.ToValue());

    auto extension_event = std::make_unique<Event>(
        events::SAFE_BROWSING_PRIVATE_ON_DANGEROUS_DOWNLOAD_OPENED,
        api::safe_browsing_private::OnDangerousDownloadOpened::kEventName,
        std::move(event_value));
    event_router_->BroadcastEvent(std::move(extension_event));
  }

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
  std::optional<enterprise_connectors::ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(
          enterprise_connectors::kKeyDangerousDownloadEvent) == 0) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUrl, params.url);
  event.Set(kKeyTabUrl, tab_url.spec());
  event.Set(kKeyFileName,
            GetFileName(file_name, enterprise_connectors::IncludeDeviceInfo(
                                       Profile::FromBrowserContext(context_),
                                       settings->per_profile)));
  event.Set(kKeyDownloadDigestSha256, params.download_digest_sha256);
  event.Set(kKeyContentType, mime_type);
  // |content_size| can be set to -1 to indicate an unknown size, in
  // which case the field is not set.
  if (content_size >= 0) {
    event.Set(kKeyContentSize, base::Int64ToValue(content_size));
  }
  event.Set(kKeyTrigger, kTriggerFileDownload);
  event.Set(kKeyEventResult, enterprise_connectors::EventResultToString(
                                 enterprise_connectors::EventResult::BYPASSED));
  event.Set(kKeyClickedThrough, true);
  event.Set(kKeyThreatType, DangerTypeToThreatType(danger_type));
  // The scan ID can be empty when the reported dangerous download is from a
  // Safe Browsing verdict.
  if (!scan_id.empty()) {
    event.Set(kKeyScanId, scan_id);
  }

  if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedFieldsForSecOps)) {
    enterprise_connectors::AddReferrerChainToEvent(referrer_chain, event);
  }

  reporting_client_->ReportRealtimeEvent(
      enterprise_connectors::kKeyDangerousDownloadEvent,
      std::move(settings.value()), std::move(event));
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
}

void SafeBrowsingPrivateEventRouter::OnSecurityInterstitialShown(
    const GURL& url,
    const std::string& reason,
    int net_error_code) {
  api::safe_browsing_private::InterstitialInfo params;
  params.url = url.spec();
  params.reason = reason;
  if (net_error_code < 0) {
    params.net_error_code = base::NumberToString(net_error_code);
  }
  params.user_name = reporting_client_->GetProfileUserName();

  // |event_router_| can be null in tests.
  if (event_router_) {
    base::Value::List event_value;
    event_value.Append(params.ToValue());

    auto extension_event = std::make_unique<Event>(
        events::SAFE_BROWSING_PRIVATE_ON_SECURITY_INTERSTITIAL_SHOWN,
        api::safe_browsing_private::OnSecurityInterstitialShown::kEventName,
        std::move(event_value));
    event_router_->BroadcastEvent(std::move(extension_event));
  }
}

void SafeBrowsingPrivateEventRouter::OnSecurityInterstitialProceeded(
    const GURL& url,
    const std::string& reason,
    int net_error_code) {
  api::safe_browsing_private::InterstitialInfo params;
  params.url = url.spec();
  params.reason = reason;
  if (net_error_code < 0) {
    params.net_error_code = base::NumberToString(net_error_code);
  }
  params.user_name = reporting_client_->GetProfileUserName();

  // |event_router_| can be null in tests.
  if (event_router_) {
    base::Value::List event_value;
    event_value.Append(params.ToValue());

    auto extension_event = std::make_unique<Event>(
        events::SAFE_BROWSING_PRIVATE_ON_SECURITY_INTERSTITIAL_PROCEEDED,
        api::safe_browsing_private::OnSecurityInterstitialProceeded::kEventName,
        std::move(event_value));
    event_router_->BroadcastEvent(std::move(extension_event));
  }
}

void SafeBrowsingPrivateEventRouter::OnAnalysisConnectorResult(
    const GURL& url,
    const GURL& tab_url,
    const std::string& source,
    const std::string& destination,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    const std::string& scan_id,
    const std::string& content_transfer_method,
    safe_browsing::DeepScanAccessPoint access_point,
    const enterprise_connectors::ContentAnalysisResponse::Result& result,
    const int64_t content_size,
    const safe_browsing::ReferrerChain& referrer_chain,
    enterprise_connectors::EventResult event_result) {
  if (result.tag() == "malware") {
    DCHECK_EQ(1, result.triggered_rules().size());
    OnDangerousDeepScanningResult(
        url, tab_url, source, destination, file_name, download_digest_sha256,
        MalwareRuleToThreatType(result.triggered_rules(0).rule_name()),
        mime_type, trigger, content_size, referrer_chain, event_result, scan_id,
        content_transfer_method);
  } else if (result.tag() == "dlp") {
    OnSensitiveDataEvent(url, tab_url, source, destination, file_name,
                         download_digest_sha256, mime_type, trigger, scan_id,
                         content_transfer_method, result, content_size,
                         referrer_chain, event_result);
  }
}

void SafeBrowsingPrivateEventRouter::OnDangerousDeepScanningResult(
    const GURL& url,
    const GURL& tab_url,
    const std::string& source,
    const std::string& destination,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& threat_type,
    const std::string& mime_type,
    const std::string& trigger,
    const int64_t content_size,
    const safe_browsing::ReferrerChain& referrer_chain,
    enterprise_connectors::EventResult event_result,
    const std::string& scan_id,
    const std::string& content_transfer_method) {
#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
  std::optional<enterprise_connectors::ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(
          enterprise_connectors::kKeyDangerousDownloadEvent) == 0) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUrl, url.spec());
  event.Set(kKeyTabUrl, tab_url.spec());
  event.Set(kKeySource, source);
  event.Set(kKeyDestination, destination);
  event.Set(kKeyFileName,
            GetFileName(file_name, enterprise_connectors::IncludeDeviceInfo(
                                       Profile::FromBrowserContext(context_),
                                       settings->per_profile)));
  event.Set(kKeyDownloadDigestSha256, download_digest_sha256);
  event.Set(kKeyThreatType, threat_type);
  event.Set(kKeyContentType, mime_type);
  // |content_size| can be set to -1 to indicate an unknown size, in
  // which case the field is not set.
  if (content_size >= 0) {
    event.Set(kKeyContentSize, base::Int64ToValue(content_size));
  }
  event.Set(kKeyTrigger, trigger);
  if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedFieldsForSecOps)) {
    enterprise_connectors::AddReferrerChainToEvent(referrer_chain, event);
  }
  event.Set(kKeyEventResult,
            enterprise_connectors::EventResultToString(event_result));
  event.Set(kKeyClickedThrough,
            event_result == enterprise_connectors::EventResult::BYPASSED);
  // The scan ID can be empty when the reported dangerous download is from a
  // Safe Browsing verdict.
  if (!scan_id.empty()) {
    event.Set(kKeyScanId, scan_id);
  }
  if (!content_transfer_method.empty()) {
    event.Set(kKeyContentTransferMethod, content_transfer_method);
  }

  reporting_client_->ReportRealtimeEvent(
      enterprise_connectors::kKeyDangerousDownloadEvent,
      std::move(settings.value()), std::move(event));
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
}

void SafeBrowsingPrivateEventRouter::OnSensitiveDataEvent(
    const GURL& url,
    const GURL& tab_url,
    const std::string& source,
    const std::string& destination,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    const std::string& scan_id,
    const std::string& content_transfer_method,
    const enterprise_connectors::ContentAnalysisResponse::Result& result,
    const int64_t content_size,
    const safe_browsing::ReferrerChain& referrer_chain,
    enterprise_connectors::EventResult event_result) {
#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
  std::optional<enterprise_connectors::ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(
          enterprise_connectors::kKeySensitiveDataEvent) == 0) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUrl, url.spec());
  event.Set(kKeyTabUrl, tab_url.spec());
  event.Set(kKeySource, source);
  event.Set(kKeyDestination, destination);
  event.Set(kKeyFileName,
            GetFileName(file_name, enterprise_connectors::IncludeDeviceInfo(
                                       Profile::FromBrowserContext(context_),
                                       settings->per_profile)));
  event.Set(kKeyDownloadDigestSha256, download_digest_sha256);
  event.Set(kKeyContentType, mime_type);
  // |content_size| can be set to -1 to indicate an unknown size, in
  // which case the field is not set.
  if (content_size >= 0) {
    event.Set(kKeyContentSize, base::Int64ToValue(content_size));
  }
  event.Set(kKeyTrigger, trigger);
  if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedFieldsForSecOps)) {
    enterprise_connectors::AddReferrerChainToEvent(referrer_chain, event);
  }
  event.Set(kKeyEventResult,
            enterprise_connectors::EventResultToString(event_result));
  event.Set(kKeyClickedThrough,
            event_result == enterprise_connectors::EventResult::BYPASSED);
  event.Set(kKeyScanId, scan_id);
  if (!content_transfer_method.empty()) {
    event.Set(kKeyContentTransferMethod, content_transfer_method);
  }

  AddAnalysisConnectorVerdictToEvent(result, event);

  reporting_client_->ReportRealtimeEvent(
      enterprise_connectors::kKeySensitiveDataEvent,
      std::move(settings.value()), std::move(event));
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
}

void SafeBrowsingPrivateEventRouter::OnAnalysisConnectorWarningBypassed(
    const GURL& url,
    const GURL& tab_url,
    const std::string& source,
    const std::string& destination,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    const std::string& scan_id,
    const std::string& content_transfer_method,
    safe_browsing::DeepScanAccessPoint access_point,
    const safe_browsing::ReferrerChain& referrer_chain,
    const enterprise_connectors::ContentAnalysisResponse::Result& result,
    const int64_t content_size,
    std::optional<std::u16string> user_justification) {
#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
  std::optional<enterprise_connectors::ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(
          enterprise_connectors::kKeySensitiveDataEvent) == 0) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUrl, url.spec());
  event.Set(kKeyTabUrl, tab_url.spec());
  event.Set(kKeySource, source);
  event.Set(kKeyDestination, destination);
  event.Set(kKeyFileName,
            GetFileName(file_name, enterprise_connectors::IncludeDeviceInfo(
                                       Profile::FromBrowserContext(context_),
                                       settings->per_profile)));
  event.Set(kKeyDownloadDigestSha256, download_digest_sha256);
  event.Set(kKeyContentType, mime_type);
  // |content_size| can be set to -1 to indicate an unknown size, in
  // which case the field is not set.
  if (content_size >= 0) {
    event.Set(kKeyContentSize, base::Int64ToValue(content_size));
  }
  event.Set(kKeyTrigger, trigger);
  event.Set(kKeyEventResult, enterprise_connectors::EventResultToString(
                                 enterprise_connectors::EventResult::BYPASSED));
  event.Set(kKeyClickedThrough, true);
  event.Set(kKeyScanId, scan_id);
  if (user_justification) {
    event.Set(kKeyUserJustification, *user_justification);
  }
  if (!content_transfer_method.empty()) {
    event.Set(kKeyContentTransferMethod, content_transfer_method);
  }
  if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedFieldsForSecOps)) {
    enterprise_connectors::AddReferrerChainToEvent(referrer_chain, event);
  }

  AddAnalysisConnectorVerdictToEvent(result, event);

  reporting_client_->ReportRealtimeEvent(
      enterprise_connectors::kKeySensitiveDataEvent,
      std::move(settings.value()), std::move(event));
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
}

void SafeBrowsingPrivateEventRouter::OnUnscannedFileEvent(
    const GURL& url,
    const GURL& tab_url,
    const std::string& source,
    const std::string& destination,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    safe_browsing::DeepScanAccessPoint access_point,
    const std::string& reason,
    const std::string& content_transfer_method,
    const int64_t content_size,
    const safe_browsing::ReferrerChain& referrer_chain,
    enterprise_connectors::EventResult event_result) {
#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
  std::optional<enterprise_connectors::ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(
          enterprise_connectors::kKeyUnscannedFileEvent) == 0) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUrl, url.spec());
  event.Set(kKeyTabUrl, tab_url.spec());
  event.Set(kKeySource, source);
  event.Set(kKeyDestination, destination);
  event.Set(kKeyFileName,
            GetFileName(file_name, enterprise_connectors::IncludeDeviceInfo(
                                       Profile::FromBrowserContext(context_),
                                       settings->per_profile)));
  event.Set(kKeyDownloadDigestSha256, download_digest_sha256);
  event.Set(kKeyContentType, mime_type);
  event.Set(kKeyUnscannedReason, reason);
  // |content_size| can be set to -1 to indicate an unknown size, in
  // which case the field is not set.
  if (content_size >= 0) {
    event.Set(kKeyContentSize, base::Int64ToValue(content_size));
  }
  event.Set(kKeyTrigger, trigger);
  if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedFieldsForSecOps)) {
    enterprise_connectors::AddReferrerChainToEvent(referrer_chain, event);
  }
  event.Set(kKeyEventResult,
            enterprise_connectors::EventResultToString(event_result));
  event.Set(kKeyClickedThrough,
            event_result == enterprise_connectors::EventResult::BYPASSED);
  if (!content_transfer_method.empty()) {
    event.Set(kKeyContentTransferMethod, content_transfer_method);
  }

  reporting_client_->ReportRealtimeEvent(
      enterprise_connectors::kKeyUnscannedFileEvent,
      std::move(settings.value()), std::move(event));
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
}

void SafeBrowsingPrivateEventRouter::OnDangerousDownloadEvent(
    const GURL& url,
    const GURL& tab_url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const download::DownloadDangerType danger_type,
    const std::string& mime_type,
    const std::string& scan_id,
    const int64_t content_size,
    const safe_browsing::ReferrerChain& referrer_chain,
    enterprise_connectors::EventResult event_result) {
  OnDangerousDownloadEvent(url, tab_url, file_name, download_digest_sha256,
                           DangerTypeToThreatType(danger_type), mime_type,
                           scan_id, content_size, referrer_chain, event_result);
}

void SafeBrowsingPrivateEventRouter::OnDangerousDownloadEvent(
    const GURL& url,
    const GURL& tab_url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& threat_type,
    const std::string& mime_type,
    const std::string& scan_id,
    const int64_t content_size,
    const safe_browsing::ReferrerChain& referrer_chain,
    enterprise_connectors::EventResult event_result) {
#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
  std::optional<enterprise_connectors::ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(
          enterprise_connectors::kKeyDangerousDownloadEvent) == 0) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUrl, url.spec());
  event.Set(kKeyTabUrl, tab_url.spec());
  event.Set(kKeyFileName,
            GetFileName(file_name, enterprise_connectors::IncludeDeviceInfo(
                                       Profile::FromBrowserContext(context_),
                                       settings->per_profile)));
  event.Set(kKeyDownloadDigestSha256, download_digest_sha256);
  event.Set(kKeyThreatType, threat_type);
  event.Set(kKeyClickedThrough, false);
  event.Set(kKeyContentType, mime_type);
  // |content_size| can be set to -1 to indicate an unknown size, in
  // which case the field is not set.
  if (content_size >= 0) {
    event.Set(kKeyContentSize, base::Int64ToValue(content_size));
  }
  event.Set(kKeyTrigger, kTriggerFileDownload);
  event.Set(kKeyEventResult,
            enterprise_connectors::EventResultToString(event_result));

  // The scan ID can be empty when the reported dangerous download is from a
  // Safe Browsing verdict.
  if (!scan_id.empty()) {
    event.Set(kKeyScanId, scan_id);
  }

  if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedFieldsForSecOps)) {
    enterprise_connectors::AddReferrerChainToEvent(referrer_chain, event);
  }

  reporting_client_->ReportRealtimeEvent(
      enterprise_connectors::kKeyDangerousDownloadEvent,
      std::move(settings.value()), std::move(event));
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
}

void SafeBrowsingPrivateEventRouter::OnDangerousDownloadWarningBypassed(
    const GURL& url,
    const GURL& tab_url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const download::DownloadDangerType danger_type,
    const std::string& mime_type,
    const std::string& scan_id,
    const int64_t content_size,
    const safe_browsing::ReferrerChain& referrer_chain) {
  OnDangerousDownloadWarningBypassed(
      url, tab_url, file_name, download_digest_sha256,
      DangerTypeToThreatType(danger_type), mime_type, scan_id, content_size,
      referrer_chain);
}

void SafeBrowsingPrivateEventRouter::OnDangerousDownloadWarningBypassed(
    const GURL& url,
    const GURL& tab_url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& threat_type,
    const std::string& mime_type,
    const std::string& scan_id,
    const int64_t content_size,
    const safe_browsing::ReferrerChain& referrer_chain) {
#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
  std::optional<enterprise_connectors::ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(
          enterprise_connectors::kKeyDangerousDownloadEvent) == 0) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUrl, url.spec());
  event.Set(kKeyTabUrl, tab_url.spec());
  event.Set(kKeyFileName,
            GetFileName(file_name, enterprise_connectors::IncludeDeviceInfo(
                                       Profile::FromBrowserContext(context_),
                                       settings->per_profile)));
  event.Set(kKeyDownloadDigestSha256, download_digest_sha256);
  event.Set(kKeyThreatType, threat_type);
  event.Set(kKeyClickedThrough, true);
  event.Set(kKeyContentType, mime_type);
  // |content_size| can be set to -1 to indicate an unknown size, in
  // which case the field is not set.
  if (content_size >= 0) {
    event.Set(kKeyContentSize, base::Int64ToValue(content_size));
  }
  event.Set(kKeyTrigger, kTriggerFileDownload);
  event.Set(kKeyEventResult, enterprise_connectors::EventResultToString(
                                 enterprise_connectors::EventResult::BYPASSED));
  // The scan ID can be empty when the reported dangerous download is from a
  // Safe Browsing verdict.
  if (!scan_id.empty()) {
    event.Set(kKeyScanId, scan_id);
  }

  if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedFieldsForSecOps)) {
    enterprise_connectors::AddReferrerChainToEvent(referrer_chain, event);
  }

  reporting_client_->ReportRealtimeEvent(
      enterprise_connectors::kKeyDangerousDownloadEvent,
      std::move(settings.value()), std::move(event));
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
}

#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
void SafeBrowsingPrivateEventRouter::OnDataControlsSensitiveDataEvent(
    const GURL& url,
    const GURL& tab_url,
    const std::string& source,
    const std::string& destination,
    const std::string& mime_type,
    const std::string& trigger,
    const data_controls::Verdict::TriggeredRules& triggered_rules,
    enterprise_connectors::EventResult event_result,
    int64_t content_size) {
  std::optional<enterprise_connectors::ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value() ||
      !base::Contains(settings->enabled_event_names,
                      enterprise_connectors::kKeySensitiveDataEvent)) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUrl, url.spec());
  event.Set(kKeyTabUrl, tab_url.spec());
  event.Set(kKeySource, source);
  event.Set(kKeyDestination, destination);
  event.Set(kKeyContentType, mime_type);
  // |content_size| can be set to -1 to indicate an unknown size, in
  // which case the field is not set.
  if (content_size >= 0) {
    event.Set(kKeyContentSize, base::Int64ToValue(content_size));
  }
  event.Set(kKeyTrigger, trigger);
  event.Set(kKeyEventResult,
            enterprise_connectors::EventResultToString(event_result));

  base::Value::List triggered_rule_info;
  triggered_rule_info.reserve(triggered_rules.size());
  for (const auto& [index, rule] : triggered_rules) {
    base::Value::Dict triggered_rule;
    int rule_id_int = 0;
    if (base::StringToInt(rule.rule_id, &rule_id_int)) {
      triggered_rule.Set(SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleId,
                         rule_id_int);
    }
    triggered_rule.Set(SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleName,
                       rule.rule_name);

    triggered_rule_info.Append(std::move(triggered_rule));
  }
  event.Set(SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleInfo,
            std::move(triggered_rule_info));

  reporting_client_->ReportRealtimeEvent(
      enterprise_connectors::kKeySensitiveDataEvent,
      std::move(settings.value()), std::move(event));
}
#endif  // BUILDFLAG(ENTERPRISE_DATA_CONTROLS)

}  // namespace extensions
