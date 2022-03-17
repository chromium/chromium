// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/json/values_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/extensions/api/safe_browsing_private.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/url_matcher/url_matcher.h"
#include "components/url_matcher/url_util.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#else
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#endif

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kActiveDirectoryPolicyClientDescription[] = "an Active Directory";
const char kPolicyClientDescription[] = "any";
const char kUserPolicyClientDescription[] = "a user";
#else
const char kChromeBrowserCloudManagementClientDescription[] =
    "a machine-level user";
#endif
const char kProfilePolicyClientDescription[] = "a profile-level user";

const char16_t kMaskedUsername[] = u"*****";

void AddAnalysisConnectorVerdictToEvent(
    const enterprise_connectors::ContentAnalysisResponse::Result& result,
    base::Value::Dict& event) {
  base::Value::List triggered_rule_info;
  for (const enterprise_connectors::TriggeredRule& trigger :
       result.triggered_rules()) {
    base::Value::Dict triggered_rule;
    triggered_rule.Set(
        extensions::SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleName,
        trigger.rule_name());
    triggered_rule.Set(
        extensions::SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleId,
        trigger.rule_id());

    triggered_rule_info.Append(std::move(triggered_rule));
  }
  event.Set(extensions::SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleInfo,
            std::move(triggered_rule_info));
}

std::string MalwareRuleToThreatType(const std::string& rule_name) {
  if (rule_name == "uws") {
    return "POTENTIALLY_UNWANTED";
  } else if (rule_name == "malware") {
    return "DANGEROUS";
  } else {
    return "UNKNOWN";
  }
}

bool IsClientValid(const std::string& dm_token,
                   policy::CloudPolicyClient* client) {
  return client && client->dm_token() == dm_token;
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

// Do a best-effort masking of `username`. If it's an email address (such as
// foo@example.com), everything before @ should be masked. Otherwise, the entire
// username should be masked.
std::u16string MaskUsername(const std::u16string& username) {
  size_t pos = username.find(u"@");
  if (pos == std::string::npos) {
    return std::u16string(kMaskedUsername);
  }

  return std::u16string(kMaskedUsername) + username.substr(pos);
}

// Create a URLMatcher representing the filters in
// `settings.enabled_opt_in_events` for `event_type`. This field of the
// reporting settings connector contains a map where keys are event types and
// values are lists of URL patterns specifying on which URLs the events are
// allowed to be reported. An event is generated iff its event type is present
// in the opt-in events field and the URL it relates to matches at least one of
// the event type's filters.
std::unique_ptr<url_matcher::URLMatcher> CreateURLMatcherForOptInEvent(
    const enterprise_connectors::ReportingSettings& settings,
    const char* event_type) {
  const auto& it = settings.enabled_opt_in_events.find(event_type);
  if (it == settings.enabled_opt_in_events.end())
    return nullptr;

  std::unique_ptr<url_matcher::URLMatcher> matcher =
      std::make_unique<url_matcher::URLMatcher>();
  url_matcher::URLMatcherConditionSet::ID unused_id(0);
  url_matcher::util::AddFilters(matcher.get(), true, &unused_id, it->second);

  return matcher;
}

bool IsOptInEventEnabled(url_matcher::URLMatcher* matcher, const GURL& url) {
  return matcher && !matcher->MatchURL(url).empty();
}

}  // namespace

