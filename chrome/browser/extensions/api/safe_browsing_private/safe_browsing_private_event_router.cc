// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"

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
#include "chrome/browser/policy/browser_dm_token_storage.h"
#include "chrome/browser/policy/chrome_browser_cloud_management_controller.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "chrome/browser/safe_browsing/download_protection/binary_upload_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/extensions/api/safe_browsing_private.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/proto/webprotect.pb.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

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
const char SafeBrowsingPrivateEventRouter::kKeyTriggeredRules[] =
    "triggeredRules";
const char SafeBrowsingPrivateEventRouter::kKeyThreatType[] = "threatType";
const char SafeBrowsingPrivateEventRouter::kKeyContentType[] = "contentType";
const char SafeBrowsingPrivateEventRouter::kKeyContentSize[] = "contentSize";
const char SafeBrowsingPrivateEventRouter::kKeyTrigger[] = "trigger";

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
const char SafeBrowsingPrivateEventRouter::kKeyLargeUnscannedFileEvent[] =
    "largeUnscannedFileEvent";

const char SafeBrowsingPrivateEventRouter::kTriggerFileDownload[] =
    "FILE_DOWNLOAD";
const char SafeBrowsingPrivateEventRouter::kTriggerFileUpload[] = "FILE_UPLOAD";
const char SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload[] =
    "WEB_CONTENT_UPLOAD";

SafeBrowsingPrivateEventRouter::SafeBrowsingPrivateEventRouter(
    content::BrowserContext* context)
    : context_(context) {
  event_router_ = EventRouter::Get(context_);

  // g_browser_process and/or g_browser_process->local_state() may be null
  // in tests.
  if (g_browser_process && g_browser_process->local_state()) {
    RealtimeReportingPrefChanged(std::string());
    registrar_.Init(g_browser_process->local_state());
    registrar_.Add(
        prefs::kUnsafeEventsReportingEnabled,
        base::BindRepeating(
            &SafeBrowsingPrivateEventRouter::RealtimeReportingPrefChanged,
            base::Unretained(this)));
  }
}

SafeBrowsingPrivateEventRouter::~SafeBrowsingPrivateEventRouter() {}

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
            event.SetBoolKey(kKeyClickedThrough, false);
            return event;
          },
          params.url, params.reason, net_error_code, params.user_name));
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
            return event;
          },
          params.url, params.reason, net_error_code, params.user_name));
}

void SafeBrowsingPrivateEventRouter::OnDangerousDeepScanningResult(
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& threat_type,
    const std::string& mime_type,
    const std::string& trigger,
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
             const std::string& trigger, const int64_t content_size) {
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
            return event;
          },
          url.spec(), file_name, download_digest_sha256, GetProfileUserName(),
          threat_type, mime_type, trigger, content_size));
}

void SafeBrowsingPrivateEventRouter::OnSensitiveDataEvent(
    const safe_browsing::DlpDeepScanningVerdict& verdict,
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    const int64_t content_size) {
  if (!IsRealtimeReportingEnabled())
    return;

  ReportRealtimeEvent(
      kKeySensitiveDataEvent,
      base::BindOnce(
          [](const safe_browsing::DlpDeepScanningVerdict& verdict,
             const std::string& url, const std::string& file_name,
             const std::string& download_digest_sha256,
             const std::string& profile_user_name, const std::string& mime_type,
             const std::string& trigger, const int64_t content_size) {
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

            base::ListValue triggered_rules;
            for (auto rule : verdict.triggered_rules()) {
              triggered_rules.AppendString(rule.rule_name());
            }
            event.SetKey(kKeyTriggeredRules, std::move(triggered_rules));
            return event;
          },
          verdict, url.spec(), file_name, download_digest_sha256,
          GetProfileUserName(), mime_type, trigger, content_size));
}

void SafeBrowsingPrivateEventRouter::OnLargeUnscannedFileEvent(
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    const int64_t content_size) {
  if (!IsRealtimeReportingEnabled())
    return;

  ReportRealtimeEvent(
      kKeyLargeUnscannedFileEvent,
      base::BindOnce(
          [](const std::string& url, const std::string& file_name,
             const std::string& download_digest_sha256,
             const std::string& profile_user_name, const std::string& mime_type,
             const std::string& trigger, const int64_t content_size) {
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
            return event;
          },
          url.spec(), file_name, download_digest_sha256, GetProfileUserName(),
          mime_type, trigger, content_size));
}

void SafeBrowsingPrivateEventRouter::OnDangerousDownloadWarning(
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
            event.SetBoolKey(kKeyClickedThrough, false);
            event.SetStringKey(kKeyContentType, mime_type);
            // |content_size| can be set to -1 to indicate an unknown size, in
            // which case the field is not set.
            if (content_size >= 0)
              event.SetIntKey(kKeyContentSize, content_size);
            event.SetStringKey(kKeyTrigger, kTriggerFileDownload);
            return event;
          },
          url.spec(), file_name, download_digest_sha256, GetProfileUserName(),
          threat_type, mime_type, content_size));
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
            return event;
          },
          url.spec(), file_name, download_digest_sha256, GetProfileUserName(),
          threat_type, mime_type, content_size));
}

