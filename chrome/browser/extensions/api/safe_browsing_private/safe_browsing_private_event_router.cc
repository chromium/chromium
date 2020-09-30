// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"

#include "base/bind_helpers.h"
#include "build/build_config.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_manager.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/extensions/api/safe_browsing_private.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/proto/webprotect.pb.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#else
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#endif

namespace {

#if defined(OS_CHROMEOS)
const char kActiveDirectoryPolicyClientDescription[] = "an Active Directory";
const char kPolicyClientDescription[] = "any";
const char kUserPolicyClientDescription[] = "a user";
#else
const char kChromeBrowserCloudManagementClientDescription[] =
    "a machine-level user";
#endif

void AddAnalysisConnectorVerdictToEvent(
    const enterprise_connectors::ContentAnalysisResponse::Result& result,
    base::Value* event) {
  DCHECK(event);
  base::ListValue triggered_rule_info;
  for (const auto& trigger : result.triggered_rules()) {
    base::Value triggered_rule(base::Value::Type::DICTIONARY);
    triggered_rule.SetStringKey(
        extensions::SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleName,
        trigger.rule_name());

    triggered_rule_info.Append(std::move(triggered_rule));
  }
  event->SetKey(
      extensions::SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleInfo,
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
const char SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleInfo[] =
    "triggeredRuleInfo";
const char SafeBrowsingPrivateEventRouter::kKeyThreatType[] = "threatType";
const char SafeBrowsingPrivateEventRouter::kKeyContentType[] = "contentType";
const char SafeBrowsingPrivateEventRouter::kKeyContentSize[] = "contentSize";
const char SafeBrowsingPrivateEventRouter::kKeyTrigger[] = "trigger";
const char SafeBrowsingPrivateEventRouter::kKeyEventResult[] = "eventResult";

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
}

SafeBrowsingPrivateEventRouter::~SafeBrowsingPrivateEventRouter() {
  if (client_)
    client_->RemoveObserver(this);
}

void SafeBrowsingPrivateEventRouter::OnPolicySpecifiedPasswordReuseDetected(
    const GURL& url,
    const std::string& user_name,
    bool is_phishing_url) {
  api::safe_browsing_private::PolicySpecifiedPasswordReuse params;
  params.url = url.spec();
  params.user_name = user_name;
  params.is_phishing_url = is_phishing_url;

  // |event_router_| can be null in tests.
  if (event_router_) {
    auto event_value = std::make_unique<base::ListValue>();
    event_value->Append(params.ToValue());

    auto extension_event = std::make_unique<Event>(
        events::
            SAFE_BROWSING_PRIVATE_ON_POLICY_SPECIFIED_PASSWORD_REUSE_DETECTED,
        api::safe_browsing_private::OnPolicySpecifiedPasswordReuseDetected::
            kEventName,
        std::move(event_value));
    event_router_->BroadcastEvent(std::move(extension_event));
  }

  if (!IsRealtimeReportingEnabled())
    return;

  ReportRealtimeEvent(
      kKeyPasswordReuseEvent,
      base::BindOnce(
          [](const std::string& url, const std::string& user_name,
             const bool is_phishing_url, const std::string& profile_user_name) {
            // Convert |params| to a real-time event dictionary
            // and report it.
            base::Value event(base::Value::Type::DICTIONARY);
            event.SetStringKey(kKeyUrl, url);
            event.SetStringKey(kKeyUserName, user_name);
            event.SetBoolKey(kKeyIsPhishingUrl, is_phishing_url);
            event.SetStringKey(kKeyProfileUserName, profile_user_name);
            return event;
          },
          params.url, params.user_name, params.is_phishing_url,
          GetProfileUserName()));
}

void SafeBrowsingPrivateEventRouter::OnPolicySpecifiedPasswordChanged(
    const std::string& user_name) {
  // |event_router_| can be null in tests.
  if (event_router_) {
    auto event_value = std::make_unique<base::ListValue>();
    event_value->Append(std::make_unique<base::Value>(user_name));
    auto extension_event = std::make_unique<Event>(
        events::SAFE_BROWSING_PRIVATE_ON_POLICY_SPECIFIED_PASSWORD_CHANGED,
        api::safe_browsing_private::OnPolicySpecifiedPasswordChanged::
            kEventName,
        std::move(event_value));
    event_router_->BroadcastEvent(std::move(extension_event));
  }

  if (!IsRealtimeReportingEnabled())
    return;

  ReportRealtimeEvent(kKeyPasswordChangedEvent,
                      base::BindOnce(
                          [](const std::string& user_name,
                             const std::string& profile_user_name) {
                            // Convert |params| to a real-time event dictionary
                            // and report it.
                            base::Value event(base::Value::Type::DICTIONARY);
                            event.SetStringKey(kKeyUserName, user_name);
                            event.SetStringKey(kKeyProfileUserName,
                                               profile_user_name);
                            return event;
                          },
                          user_name, GetProfileUserName()));
}

void SafeBrowsingPrivateEventRouter::OnDangerousDownloadOpened(
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const int64_t content_size) {
  api::safe_browsing_private::DangerousDownloadInfo params;
  params.url = url.spec();
  params.file_name = file_name;
  params.download_digest_sha256 = download_digest_sha256;
  params.user_name = GetProfileUserName();

  // |event_router_| can be null in tests.
  if (event_router_) {
    auto event_value = std::make_unique<base::ListValue>();
    event_value->Append(params.ToValue());

    auto extension_event = std::make_unique<Event>(
        events::SAFE_BROWSING_PRIVATE_ON_DANGEROUS_DOWNLOAD_OPENED,
        api::safe_browsing_private::OnDangerousDownloadOpened::kEventName,
        std::move(event_value));
    event_router_->BroadcastEvent(std::move(extension_event));
  }

  if (!IsRealtimeReportingEnabled())
    return;

  ReportRealtimeEvent(
      kKeyDangerousDownloadEvent,
      base::BindOnce(
          [](const std::string& url, const std::string& file_name,
             const std::string& download_digest_sha256,
             const std::string& user_name, const std::string& mime_type,
             const int64_t content_size) {
            // Convert |params| to a real-time event dictionary and report it.
            base::Value event(base::Value::Type::DICTIONARY);
            event.SetStringKey(kKeyUrl, url);
            event.SetStringKey(kKeyFileName, file_name);
            event.SetStringKey(kKeyDownloadDigestSha256,
                               download_digest_sha256);
            event.SetStringKey(kKeyProfileUserName, user_name);
            event.SetStringKey(kKeyContentType, mime_type);
            // |content_size| can be set to -1 to indicate an unknown size, in
            // which case the field is not set.
            if (content_size >= 0)
              event.SetIntKey(kKeyContentSize, content_size);
            event.SetStringKey(kKeyTrigger, kTriggerFileDownload);
            event.SetStringKey(kKeyEventResult,
                               safe_browsing::EventResultToString(
                                   safe_browsing::EventResult::BYPASSED));
            event.SetBoolKey(kKeyClickedThrough, true);
            return event;
          },
          params.url, params.file_name, params.download_digest_sha256,
          params.user_name, mime_type, content_size));
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
    auto event_value = std::make_unique<base::ListValue>();
    event_value->Append(params.ToValue());

    auto extension_event = std::make_unique<Event>(
        events::SAFE_BROWSING_PRIVATE_ON_SECURITY_INTERSTITIAL_SHOWN,
        api::safe_browsing_private::OnSecurityInterstitialShown::kEventName,
        std::move(event_value));
    event_router_->BroadcastEvent(std::move(extension_event));
  }

  if (!IsRealtimeReportingEnabled())
    return;

  PrefService* prefs = Profile::FromBrowserContext(context_)->GetPrefs();
  auto event_result =
      prefs->GetBoolean(prefs::kSafeBrowsingProceedAnywayDisabled)
          ? safe_browsing::EventResult::BLOCKED
          : safe_browsing::EventResult::WARNED;
  ReportRealtimeEvent(
      kKeyInterstitialEvent,
      base::BindOnce(
          [](const std::string& url, const std::string& reason,
             int net_error_code, const std::string& user_name,
             safe_browsing::EventResult event_result) {
            // Convert |params| to a real-time event dictionary and report it.
            base::Value event(base::Value::Type::DICTIONARY);
            event.SetStringKey(kKeyUrl, url);
            event.SetStringKey(kKeyReason, reason);
            event.SetIntKey(kKeyNetErrorCode, net_error_code);
            event.SetStringKey(kKeyProfileUserName, user_name);
            event.SetBoolKey(kKeyClickedThrough, false);
            event.SetStringKey(
                kKeyEventResult,
                safe_browsing::EventResultToString(event_result));

            return event;
          },
          params.url, params.reason, net_error_code, params.user_name,
          event_result));
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
    auto event_value = std::make_unique<base::ListValue>();
    event_value->Append(params.ToValue());

    auto extension_event = std::make_unique<Event>(
        events::SAFE_BROWSING_PRIVATE_ON_SECURITY_INTERSTITIAL_PROCEEDED,
        api::safe_browsing_private::OnSecurityInterstitialProceeded::kEventName,
        std::move(event_value));
    event_router_->BroadcastEvent(std::move(extension_event));
  }