namespace extensions {

const base::Feature SafeBrowsingPrivateEventRouter::kRealtimeReportingFeature{
    "SafeBrowsingRealtimeReporting", base::FEATURE_DISABLED_BY_DEFAULT};

// Key names used with when building the dictionary to pass to the real-time
// reporting API.
const char SafeBrowsingPrivateEventRouter::kKeyUrl[] = "url";
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
const char SafeBrowsingPrivateEventRouter::kKeyEventResult[] = "eventResult";
const char SafeBrowsingPrivateEventRouter::kKeyMalwareFamily[] =
    "malwareFamily";
const char SafeBrowsingPrivateEventRouter::kKeyMalwareCategory[] =
    "malwareCategory";
const char SafeBrowsingPrivateEventRouter::kKeyEvidenceLockerFilePath[] =
    "evidenceLockerFilepath";
const char SafeBrowsingPrivateEventRouter::kKeyScanId[] = "scanId";
const char SafeBrowsingPrivateEventRouter::kKeyIsFederated[] = "isFederated";
const char SafeBrowsingPrivateEventRouter::kKeyFederatedOrigin[] =
    "federatedOrigin";
const char SafeBrowsingPrivateEventRouter::kKeyLoginUserName[] =
    "loginUserName";
const char SafeBrowsingPrivateEventRouter::kKeyPasswordBreachIdentities[] =
    "identities";
const char SafeBrowsingPrivateEventRouter::kKeyPasswordBreachIdentitiesUrl[] =
    "url";
const char
    SafeBrowsingPrivateEventRouter::kKeyPasswordBreachIdentitiesUsername[] =
        "username";
const char SafeBrowsingPrivateEventRouter::kKeyUserJustification[] =
    "userJustification";

// All new event names should be added to the kAllEvents array below!
const char SafeBrowsingPrivateEventRouter::kKeyPasswordReuseEvent[] =
    "passwordReuseEvent";
const char SafeBrowsingPrivateEventRouter::kKeyPasswordChangedEvent[] =
    "passwordChangedEvent";
const char SafeBrowsingPrivateEventRouter::kKeyDangerousDownloadEvent[] =
    "dangerousDownloadEvent";
const char SafeBrowsingPrivateEventRouter::kKeyInterstitialEvent[] =
    "interstitialEvent";
const char SafeBrowsingPrivateEventRouter::kKeySensitiveDataEvent[] =
    "sensitiveDataEvent";
const char SafeBrowsingPrivateEventRouter::kKeyUnscannedFileEvent[] =
    "unscannedFileEvent";
const char SafeBrowsingPrivateEventRouter::kKeyLoginEvent[] = "loginEvent";
const char SafeBrowsingPrivateEventRouter::kKeyPasswordBreachEvent[] =
    "passwordBreachEvent";
// All new event names should be added to this array!
const char* SafeBrowsingPrivateEventRouter::kAllEvents[8] = {
    SafeBrowsingPrivateEventRouter::kKeyPasswordReuseEvent,
    SafeBrowsingPrivateEventRouter::kKeyPasswordChangedEvent,
    SafeBrowsingPrivateEventRouter::kKeyDangerousDownloadEvent,
    SafeBrowsingPrivateEventRouter::kKeyInterstitialEvent,
    SafeBrowsingPrivateEventRouter::kKeySensitiveDataEvent,
    SafeBrowsingPrivateEventRouter::kKeyUnscannedFileEvent,
    SafeBrowsingPrivateEventRouter::kKeyLoginEvent,
    SafeBrowsingPrivateEventRouter::kKeyPasswordBreachEvent,
};

const char SafeBrowsingPrivateEventRouter::kKeyUnscannedReason[] =
    "unscannedReason";

const char SafeBrowsingPrivateEventRouter::kTriggerFileDownload[] =
    "FILE_DOWNLOAD";
const char SafeBrowsingPrivateEventRouter::kTriggerFileUpload[] = "FILE_UPLOAD";
const char SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload[] =
    "WEB_CONTENT_UPLOAD";

SafeBrowsingPrivateEventRouter::SafeBrowsingPrivateEventRouter(
    content::BrowserContext* context)
    : context_(context) {
  event_router_ = EventRouter::Get(context_);
  identity_manager_ = IdentityManagerFactory::GetForProfile(
      Profile::FromBrowserContext(context_));
}

SafeBrowsingPrivateEventRouter::~SafeBrowsingPrivateEventRouter() {
  if (browser_client_)
    browser_client_->RemoveObserver(this);
  if (profile_client_)
    profile_client_->RemoveObserver(this);
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
    std::vector<base::Value> event_value;
    event_value.push_back(base::Value::FromUniquePtrValue(params.ToValue()));

    auto extension_event = std::make_unique<Event>(
        events::
            SAFE_BROWSING_PRIVATE_ON_POLICY_SPECIFIED_PASSWORD_REUSE_DETECTED,
        api::safe_browsing_private::OnPolicySpecifiedPasswordReuseDetected::
            kEventName,
        std::move(event_value));
    event_router_->BroadcastEvent(std::move(extension_event));
  }

  absl::optional<enterprise_connectors::ReportingSettings> settings =
      GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(kKeyPasswordReuseEvent) == 0) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUrl, params.url);
  event.Set(kKeyUserName, params.user_name);
  event.Set(kKeyIsPhishingUrl, params.is_phishing_url);
  event.Set(kKeyProfileUserName, GetProfileUserName());
  event.Set(kKeyEventResult,
            safe_browsing::EventResultToString(
                warning_shown ? safe_browsing::EventResult::WARNED
                              : safe_browsing::EventResult::ALLOWED));

  ReportRealtimeEvent(kKeyPasswordReuseEvent, std::move(settings.value()),
                      std::move(event));
}

void SafeBrowsingPrivateEventRouter::OnPolicySpecifiedPasswordChanged(
    const std::string& user_name) {
  // |event_router_| can be null in tests.
  if (event_router_) {
    std::vector<base::Value> event_value;
    event_value.push_back(base::Value(user_name));
    auto extension_event = std::make_unique<Event>(
        events::SAFE_BROWSING_PRIVATE_ON_POLICY_SPECIFIED_PASSWORD_CHANGED,
        api::safe_browsing_private::OnPolicySpecifiedPasswordChanged::
            kEventName,
        std::move(event_value));
    event_router_->BroadcastEvent(std::move(extension_event));
  }

  absl::optional<enterprise_connectors::ReportingSettings> settings =
      GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(kKeyPasswordChangedEvent) == 0) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUserName, user_name);
  event.Set(kKeyProfileUserName, GetProfileUserName());

  ReportRealtimeEvent(kKeyPasswordChangedEvent, std::move(settings.value()),
                      std::move(event));
}