bool SafeBrowsingPrivateEventRouter::ShouldInitRealtimeReportingClient() {
  // This method is not compiled on Chrome OS because
  // ChromeBrowserCloudManagementController does not exist. Once this is
  // fixed the #if !defined can be removed.
#if !defined(OS_CHROMEOS)
  if (!base::FeatureList::IsEnabled(kRealtimeReportingFeature))
    return false;

  if (!policy::ChromeBrowserCloudManagementController::IsEnabled())
    return false;

  return true;
#else
  return false;
#endif
}

void SafeBrowsingPrivateEventRouter::SetCloudPolicyClientForTesting(
    std::unique_ptr<policy::CloudPolicyClient> client) {
  DCHECK_EQ(nullptr, client_.get());
  client_ = std::move(client);
}

void SafeBrowsingPrivateEventRouter::InitRealtimeReportingClient() {
  // If already initialized, do nothing.
  if (client_)
    return;

  if (!ShouldInitRealtimeReportingClient())
    return;

  // |identity_manager_| may be null in tests.  If there is no identity
  // manager don't enable the real-time reporting API since the router won't
  // be able to fill in all the info needed for the reports.
  identity_manager_ = IdentityManagerFactory::GetForProfile(
      Profile::FromBrowserContext(context_));
  if (!identity_manager_)
    return;

  // |device_management_service| may be null in tests.  If there is no device
  // management service don't enable the real-time reporting API since the
  // router won't be able to create the reporting server client below.
  policy::DeviceManagementService* device_management_service =
      g_browser_process->browser_policy_connector()
          ->device_management_service();
  if (!device_management_service)
    return;

  if (g_browser_process) {
    binary_upload_service_ =
        g_browser_process->safe_browsing_service()->GetBinaryUploadService(
            Profile::FromBrowserContext(context_));
    binary_upload_service_->IsAuthorized(base::BindOnce(
        &SafeBrowsingPrivateEventRouter::InitRealtimeReportingClientCallback,
        weakptr_factory_.GetWeakPtr(), device_management_service));
  }
}

void SafeBrowsingPrivateEventRouter::InitRealtimeReportingClientCallback(
    policy::DeviceManagementService* device_management_service,
    bool authorized) {
#if !defined(OS_CHROMEOS)
  // Don't initialize the client if the browser cannot upload data.
  if (!authorized)
    return;

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
      policy::BrowserDMTokenStorage::Get()->RetrieveBrowserDMToken();
  std::string client_id =
      policy::BrowserDMTokenStorage::Get()->RetrieveClientId();

  if (!dm_token.is_valid())
    return;

  // Make sure DeviceManagementService has been initialized.
  device_management_service->ScheduleInitialization(0);

  client_ = std::make_unique<policy::CloudPolicyClient>(
      /*machine_id=*/std::string(), /*machine_model=*/std::string(),
      /*brand_code=*/std::string(), /*ethernet_mac_address=*/std::string(),
      /*dock_mac_address=*/std::string(), /*manufacture_date=*/std::string(),
      device_management_service, g_browser_process->shared_url_loader_factory(),
      nullptr, policy::CloudPolicyClient::DeviceDMTokenCallback());

  if (!client_->is_registered()) {
    client_->SetupRegistration(
        dm_token.value(), client_id,
        /*user_affiliation_ids=*/std::vector<std::string>());
  }
#endif
}

bool SafeBrowsingPrivateEventRouter::IsRealtimeReportingEnabled() {
  // g_browser_process and/or g_browser_process->local_state() may be null
  // in tests.
  return g_browser_process && g_browser_process->local_state() &&
         g_browser_process->local_state()->GetBoolean(
             prefs::kUnsafeEventsReportingEnabled);
}

void SafeBrowsingPrivateEventRouter::RealtimeReportingPrefChanged(
    const std::string& pref) {
  // If the reporting policy has been turned on, try to initialized now.
  if (IsRealtimeReportingEnabled())
    InitRealtimeReportingClient();
}

void SafeBrowsingPrivateEventRouter::ReportRealtimeEvent(
    const std::string& name,
    EventBuilder event_builder) {
  if (binary_upload_service_) {
    binary_upload_service_->IsAuthorized(base::BindOnce(
        &SafeBrowsingPrivateEventRouter::ReportRealtimeEventCallback,
        weakptr_factory_.GetWeakPtr(), name, std::move(event_builder)));
  }
}

void SafeBrowsingPrivateEventRouter::ReportRealtimeEventCallback(
    const std::string& name,
    EventBuilder event_builder,
    bool authorized) {
  // Ignore the event if we know we can't report it.
  if (!authorized)
    return;

  // |client_| should be set when authorized is true.
  DCHECK(client_);

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

  base::Value event_list(base::Value::Type::LIST);
  event_list.Append(std::move(wrapper));

  client_->UploadRealtimeReport(
      policy::RealtimeReportingJobConfiguration::BuildReport(
          std::move(event_list),
          reporting::GetContext(Profile::FromBrowserContext(context_))),
      base::DoNothing());
}

std::string SafeBrowsingPrivateEventRouter::GetProfileUserName() {
  // |identity_manager_| may be null is some tests.
  return identity_manager_ && identity_manager_->HasPrimaryAccount()
             ? identity_manager_->GetPrimaryAccountInfo().email
             : std::string();
}

}  // namespace extensions