  if (!IsRealtimeReportingEnabled())
    return;

  ReportRealtimeEvent(
      kKeyInterstitialEvent,
      base::BindOnce(
          [](const std::string& url, const std::string& reason,
             int net_error_code, const std::string& user_name) {
            // Convert |params| to a real-time event dictionary and report it.
            base::Value event(base::Value::Type::DICTIONARY);
            event.SetStringKey(kKeyUrl, url);
            event.SetStringKey(kKeyReason, reason);
            event.SetIntKey(kKeyNetErrorCode, net_error_code);
            event.SetStringKey(kKeyProfileUserName, user_name);
            event.SetBoolKey(kKeyClickedThrough, true);
            event.SetStringKey(kKeyEventResult,
                               safe_browsing::EventResultToString(
                                   safe_browsing::EventResult::BYPASSED));
            return event;
          },
          params.url, params.reason, net_error_code, params.user_name));
}

void SafeBrowsingPrivateEventRouter::OnAnalysisConnectorResult(
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    safe_browsing::DeepScanAccessPoint /* access_point */,
    const enterprise_connectors::ContentAnalysisResponse::Result& result,
    const int64_t content_size,
    safe_browsing::EventResult event_result) {
  if (!IsRealtimeReportingEnabled())
    return;

  if (result.tag() == "malware") {
    DCHECK_EQ(1, result.triggered_rules().size());
    OnDangerousDeepScanningResult(
        url, file_name, download_digest_sha256,
        MalwareRuleToThreatType(result.triggered_rules(0).rule_name()),
        mime_type, trigger, content_size, event_result);
  } else if (result.tag() == "dlp") {
    OnSensitiveDataEvent(url, file_name, download_digest_sha256, mime_type,
                         trigger, result, content_size, event_result);
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
    safe_browsing::EventResult event_result) {
  if (!IsRealtimeReportingEnabled())
    return;

  ReportRealtimeEvent(
      kKeyDangerousDownloadEvent,
      base::BindOnce(
          [](const std::string& url, const std::string& file_name,
             const std::string& download_digest_sha256,
             const std::string& profile_user_name,
             const std::string& threat_type, const std::string& mime_type,
             const std::string& trigger, const int64_t content_size,
             safe_browsing::EventResult event_result) {
            // Create a real-time event dictionary from the arguments and
            // report it.
            base::Value event(base::Value::Type::DICTIONARY);
            event.SetStringKey(kKeyUrl, url);
            event.SetStringKey(kKeyFileName, file_name);
            event.SetStringKey(kKeyDownloadDigestSha256,
                               download_digest_sha256);
            event.SetStringKey(kKeyProfileUserName, profile_user_name);
            event.SetStringKey(kKeyThreatType, threat_type);
            event.SetStringKey(kKeyContentType, mime_type);
            // |content_size| can be set to -1 to indicate an unknown size, in
            // which case the field is not set.
            if (content_size >= 0)
              event.SetIntKey(kKeyContentSize, content_size);
            event.SetStringKey(kKeyTrigger, trigger);
            event.SetStringKey(
                kKeyEventResult,
                safe_browsing::EventResultToString(event_result));
            event.SetBoolKey(
                kKeyClickedThrough,
                event_result == safe_browsing::EventResult::BYPASSED);
            return event;
          },
          url.spec(), file_name, download_digest_sha256, GetProfileUserName(),
          threat_type, mime_type, trigger, content_size, event_result));
}

void SafeBrowsingPrivateEventRouter::OnSensitiveDataEvent(
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    const enterprise_connectors::ContentAnalysisResponse::Result& result,
    const int64_t content_size,
    safe_browsing::EventResult event_result) {
  if (!IsRealtimeReportingEnabled())
    return;

  ReportRealtimeEvent(
      kKeySensitiveDataEvent,
      base::BindOnce(
          [](const enterprise_connectors::ContentAnalysisResponse::Result&
                 result,
             const std::string& url, const std::string& file_name,
             const std::string& download_digest_sha256,
             const std::string& profile_user_name, const std::string& mime_type,
             const std::string& trigger, const int64_t content_size,
             safe_browsing::EventResult event_result) {
            // Create a real-time event dictionary from the arguments and
            // report it.
            base::Value event(base::Value::Type::DICTIONARY);
            event.SetStringKey(kKeyUrl, url);
            event.SetStringKey(kKeyFileName, file_name);
            event.SetStringKey(kKeyDownloadDigestSha256,
                               download_digest_sha256);
            event.SetStringKey(kKeyProfileUserName, profile_user_name);
            event.SetStringKey(kKeyContentType, mime_type);
            // |content_size| can be set to -1 to indicate an unknown size, in
            // which case the field is not set.
            if (content_size >= 0)
              event.SetIntKey(kKeyContentSize, content_size);
            event.SetStringKey(kKeyTrigger, trigger);
            event.SetStringKey(
                kKeyEventResult,
                safe_browsing::EventResultToString(event_result));
            event.SetBoolKey(
                kKeyClickedThrough,
                event_result == safe_browsing::EventResult::BYPASSED);

            AddAnalysisConnectorVerdictToEvent(result, &event);

            return event;
          },
          result, url.spec(), file_name, download_digest_sha256,
          GetProfileUserName(), mime_type, trigger, content_size,
          event_result));
}

void SafeBrowsingPrivateEventRouter::OnAnalysisConnectorWarningBypassed(
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    safe_browsing::DeepScanAccessPoint access_point,
    const enterprise_connectors::ContentAnalysisResponse::Result& result,
    const int64_t content_size) {
  if (!IsRealtimeReportingEnabled())
    return;

  ReportRealtimeEvent(
      kKeySensitiveDataEvent,
      base::BindOnce(
          [](const enterprise_connectors::ContentAnalysisResponse::Result&
                 result,
             const std::string& url, const std::string& file_name,
             const std::string& download_digest_sha256,
             const std::string& profile_user_name, const std::string& mime_type,
             const std::string& trigger,
             safe_browsing::DeepScanAccessPoint /* access_point */,
             const int64_t content_size) {
            // Create a real-time event dictionary from the arguments and
            // report it.
            base::Value event(base::Value::Type::DICTIONARY);
            event.SetStringKey(kKeyUrl, url);
            event.SetStringKey(kKeyFileName, file_name);
            event.SetStringKey(kKeyDownloadDigestSha256,
                               download_digest_sha256);
            event.SetStringKey(kKeyProfileUserName, profile_user_name);
            event.SetStringKey(kKeyContentType, mime_type);
            // |content_size| can be set to -1 to indicate an unknown size, in
            // which case the field is not set.
            if (content_size >= 0)
              event.SetIntKey(kKeyContentSize, content_size);
            event.SetStringKey(kKeyTrigger, trigger);
            event.SetStringKey(kKeyEventResult,
                               safe_browsing::EventResultToString(
                                   safe_browsing::EventResult::BYPASSED));
            event.SetBoolKey(kKeyClickedThrough, true);

            AddAnalysisConnectorVerdictToEvent(result, &event);

            return event;
          },
          result, url.spec(), file_name, download_digest_sha256,
          GetProfileUserName(), mime_type, trigger, access_point,
          content_size));
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
  if (!IsRealtimeReportingEnabled())
    return;

  ReportRealtimeEvent(
      kKeyUnscannedFileEvent,
      base::BindOnce(
          [](const std::string& url, const std::string& file_name,
             const std::string& download_digest_sha256,
             const std::string& profile_user_name, const std::string& mime_type,
             const std::string& trigger,
             safe_browsing::DeepScanAccessPoint access_point,
             const std::string& reason, const int64_t content_size,
             safe_browsing::EventResult event_result) {
            // Create a real-time event dictionary from the arguments and
            // report it.
            base::Value event(base::Value::Type::DICTIONARY);
            event.SetStringKey(kKeyUrl, url);
            event.SetStringKey(kKeyFileName, file_name);
            event.SetStringKey(kKeyDownloadDigestSha256,
                               download_digest_sha256);
            event.SetStringKey(kKeyProfileUserName, profile_user_name);
            event.SetStringKey(kKeyContentType, mime_type);
            event.SetStringKey(kKeyUnscannedReason, reason);
            // |content_size| can be set to -1 to indicate an unknown size, in
            // which case the field is not set.
            if (content_size >= 0)
              event.SetIntKey(kKeyContentSize, content_size);
            event.SetStringKey(kKeyTrigger, trigger);
            event.SetStringKey(
                kKeyEventResult,
                safe_browsing::EventResultToString(event_result));
            event.SetBoolKey(
                kKeyClickedThrough,
                event_result == safe_browsing::EventResult::BYPASSED);
            return event;
          },
          url.spec(), file_name, download_digest_sha256, GetProfileUserName(),
          mime_type, trigger, access_point, reason, content_size,
          event_result));
}

void SafeBrowsingPrivateEventRouter::OnDangerousDownloadEvent(
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& threat_type,
    const std::string& mime_type,
    const int64_t content_size,
    safe_browsing::EventResult event_result) {
  if (!IsRealtimeReportingEnabled())
    return;

  ReportRealtimeEvent(
      kKeyDangerousDownloadEvent,
      base::BindOnce(
          [](const std::string& url, const std::string& file_name,
             const std::string& download_digest_sha256,
             const std::string& profile_user_name,
             const std::string& threat_type, const std::string& mime_type,
             const int64_t content_size,
             safe_browsing::EventResult event_result) {
            // Create a real-time event dictionary and report it.
            base::Value event(base::Value::Type::DICTIONARY);
            event.SetStringKey(kKeyUrl, url);
            event.SetStringKey(kKeyFileName, file_name);
            event.SetStringKey(kKeyDownloadDigestSha256,
                               download_digest_sha256);
            event.SetStringKey(kKeyProfileUserName, profile_user_name);
            event.SetStringKey(kKeyThreatType, threat_type);
            event.SetBoolKey(kKeyClickedThrough, false);
            event.SetStringKey(kKeyContentType, mime_type);
            // |content_size| can be set to -1 to indicate an unknown size, in
            // which case the field is not set.
            if (content_size >= 0)
              event.SetIntKey(kKeyContentSize, content_size);
            event.SetStringKey(kKeyTrigger, kTriggerFileDownload);
            event.SetStringKey(
                kKeyEventResult,
                safe_browsing::EventResultToString(event_result));
            return event;
          },
          url.spec(), file_name, download_digest_sha256, GetProfileUserName(),
          threat_type, mime_type, content_size, event_result));
}

void SafeBrowsingPrivateEventRouter::OnDangerousDownloadWarningBypassed(
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& threat_type,
    const std::string& mime_type,
    const int64_t content_size) {
  if (!IsRealtimeReportingEnabled())
    return;

  ReportRealtimeEvent(
      kKeyDangerousDownloadEvent,
      base::BindOnce(
          [](const std::string& url, const std::string& file_name,
             const std::string& download_digest_sha256,
             const std::string& profile_user_name,
             const std::string& threat_type, const std::string& mime_type,
             const int64_t content_size) {
            // Create a real-time event dictionary and report it.
            base::Value event(base::Value::Type::DICTIONARY);
            event.SetStringKey(kKeyUrl, url);
            event.SetStringKey(kKeyFileName, file_name);
            event.SetStringKey(kKeyDownloadDigestSha256,
                               download_digest_sha256);
            event.SetStringKey(kKeyProfileUserName, profile_user_name);
            event.SetStringKey(kKeyThreatType, threat_type);
            event.SetBoolKey(kKeyClickedThrough, true);
            event.SetStringKey(kKeyContentType, mime_type);
            // |content_size| can be set to -1 to indicate an unknown size, in
            // which case the field is not set.
            if (content_size >= 0)
              event.SetIntKey(kKeyContentSize, content_size);
            event.SetStringKey(kKeyTrigger, kTriggerFileDownload);
            event.SetStringKey(kKeyEventResult,
                               safe_browsing::EventResultToString(
                                   safe_browsing::EventResult::BYPASSED));
            return event;
          },
          url.spec(), file_name, download_digest_sha256, GetProfileUserName(),
          threat_type, mime_type, content_size));
}

// static
bool SafeBrowsingPrivateEventRouter::ShouldInitRealtimeReportingClient() {
  if (!base::FeatureList::IsEnabled(kRealtimeReportingFeature) &&
      !base::FeatureList::IsEnabled(
          enterprise_connectors::kEnterpriseConnectorsEnabled)) {
    DVLOG(2) << "Safe browsing real-time reporting is not enabled.";
    return false;
  }

  if (!IsRealtimeReportingAvailable()) {
    DVLOG(1) << "Safe browsing real-time event reporting is only available for "
                "managed browsers or devices.";
    return false;
  }
  return true;
}

void SafeBrowsingPrivateEventRouter::SetCloudPolicyClientForTesting(
    policy::CloudPolicyClient* client) {
  client_ = client;
}

void SafeBrowsingPrivateEventRouter::SetBinaryUploadServiceForTesting(
    safe_browsing::BinaryUploadService* binary_upload_service) {
  binary_upload_service_ = binary_upload_service;
}

void SafeBrowsingPrivateEventRouter::SetIdentityManagerForTesting(
    signin::IdentityManager* identity_manager) {
  identity_manager_ = identity_manager;
}

void SafeBrowsingPrivateEventRouter::InitRealtimeReportingClient() {
  // If already initialized, do nothing.
  if (client_) {
    DVLOG(2) << "Safe browsing real-time event reporting already initialized.";
    return;
  }

  if (!ShouldInitRealtimeReportingClient())
    return;

  // |identity_manager_| may be null in tests. If there is no identity
  // manager don't enable the real-time reporting API since the router won't
  // be able to fill in all the info needed for the reports.
  identity_manager_ = IdentityManagerFactory::GetForProfile(
      Profile::FromBrowserContext(context_));
  if (!identity_manager_) {
    DVLOG(2) << "Safe browsing real-time event requires an identity manager.";
    return;
  }

  // |device_management_service| may be null in tests. If there is no device
  // management service don't enable the real-time reporting API since the
  // router won't be able to create the reporting server client below.
  policy::DeviceManagementService* device_management_service =
      g_browser_process->browser_policy_connector()
          ->device_management_service();
  if (!device_management_service) {
    DVLOG(2) << "Safe browsing real-time event requires a device management "
                "service.";
    return;
  }

  policy::CloudPolicyClient* client = nullptr;
  std::string policy_client_desc;

#if defined(OS_CHROMEOS)
  policy_client_desc = kPolicyClientDescription;
  auto* user = GetChromeOSUser();
  if (user) {
    auto* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(user);
    // If primary user profile is not finalized, use the current profile.
    if (!profile)
      profile = Profile::FromBrowserContext(context_);
    DCHECK(profile);
    if (user->IsActiveDirectoryUser()) {
      // TODO(crbug.com/1012048): Handle AD, likely through crbug.com/1012170.
      policy_client_desc = kActiveDirectoryPolicyClientDescription;
    } else {
      policy_client_desc = kUserPolicyClientDescription;
      auto* policy_manager = profile->GetUserCloudPolicyManagerChromeOS();
      if (policy_manager)
        client = policy_manager->core()->client();
    }
  } else {
    LOG(ERROR) << "Could not determine who the user is.";
  }
#else
  policy_client_desc = kChromeBrowserCloudManagementClientDescription;
  // Make sure we have a DM token to proceed.  During the lifetime of a running
  // chrome browser, this can only change from empty to non-empty.  There are
  // no cases where chrome starts with a dm token and then it goes away.
  // When chrome starts without a dm token and determines that one is needed,
  // browser startup is blocked until it is retrieved or an error occurs.  In
  // the latter case, chrome won't try to retrieve it again until the next
  // restart.
  //
  // Therefore, it is OK to retrieve the dm token once here on initialization
  // of the router to determine if real-time reporting can be enabled or not.
  policy::DMToken dm_token =
      policy::BrowserDMTokenStorage::Get()->RetrieveDMToken();
  std::string client_id =
      policy::BrowserDMTokenStorage::Get()->RetrieveClientId();

  if (!dm_token.is_valid())
    return;

  // Make sure DeviceManagementService has been initialized.
  device_management_service->ScheduleInitialization(0);

  private_client_ = std::make_unique<policy::CloudPolicyClient>(
      device_management_service, g_browser_process->shared_url_loader_factory(),
      policy::CloudPolicyClient::DeviceDMTokenCallback());
  client = private_client_.get();

  // TODO(crbug.com/1069049): when we decide to add the extra URL parameters to
  // the uploaded reports, do the following:
  //     client->add_connector_url_params(base::FeatureList::IsEnabled(
  //        enterprise_connectors::kEnterpriseConnectorsEnabled));

  if (!client->is_registered()) {
    client->SetupRegistration(
        dm_token.value(), client_id,
        /*user_affiliation_ids=*/std::vector<std::string>());
  }
#endif

  OnCloudPolicyClientAvailable(policy_client_desc, client);
}

void SafeBrowsingPrivateEventRouter::OnCloudPolicyClientAvailable(
    const std::string& policy_client_desc,
    policy::CloudPolicyClient* client) {
  client_ = client;
  if (client_ == nullptr) {
    LOG(ERROR) << "Could not obtain " << policy_client_desc
               << " for safe browsing real-time event reporting.";
    return;
  }

  client_->AddObserver(this);

  VLOG(1) << "Ready for safe browsing real-time event reporting.";
}

bool SafeBrowsingPrivateEventRouter::IsRealtimeReportingEnabled() {
  auto settings =
      enterprise_connectors::ConnectorsManager::GetInstance()
          ->GetReportingSettings(
              enterprise_connectors::ReportingConnector::SECURITY_EVENT);
  return settings.has_value();
}

void SafeBrowsingPrivateEventRouter::IfAuthorized(
    base::OnceCallback<void(bool)> cont) {
  if (!binary_upload_service_ && g_browser_process) {
    binary_upload_service_ =
        safe_browsing::BinaryUploadServiceFactory::GetForProfile(
            Profile::FromBrowserContext(context_));
  }

  // TODO(crbug/1069049): Use reporting URL.
  if (binary_upload_service_)
    binary_upload_service_->IsAuthorized(GURL(), std::move(cont));
}

void SafeBrowsingPrivateEventRouter::ReportRealtimeEvent(
    const std::string& name,
    EventBuilder event_builder) {
  IfAuthorized(base::BindOnce(
      &SafeBrowsingPrivateEventRouter::ReportRealtimeEventCallback,
      weak_ptr_factory_.GetWeakPtr(), name, std::move(event_builder)));
}

void SafeBrowsingPrivateEventRouter::ReportRealtimeEventCallback(
    const std::string& name,
    EventBuilder event_builder,
    bool authorized) {
  // Ignore the event if we know we can't report it.
  if (!authorized) {
    DVLOG(2) << "Safe browsing real-time reporting is not authorized.";
    return;
  }

  // Make sure real-time reporting is initialized.
  InitRealtimeReportingClient();
  if (!client_)
    return;

  // Format the current time (UTC) in RFC3339 format.
  base::Time::Exploded now_exploded;
  base::Time::Now().UTCExplode(&now_exploded);
  std::string now_str = base::StringPrintf(
      "%d-%02d-%02dT%02d:%02d:%02d.%03dZ", now_exploded.year,
      now_exploded.month, now_exploded.day_of_month, now_exploded.hour,
      now_exploded.minute, now_exploded.second, now_exploded.millisecond);

  base::Value wrapper(base::Value::Type::DICTIONARY);
  wrapper.SetStringKey("time", now_str);
  wrapper.SetKey(name, std::move(event_builder).Run());

  auto upload_callback = base::BindOnce(
      [](base::Value wrapper, bool uploaded) {
        // Show the report on chrome://safe-browsing, if appropriate.
        wrapper.SetBoolKey("uploaded_successfully", uploaded);
        safe_browsing::WebUIInfoSingleton::GetInstance()->AddToReportingEvents(
            wrapper);
      },
      wrapper.Clone());

  base::Value event_list(base::Value::Type::LIST);
  event_list.Append(std::move(wrapper));

  client_->UploadRealtimeReport(
      policy::RealtimeReportingJobConfiguration::BuildReport(
          std::move(event_list),
          reporting::GetContext(Profile::FromBrowserContext(context_))),
      std::move(upload_callback));
}

std::string SafeBrowsingPrivateEventRouter::GetProfileUserName() const {
  return safe_browsing::GetProfileEmail(identity_manager_);
}

#if defined(OS_CHROMEOS)
// static
const user_manager::User* SafeBrowsingPrivateEventRouter::GetChromeOSUser() {
  return user_manager::UserManager::IsInitialized()
             ? user_manager::UserManager::Get()->GetPrimaryUser()
             : nullptr;
}

#endif

bool SafeBrowsingPrivateEventRouter::IsRealtimeReportingAvailable() {
#if defined(OS_CHROMEOS)
  // The device must be managed.
  if (!g_browser_process->platform_part()
           ->browser_policy_connector_chromeos()
           ->IsEnterpriseManaged())
    return false;

  // The Chrome OS user must be affiliated with the device.
  // This also implies that the user is managed.
  auto* user = GetChromeOSUser();
  return user && user->IsAffiliated();
#else
  return g_browser_process->browser_policy_connector()
      ->chrome_browser_cloud_management_controller()
      ->IsEnabled();
#endif
}

void SafeBrowsingPrivateEventRouter::OnClientError(
    policy::CloudPolicyClient* client) {
  base::Value error_value(base::Value::Type::DICTIONARY);
  error_value.SetStringKey(
      "error", "An event got an error status and hasn't been reported");
  error_value.SetIntKey("status", client->status());
  safe_browsing::WebUIInfoSingleton::GetInstance()->AddToReportingEvents(
      error_value);
}

}  // namespace extensions