void SafeBrowsingPrivateEventRouter::OnDangerousDownloadOpened(
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& scan_id,
    const download::DownloadDangerType danger_type,
    const int64_t content_size) {
  api::safe_browsing_private::DangerousDownloadInfo params;
  params.url = url.spec();
  params.file_name = file_name;
  params.download_digest_sha256 = download_digest_sha256;
  params.user_name = GetProfileUserName();

  // |event_router_| can be null in tests.
  if (event_router_) {
    std::vector<base::Value> event_value;
    event_value.push_back(base::Value::FromUniquePtrValue(params.ToValue()));

    auto extension_event = std::make_unique<Event>(
        events::SAFE_BROWSING_PRIVATE_ON_DANGEROUS_DOWNLOAD_OPENED,
        api::safe_browsing_private::OnDangerousDownloadOpened::kEventName,
        std::move(event_value));
    event_router_->BroadcastEvent(std::move(extension_event));
  }

  absl::optional<enterprise_connectors::ReportingSettings> settings =
      GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(kKeyDangerousDownloadEvent) == 0) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUrl, params.url);
  event.Set(kKeyFileName, params.file_name);
  event.Set(kKeyDownloadDigestSha256, params.download_digest_sha256);
  event.Set(kKeyProfileUserName, params.user_name);
  event.Set(kKeyContentType, mime_type);
  // |content_size| can be set to -1 to indicate an unknown size, in
  // which case the field is not set.
  if (content_size >= 0)
    event.Set(kKeyContentSize, base::Int64ToValue(content_size));
  event.Set(kKeyTrigger, kTriggerFileDownload);
  event.Set(kKeyEventResult, safe_browsing::EventResultToString(
                                 safe_browsing::EventResult::BYPASSED));
  event.Set(kKeyClickedThrough, true);
  event.Set(kKeyThreatType, DangerTypeToThreatType(danger_type));
  // The scan ID can be empty when the reported dangerous download is from a
  // Safe Browsing verdict.
  if (!scan_id.empty()) {
    event.Set(kKeyScanId, scan_id);
  }

  ReportRealtimeEvent(kKeyDangerousDownloadEvent, std::move(settings.value()),
                      std::move(event));
}

void SafeBrowsingPrivateEventRouter::OnSecurityInterstitialShown(
    const GURL& url,
    const std::string& reason,
    int net_error_code) {
  api::safe_browsing_private::InterstitialInfo params;
  params.url = url.spec();
  params.reason = reason;
  if (net_error_code < 0) {
    params.net_error_code =
        std::make_unique<std::string>(base::NumberToString(net_error_code));
  }
  params.user_name = GetProfileUserName();

  // |event_router_| can be null in tests.
  if (event_router_) {
    std::vector<base::Value> event_value;
    event_value.push_back(base::Value::FromUniquePtrValue(params.ToValue()));

    auto extension_event = std::make_unique<Event>(
        events::SAFE_BROWSING_PRIVATE_ON_SECURITY_INTERSTITIAL_SHOWN,
        api::safe_browsing_private::OnSecurityInterstitialShown::kEventName,
        std::move(event_value));
    event_router_->BroadcastEvent(std::move(extension_event));
  }

  absl::optional<enterprise_connectors::ReportingSettings> settings =
      GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(kKeyInterstitialEvent) == 0) {
    return;
  }

  PrefService* prefs = Profile::FromBrowserContext(context_)->GetPrefs();
  safe_browsing::EventResult event_result =
      prefs->GetBoolean(prefs::kSafeBrowsingProceedAnywayDisabled)
          ? safe_browsing::EventResult::BLOCKED
          : safe_browsing::EventResult::WARNED;
  base::Value::Dict event;
  event.Set(kKeyUrl, params.url);
  event.Set(kKeyReason, params.reason);
  event.Set(kKeyNetErrorCode, net_error_code);
  event.Set(kKeyProfileUserName, params.user_name);
  event.Set(kKeyClickedThrough, false);
  event.Set(kKeyEventResult, safe_browsing::EventResultToString(event_result));

  ReportRealtimeEvent(kKeyInterstitialEvent, std::move(settings.value()),
                      std::move(event));
}

void SafeBrowsingPrivateEventRouter::OnSecurityInterstitialProceeded(
    const GURL& url,
    const std::string& reason,
    int net_error_code) {
  api::safe_browsing_private::InterstitialInfo params;
  params.url = url.spec();
  params.reason = reason;
  if (net_error_code < 0) {
    params.net_error_code =
        std::make_unique<std::string>(base::NumberToString(net_error_code));
  }
  params.user_name = GetProfileUserName();

  // |event_router_| can be null in tests.
  if (event_router_) {
    std::vector<base::Value> event_value;
    event_value.push_back(base::Value::FromUniquePtrValue(params.ToValue()));

    auto extension_event = std::make_unique<Event>(
        events::SAFE_BROWSING_PRIVATE_ON_SECURITY_INTERSTITIAL_PROCEEDED,
        api::safe_browsing_private::OnSecurityInterstitialProceeded::kEventName,
        std::move(event_value));
    event_router_->BroadcastEvent(std::move(extension_event));
  }

  absl::optional<enterprise_connectors::ReportingSettings> settings =
      GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(kKeyInterstitialEvent) == 0) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUrl, params.url);
  event.Set(kKeyReason, params.reason);
  event.Set(kKeyNetErrorCode, net_error_code);
  event.Set(kKeyProfileUserName, params.user_name);
  event.Set(kKeyClickedThrough, true);
  event.Set(kKeyEventResult, safe_browsing::EventResultToString(
                                 safe_browsing::EventResult::BYPASSED));

  ReportRealtimeEvent(kKeyInterstitialEvent, std::move(settings.value()),
                      std::move(event));
}

void SafeBrowsingPrivateEventRouter::OnAnalysisConnectorResult(
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    const std::string& scan_id,
    safe_browsing::DeepScanAccessPoint /* access_point */,
    const enterprise_connectors::ContentAnalysisResponse::Result& result,
    const int64_t content_size,
    safe_browsing::EventResult event_result) {
  if (result.tag() == "malware") {
    DCHECK_EQ(1, result.triggered_rules().size());
    OnDangerousDeepScanningResult(
        url, file_name, download_digest_sha256,
        MalwareRuleToThreatType(result.triggered_rules(0).rule_name()),
        mime_type, trigger, content_size, event_result, result.malware_family(),
        result.malware_category(), result.evidence_locker_filepath(), scan_id);
  } else if (result.tag() == "dlp") {
    OnSensitiveDataEvent(url, file_name, download_digest_sha256, mime_type,
                         trigger, scan_id, result, content_size, event_result);
  }
}

void SafeBrowsingPrivateEventRouter::OnDangerousDeepScanningResult(
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& threat_type,
    const std::string& mime_type,
    const std::string& trigger,
    const int64_t content_size,
    safe_browsing::EventResult event_result,
    const std::string& malware_family,
    const std::string& malware_category,
    const std::string& evidence_locker_filepath,
    const std::string& scan_id) {
  absl::optional<enterprise_connectors::ReportingSettings> settings =
      GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(kKeyDangerousDownloadEvent) == 0) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUrl, url.spec());
  event.Set(kKeyFileName, file_name);
  event.Set(kKeyDownloadDigestSha256, download_digest_sha256);
  event.Set(kKeyProfileUserName, GetProfileUserName());
  event.Set(kKeyThreatType, threat_type);
  event.Set(kKeyContentType, mime_type);
  // |content_size| can be set to -1 to indicate an unknown size, in
  // which case the field is not set.
  if (content_size >= 0) {
    event.Set(kKeyContentSize, base::Int64ToValue(content_size));
  }
  event.Set(kKeyTrigger, trigger);
  event.Set(kKeyEventResult, safe_browsing::EventResultToString(event_result));
  event.Set(kKeyClickedThrough,
            event_result == safe_browsing::EventResult::BYPASSED);
  if (!malware_family.empty()) {
    event.Set(kKeyMalwareFamily, malware_family);
  }
  if (!malware_category.empty()) {
    event.Set(kKeyMalwareCategory, malware_category);
  }
  if (!evidence_locker_filepath.empty()) {
    event.Set(kKeyEvidenceLockerFilePath, evidence_locker_filepath);
  }
  // The scan ID can be empty when the reported dangerous download is from a
  // Safe Browsing verdict.
  if (!scan_id.empty()) {
    event.Set(kKeyScanId, scan_id);
  }

  ReportRealtimeEvent(kKeyDangerousDownloadEvent, std::move(settings.value()),
                      std::move(event));
}

void SafeBrowsingPrivateEventRouter::OnSensitiveDataEvent(
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    const std::string& scan_id,
    const enterprise_connectors::ContentAnalysisResponse::Result& result,
    const int64_t content_size,
    safe_browsing::EventResult event_result) {
  absl::optional<enterprise_connectors::ReportingSettings> settings =
      GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(kKeySensitiveDataEvent) == 0) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUrl, url.spec());
  event.Set(kKeyFileName, file_name);
  event.Set(kKeyDownloadDigestSha256, download_digest_sha256);
  event.Set(kKeyProfileUserName, GetProfileUserName());
  event.Set(kKeyContentType, mime_type);
  // |content_size| can be set to -1 to indicate an unknown size, in
  // which case the field is not set.
  if (content_size >= 0) {
    event.Set(kKeyContentSize, base::Int64ToValue(content_size));
  }
  event.Set(kKeyTrigger, trigger);
  event.Set(kKeyEventResult, safe_browsing::EventResultToString(event_result));
  event.Set(kKeyClickedThrough,
            event_result == safe_browsing::EventResult::BYPASSED);
  if (!result.evidence_locker_filepath().empty()) {
    event.Set(kKeyEvidenceLockerFilePath, result.evidence_locker_filepath());
  }
  event.Set(kKeyScanId, scan_id);

  AddAnalysisConnectorVerdictToEvent(result, event);

  ReportRealtimeEvent(kKeySensitiveDataEvent, std::move(settings.value()),
                      std::move(event));
}

void SafeBrowsingPrivateEventRouter::OnAnalysisConnectorWarningBypassed(
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    const std::string& scan_id,
    safe_browsing::DeepScanAccessPoint access_point,
    const enterprise_connectors::ContentAnalysisResponse::Result& result,
    const int64_t content_size,
    absl::optional<std::u16string> user_justification) {
  absl::optional<enterprise_connectors::ReportingSettings> settings =
      GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(kKeySensitiveDataEvent) == 0) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUrl, url.spec());
  event.Set(kKeyFileName, file_name);
  event.Set(kKeyDownloadDigestSha256, download_digest_sha256);
  event.Set(kKeyProfileUserName, GetProfileUserName());
  event.Set(kKeyContentType, mime_type);
  // |content_size| can be set to -1 to indicate an unknown size, in
  // which case the field is not set.
  if (content_size >= 0) {
    event.Set(kKeyContentSize, base::Int64ToValue(content_size));
  }
  event.Set(kKeyTrigger, trigger);
  event.Set(kKeyEventResult, safe_browsing::EventResultToString(
                                 safe_browsing::EventResult::BYPASSED));
  event.Set(kKeyClickedThrough, true);
  if (!result.evidence_locker_filepath().empty()) {
    event.Set(kKeyEvidenceLockerFilePath, result.evidence_locker_filepath());
  }
  event.Set(kKeyScanId, scan_id);
  if (user_justification) {
    event.Set(kKeyUserJustification, *user_justification);
  }

  AddAnalysisConnectorVerdictToEvent(result, event);

  ReportRealtimeEvent(kKeySensitiveDataEvent, std::move(settings.value()),
                      std::move(event));
}

void SafeBrowsingPrivateEventRouter::OnUnscannedFileEvent(
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    safe_browsing::DeepScanAccessPoint access_point,
    const std::string& reason,
    const int64_t content_size,
    safe_browsing::EventResult event_result) {
  absl::optional<enterprise_connectors::ReportingSettings> settings =
      GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(kKeyUnscannedFileEvent) == 0) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUrl, url.spec());
  event.Set(kKeyFileName, file_name);
  event.Set(kKeyDownloadDigestSha256, download_digest_sha256);
  event.Set(kKeyProfileUserName, GetProfileUserName());
  event.Set(kKeyContentType, mime_type);
  event.Set(kKeyUnscannedReason, reason);
  // |content_size| can be set to -1 to indicate an unknown size, in
  // which case the field is not set.
  if (content_size >= 0) {
    event.Set(kKeyContentSize, base::Int64ToValue(content_size));
  }
  event.Set(kKeyTrigger, trigger);
  event.Set(kKeyEventResult, safe_browsing::EventResultToString(event_result));
  event.Set(kKeyClickedThrough,
            event_result == safe_browsing::EventResult::BYPASSED);

  ReportRealtimeEvent(kKeyUnscannedFileEvent, std::move(settings.value()),
                      std::move(event));
}

void SafeBrowsingPrivateEventRouter::OnDangerousDownloadEvent(
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const download::DownloadDangerType danger_type,
    const std::string& mime_type,
    const std::string& scan_id,
    const int64_t content_size,
    safe_browsing::EventResult event_result) {
  OnDangerousDownloadEvent(url, file_name, download_digest_sha256,
                           DangerTypeToThreatType(danger_type), mime_type,
                           scan_id, content_size, event_result);
}

void SafeBrowsingPrivateEventRouter::OnDangerousDownloadEvent(
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& threat_type,
    const std::string& mime_type,
    const std::string& scan_id,
    const int64_t content_size,
    safe_browsing::EventResult event_result) {
  absl::optional<enterprise_connectors::ReportingSettings> settings =
      GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(kKeyDangerousDownloadEvent) == 0) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUrl, url.spec());
  event.Set(kKeyFileName, file_name);
  event.Set(kKeyDownloadDigestSha256, download_digest_sha256);
  event.Set(kKeyProfileUserName, GetProfileUserName());
  event.Set(kKeyThreatType, threat_type);
  event.Set(kKeyClickedThrough, false);
  event.Set(kKeyContentType, mime_type);
  // |content_size| can be set to -1 to indicate an unknown size, in
  // which case the field is not set.
  if (content_size >= 0) {
    event.Set(kKeyContentSize, base::Int64ToValue(content_size));
  }
  event.Set(kKeyTrigger, kTriggerFileDownload);
  event.Set(kKeyEventResult, safe_browsing::EventResultToString(event_result));

  // The scan ID can be empty when the reported dangerous download is from a
  // Safe Browsing verdict.
  if (!scan_id.empty()) {
    event.Set(kKeyScanId, scan_id);
  }

  ReportRealtimeEvent(kKeyDangerousDownloadEvent, std::move(settings.value()),
                      std::move(event));
}

void SafeBrowsingPrivateEventRouter::OnDangerousDownloadWarningBypassed(
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const download::DownloadDangerType danger_type,
    const std::string& mime_type,
    const std::string& scan_id,
    const int64_t content_size) {
  OnDangerousDownloadWarningBypassed(url, file_name, download_digest_sha256,
                                     DangerTypeToThreatType(danger_type),
                                     mime_type, scan_id, content_size);
}

void SafeBrowsingPrivateEventRouter::OnDangerousDownloadWarningBypassed(
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& threat_type,
    const std::string& mime_type,
    const std::string& scan_id,
    const int64_t content_size) {
  absl::optional<enterprise_connectors::ReportingSettings> settings =
      GetReportingSettings();
  if (!settings.has_value() ||
      settings->enabled_event_names.count(kKeyDangerousDownloadEvent) == 0) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUrl, url.spec());
  event.Set(kKeyFileName, file_name);
  event.Set(kKeyDownloadDigestSha256, download_digest_sha256);
  event.Set(kKeyProfileUserName, GetProfileUserName());
  event.Set(kKeyThreatType, threat_type);
  event.Set(kKeyClickedThrough, true);
  event.Set(kKeyContentType, mime_type);
  // |content_size| can be set to -1 to indicate an unknown size, in
  // which case the field is not set.
  if (content_size >= 0) {
    event.Set(kKeyContentSize, base::Int64ToValue(content_size));
  }
  event.Set(kKeyTrigger, kTriggerFileDownload);
  event.Set(kKeyEventResult, safe_browsing::EventResultToString(
                                 safe_browsing::EventResult::BYPASSED));
  // The scan ID can be empty when the reported dangerous download is from a
  // Safe Browsing verdict.
  if (!scan_id.empty()) {
    event.Set(kKeyScanId, scan_id);
  }

  ReportRealtimeEvent(kKeyDangerousDownloadEvent, std::move(settings.value()),
                      std::move(event));
}

void SafeBrowsingPrivateEventRouter::OnLoginEvent(
    const GURL& url,
    bool is_federated,
    const url::Origin& federated_origin,
    const std::u16string& username) {
  absl::optional<enterprise_connectors::ReportingSettings> settings =
      GetReportingSettings();
  if (!settings.has_value()) {
    return;
  }

  std::unique_ptr<url_matcher::URLMatcher> matcher =
      CreateURLMatcherForOptInEvent(settings.value(), kKeyLoginEvent);
  if (!IsOptInEventEnabled(matcher.get(), url)) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUrl, url.spec());
  event.Set(kKeyIsFederated, is_federated);
  if (is_federated) {
    event.Set(kKeyFederatedOrigin, federated_origin.Serialize());
  }
  event.Set(kKeyProfileUserName, GetProfileUserName());
  event.Set(kKeyLoginUserName, MaskUsername(username));

  ReportRealtimeEvent(kKeyLoginEvent, std::move(settings.value()),
                      std::move(event));
}

void SafeBrowsingPrivateEventRouter::OnPasswordBreach(
    const std::string& trigger,
    const std::vector<std::pair<GURL, std::u16string>>& identities) {
  absl::optional<enterprise_connectors::ReportingSettings> settings =
      GetReportingSettings();
  if (!settings.has_value()) {
    return;
  }

  std::unique_ptr<url_matcher::URLMatcher> matcher =
      CreateURLMatcherForOptInEvent(settings.value(), kKeyPasswordBreachEvent);
  if (!matcher) {
    return;
  }

  base::Value::Dict event;
  base::Value::List identities_list;
  event.Set(kKeyTrigger, trigger);
  for (const std::pair<GURL, std::u16string>& i : identities) {
    if (!IsOptInEventEnabled(matcher.get(), i.first)) {
      continue;
    }

    base::Value::Dict identity;
    identity.Set(kKeyPasswordBreachIdentitiesUrl, i.first.spec());
    identity.Set(kKeyPasswordBreachIdentitiesUsername, i.second);
    identities_list.Append(std::move(identity));
  }

  if (identities_list.empty()) {
    // Don't send an empty event if none of the breached identities matched a
    // pattern in the URL filters.
    return;
  }

  event.Set(kKeyPasswordBreachIdentities, std::move(identities_list));
  event.Set(kKeyProfileUserName, GetProfileUserName());

  ReportRealtimeEvent(kKeyPasswordBreachEvent, std::move(settings.value()),
                      std::move(event));
}

// static
bool SafeBrowsingPrivateEventRouter::ShouldInitRealtimeReportingClient() {
  if (!base::FeatureList::IsEnabled(kRealtimeReportingFeature) &&
      !base::FeatureList::IsEnabled(
          enterprise_connectors::kEnterpriseConnectorsEnabled)) {
    DVLOG(2) << "Safe browsing real-time reporting is not enabled.";
    return false;
  }

  return true;
}

void SafeBrowsingPrivateEventRouter::SetBrowserCloudPolicyClientForTesting(
    policy::CloudPolicyClient* client) {
  if (client == nullptr && browser_client_)
    browser_client_->RemoveObserver(this);

  browser_client_ = client;
  if (browser_client_)
    browser_client_->AddObserver(this);
}

void SafeBrowsingPrivateEventRouter::SetProfileCloudPolicyClientForTesting(
    policy::CloudPolicyClient* client) {
  if (client == nullptr && profile_client_)
    profile_client_->RemoveObserver(this);

  profile_client_ = client;
  if (profile_client_)
    profile_client_->AddObserver(this);
}

void SafeBrowsingPrivateEventRouter::SetIdentityManagerForTesting(
    signin::IdentityManager* identity_manager) {
  identity_manager_ = identity_manager;
}

void SafeBrowsingPrivateEventRouter::InitRealtimeReportingClient(
    const enterprise_connectors::ReportingSettings& settings) {
  // If the corresponding client is already initialized, do nothing.
  if ((settings.per_profile &&
       IsClientValid(settings.dm_token, profile_client_)) ||
      (!settings.per_profile &&
       IsClientValid(settings.dm_token, browser_client_))) {
    DVLOG(2) << "Safe browsing real-time event reporting already initialized.";
    return;
  }

  if (!ShouldInitRealtimeReportingClient())
    return;

  // |identity_manager_| may be null in tests. If there is no identity
  // manager don't enable the real-time reporting API since the router won't
  // be able to fill in all the info needed for the reports.
  if (!identity_manager_) {
    DVLOG(2) << "Safe browsing real-time event requires an identity manager.";
    return;
  }

  policy::CloudPolicyClient* client = nullptr;
  std::string policy_client_desc;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::pair<std::string, policy::CloudPolicyClient*> desc_and_client =
      InitBrowserReportingClient(settings.dm_token);
#else
  std::pair<std::string, policy::CloudPolicyClient*> desc_and_client =
      settings.per_profile ? InitProfileReportingClient(settings.dm_token)
                           : InitBrowserReportingClient(settings.dm_token);
#endif
  if (!desc_and_client.second)
    return;
  policy_client_desc = std::move(desc_and_client.first);
  client = std::move(desc_and_client.second);

  OnCloudPolicyClientAvailable(policy_client_desc, client);
}

std::pair<std::string, policy::CloudPolicyClient*>
SafeBrowsingPrivateEventRouter::InitBrowserReportingClient(
    const std::string& dm_token) {
  // |device_management_service| may be null in tests. If there is no device
  // management service don't enable the real-time reporting API since the
  // router won't be able to create the reporting server client below.
  policy::DeviceManagementService* device_management_service =
      g_browser_process->browser_policy_connector()
          ->device_management_service();
  std::string policy_client_desc;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  policy_client_desc = kPolicyClientDescription;
#else
  policy_client_desc = kChromeBrowserCloudManagementClientDescription;
#endif
  if (!device_management_service) {
    DVLOG(2) << "Safe browsing real-time event requires a device management "
                "service.";
    return {policy_client_desc, nullptr};
  }

  policy::CloudPolicyClient* client = nullptr;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  const user_manager::User* user = GetChromeOSUser();
  if (user) {
    Profile* profile = ash::ProfileHelper::Get()->GetProfileByUser(user);
    // If primary user profile is not finalized, use the current profile.
    if (!profile)
      profile = Profile::FromBrowserContext(context_);
    DCHECK(profile);
    if (user->IsActiveDirectoryUser()) {
      // TODO(crbug.com/1012048): Handle AD, likely through crbug.com/1012170.
      policy_client_desc = kActiveDirectoryPolicyClientDescription;
    } else {
      policy_client_desc = kUserPolicyClientDescription;
      policy::UserCloudPolicyManagerAsh* policy_manager =
          profile->GetUserCloudPolicyManagerAsh();
      if (policy_manager)
        client = policy_manager->core()->client();
    }
  } else {
    LOG(ERROR) << "Could not determine who the user is.";
  }
#else
  std::string client_id =
      policy::BrowserDMTokenStorage::Get()->RetrieveClientId();

  // Make sure DeviceManagementService has been initialized.
  device_management_service->ScheduleInitialization(0);

  browser_private_client_ = std::make_unique<policy::CloudPolicyClient>(
      device_management_service, g_browser_process->shared_url_loader_factory(),
      policy::CloudPolicyClient::DeviceDMTokenCallback());
  client = browser_private_client_.get();

  // TODO(crbug.com/1069049): when we decide to add the extra URL parameters to
  // the uploaded reports, do the following:
  //     client->add_connector_url_params(base::FeatureList::IsEnabled(
  //        enterprise_connectors::kEnterpriseConnectorsEnabled));
  if (!client->is_registered()) {
    client->SetupRegistration(
        dm_token, client_id,
        /*user_affiliation_ids=*/std::vector<std::string>());
  }
#endif

  return {policy_client_desc, client};
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
std::pair<std::string, policy::CloudPolicyClient*>
SafeBrowsingPrivateEventRouter::InitProfileReportingClient(
    const std::string& dm_token) {
  policy::UserCloudPolicyManager* policy_manager =
      Profile::FromBrowserContext(context_)->GetUserCloudPolicyManager();
  if (!policy_manager || !policy_manager->core() ||
      !policy_manager->core()->client()) {
    return {kProfilePolicyClientDescription, nullptr};
  }

  profile_private_client_ = std::make_unique<policy::CloudPolicyClient>(
      policy_manager->core()->client()->service(),
      g_browser_process->shared_url_loader_factory(),
      policy::CloudPolicyClient::DeviceDMTokenCallback());
  policy::CloudPolicyClient* client = profile_private_client_.get();

  // TODO(crbug.com/1069049): when we decide to add the extra URL parameters to
  // the uploaded reports, do the following:
  //     client->add_connector_url_params(base::FeatureList::IsEnabled(
  //        enterprise_connectors::kEnterpriseConnectorsEnabled));

  client->SetupRegistration(dm_token,
                            policy_manager->core()->client()->client_id(),
                            /*user_affiliation_ids*/ {});

  return {kProfilePolicyClientDescription, client};
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

void SafeBrowsingPrivateEventRouter::OnCloudPolicyClientAvailable(
    const std::string& policy_client_desc,
    policy::CloudPolicyClient* client) {
  if (policy_client_desc == kProfilePolicyClientDescription)
    profile_client_ = client;
  else
    browser_client_ = client;

  if (client == nullptr) {
    LOG(ERROR) << "Could not obtain " << policy_client_desc
               << " for safe browsing real-time event reporting.";
    return;
  }

  client->AddObserver(this);

  VLOG(1) << "Ready for safe browsing real-time event reporting.";
}

absl::optional<enterprise_connectors::ReportingSettings>
SafeBrowsingPrivateEventRouter::GetReportingSettings() {
  return enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
             context_)
      ->GetReportingSettings(
          enterprise_connectors::ReportingConnector::SECURITY_EVENT);
}

void SafeBrowsingPrivateEventRouter::ReportRealtimeEvent(
    const std::string& name,
    const enterprise_connectors::ReportingSettings& settings,
    base::Value::Dict event) {
  if (rejected_dm_token_timers_.contains(settings.dm_token)) {
    return;
  }

#ifndef NDEBUG
  // Make sure that the event is included in the kAllEvents array.
  bool found = false;
  for (const char* known_event_name :
       extensions::SafeBrowsingPrivateEventRouter::kAllEvents) {
    if (name == known_event_name) {
      found = true;
      break;
    }
  }
  DCHECK(found);
#endif

  // Make sure real-time reporting is initialized.
  InitRealtimeReportingClient(settings);
  if ((settings.per_profile && !profile_client_) ||
      (!settings.per_profile && !browser_client_)) {
    return;
  }

  // Format the current time (UTC) in RFC3339 format.
  base::Time::Exploded now_exploded;
  base::Time::Now().UTCExplode(&now_exploded);
  std::string now_str = base::StringPrintf(
      "%d-%02d-%02dT%02d:%02d:%02d.%03dZ", now_exploded.year,
      now_exploded.month, now_exploded.day_of_month, now_exploded.hour,
      now_exploded.minute, now_exploded.second, now_exploded.millisecond);

  policy::CloudPolicyClient* client =
      settings.per_profile ? profile_client_.get() : browser_client_.get();
  base::Value::Dict wrapper;
  wrapper.Set("time", now_str);
  wrapper.Set(name, std::move(event));

  auto upload_callback = base::BindOnce(
      [](base::Value::Dict wrapper, bool per_profile, std::string dm_token,
         bool uploaded) {
        // Show the report on chrome://safe-browsing, if appropriate.
        wrapper.Set("uploaded_successfully", uploaded);
        wrapper.Set(per_profile ? "profile_dm_token" : "browser_dm_token",
                    std::move(dm_token));
        safe_browsing::WebUIInfoSingleton::GetInstance()->AddToReportingEvents(
            std::move(wrapper));
      },
      wrapper.Clone(), settings.per_profile, client->dm_token());

  base::Value::List event_list;
  event_list.Append(std::move(wrapper));

  Profile* profile = Profile::FromBrowserContext(context_);

  client->UploadSecurityEventReport(
      context_,
      enterprise_connectors::IncludeDeviceInfo(profile, settings.per_profile),
      policy::RealtimeReportingJobConfiguration::BuildReport(
          std::move(event_list),
          reporting::GetContext(Profile::FromBrowserContext(context_))),
      std::move(upload_callback));
}

std::string SafeBrowsingPrivateEventRouter::GetProfileUserName() const {
  return safe_browsing::GetProfileEmail(identity_manager_);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// static
const user_manager::User* SafeBrowsingPrivateEventRouter::GetChromeOSUser() {
  return user_manager::UserManager::IsInitialized()
             ? user_manager::UserManager::Get()->GetPrimaryUser()
             : nullptr;
}

#endif

void SafeBrowsingPrivateEventRouter::RemoveDmTokenFromRejectedSet(
    const std::string& dm_token) {
  rejected_dm_token_timers_.erase(dm_token);
}

void SafeBrowsingPrivateEventRouter::OnClientError(
    policy::CloudPolicyClient* client) {
  base::Value::Dict error_value;
  error_value.Set("error",
                  "An event got an error status and hasn't been reported");
  error_value.Set("status", client->status());
  safe_browsing::WebUIInfoSingleton::GetInstance()->AddToReportingEvents(
      error_value);

  // This is the status set when the server returned 403, which is what the
  // reporting server returns when the customer is not allowed to report events.
  if (client->status() == policy::DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED) {
    // This could happen if a second event was fired before the first one
    // returned an error.
    if (!rejected_dm_token_timers_.contains(client->dm_token())) {
      rejected_dm_token_timers_[client->dm_token()] =
          std::make_unique<base::OneShotTimer>();
      rejected_dm_token_timers_[client->dm_token()]->Start(
          FROM_HERE, base::Hours(24),
          base::BindOnce(
              &SafeBrowsingPrivateEventRouter::RemoveDmTokenFromRejectedSet,
              weak_ptr_factory_.GetWeakPtr(), client->dm_token()));
    }
  }
}

}  // namespace extensions
