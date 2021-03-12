// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SAFE_BROWSING_PRIVATE_SAFE_BROWSING_PRIVATE_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SAFE_BROWSING_PRIVATE_SAFE_BROWSING_PRIVATE_EVENT_ROUTER_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class EventRouter;
}

namespace signin {
class IdentityManager;
}

class GURL;

namespace policy {
class DeviceManagementService;
}

namespace safe_browsing {
enum class DeepScanAccessPoint;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

namespace user_manager {
class User;
}

#endif

namespace extensions {

// An event router that observes Safe Browsing events and notifies listeners.
// The router also uploads events to the chrome reporting server side API if
// the kRealtimeReportingFeature feature is enabled.
class SafeBrowsingPrivateEventRouter
    : public KeyedService,
      public policy::CloudPolicyClient::Observer {
 public:
  // Feature that controls whether real-time reports are sent.
  static const base::Feature kRealtimeReportingFeature;

  // Key names used with when building the dictionary to pass to the real-time
  // reporting API.
  static const char kKeyUrl[];
  static const char kKeyUserName[];
  static const char kKeyIsPhishingUrl[];
  static const char kKeyProfileUserName[];
  static const char kKeyFileName[];
  static const char kKeyDownloadDigestSha256[];
  static const char kKeyReason[];
  static const char kKeyNetErrorCode[];
  static const char kKeyClickedThrough[];
  static const char kKeyTriggeredRuleName[];
  static const char kKeyTriggeredRuleId[];
  static const char kKeyTriggeredRuleInfo[];
  static const char kKeyThreatType[];
  static const char kKeyContentType[];
  static const char kKeyContentSize[];
  static const char kKeyTrigger[];
  static const char kKeyEventResult[];
  static const char kKeyMalwareFamily[];
  static const char kKeyMalwareCategory[];
  static const char kKeyEvidenceLockerFilePath[];

  static const char kKeyPasswordReuseEvent[];
  static const char kKeyPasswordChangedEvent[];
  static const char kKeyDangerousDownloadEvent[];
  static const char kKeyInterstitialEvent[];
  static const char kKeySensitiveDataEvent[];
  static const char kKeyUnscannedFileEvent[];
  static const char* kAllEvents[6];

  static const char kKeyUnscannedReason[];

  // String constants for the "trigger" event field.  This corresponds to
  // an enterprise connector.
  static const char kTriggerFileDownload[];
  static const char kTriggerFileUpload[];
  static const char kTriggerWebContentUpload[];

  explicit SafeBrowsingPrivateEventRouter(content::BrowserContext* context);

  ~SafeBrowsingPrivateEventRouter() override;

  // Notifies listeners that the user reused a protected password.
  void OnPolicySpecifiedPasswordReuseDetected(const GURL& url,
                                              const std::string& user_name,
                                              bool is_phishing_url);

  // Notifies listeners that the user changed the password associated with
  // |user_name|.
  void OnPolicySpecifiedPasswordChanged(const std::string& user_name);

  // Notifies listeners that the user just opened a dangerous download.
  void OnDangerousDownloadOpened(const GURL& url,
                                 const std::string& file_name,
                                 const std::string& download_digest_sha256,
                                 const std::string& mime_type,
                                 const download::DownloadDangerType danger_type,
                                 const int64_t content_size);

  // Notifies listeners that the user saw a security interstitial.
  void OnSecurityInterstitialShown(const GURL& url,
                                   const std::string& reason,
                                   int net_error_code);

  // Notifies listeners that the user clicked-through a security interstitial.
  void OnSecurityInterstitialProceeded(const GURL& url,
                                       const std::string& reason,
                                       int net_error_code);

  // Notifies listeners that the analysis connector detected a violation.
  void OnAnalysisConnectorResult(
      const GURL& url,
      const std::string& file_name,
      const std::string& download_digest_sha256,
      const std::string& mime_type,
      const std::string& trigger,
      safe_browsing::DeepScanAccessPoint access_point,
      const enterprise_connectors::ContentAnalysisResponse::Result& result,
      const int64_t content_size,
      safe_browsing::EventResult event_result);

  // Notifies listeners that an analysis connector violation was bypassed.
  void OnAnalysisConnectorWarningBypassed(
      const GURL& url,
      const std::string& file_name,
      const std::string& download_digest_sha256,
      const std::string& mime_type,
      const std::string& trigger,
      safe_browsing::DeepScanAccessPoint access_point,
      const enterprise_connectors::ContentAnalysisResponse::Result& result,
      const int64_t content_size);

  // Notifies listeners that deep scanning failed, for the given |reason|.
  void OnUnscannedFileEvent(const GURL& url,
                            const std::string& file_name,
                            const std::string& download_digest_sha256,
                            const std::string& mime_type,
                            const std::string& trigger,
                            safe_browsing::DeepScanAccessPoint access_point,
                            const std::string& reason,
                            const int64_t content_size,
                            safe_browsing::EventResult event_result);

  // Notifies listeners that the user saw a download warning.
  // - |url| is the download URL
  // - |file_name| is the path on disk
  // - |download_digest_sha256| is the hex-encoded SHA256
  // - |threat_type| is the danger type of the download.
  void OnDangerousDownloadEvent(const GURL& url,
                                const std::string& file_name,
                                const std::string& download_digest_sha256,
                                const std::string& threat_type,
                                const std::string& mime_type,
                                const int64_t content_size,
                                safe_browsing::EventResult event_result);
  void OnDangerousDownloadEvent(const GURL& url,
                                const std::string& file_name,
                                const std::string& download_digest_sha256,
                                const download::DownloadDangerType danger_type,
                                const std::string& mime_type,
                                const int64_t content_size,
                                safe_browsing::EventResult event_result);

  // Notifies listeners that the user bypassed a download warning.
  // - |url| is the download URL
  // - |file_name| is the path on disk
  // - |download_digest_sha256| is the hex-encoded SHA256
  // - |threat_type| is the danger type of the download.
  void OnDangerousDownloadWarningBypassed(
      const GURL& url,
      const std::string& file_name,
      const std::string& download_digest_sha256,
      const std::string& threat_type,
      const std::string& mime_type,
      const int64_t content_size);
  void OnDangerousDownloadWarningBypassed(
      const GURL& url,
      const std::string& file_name,
      const std::string& download_digest_sha256,
      const download::DownloadDangerType danger_type,
      const std::string& mime_type,
      const int64_t content_size);

  // Returns true if enterprise real-time reporting should be initialized,
  // checking both the feature flag. This function is public so that it can
  // called in tests.
  static bool ShouldInitRealtimeReportingClient();

  void SetBrowserCloudPolicyClientForTesting(policy::CloudPolicyClient* client);
  void SetProfileCloudPolicyClientForTesting(policy::CloudPolicyClient* client);

  void SetIdentityManagerForTesting(signin::IdentityManager* identity_manager);

  // policy::CloudPolicyClient::Observer:
  void OnClientError(policy::CloudPolicyClient* client) override;
  void OnPolicyFetched(policy::CloudPolicyClient* client) override {}
  void OnRegistrationStateChanged(policy::CloudPolicyClient* client) override {}

 protected:
  // Report safe browsing event through real-time reporting channel, if enabled.
  // Declared as virtual for tests. Declared as protected to be called directly
  // by tests.
  virtual void ReportRealtimeEvent(
      const std::string&,
      const enterprise_connectors::ReportingSettings& settings,
      base::Value event);

 private:
  // Initialize a real-time report client if needed.  This client is used only
  // if real-time reporting is enabled, the machine is properly reigistered
  // with CBCM and the appropriate policies are enabled.
  void InitRealtimeReportingClient(
      const enterprise_connectors::ReportingSettings& settings);

  // Sub-methods called by InitRealtimeReportingClient to make appropriate
  // verifications and initialize the corresponding client. Returns a policy
  // client description and a client, which can be nullptr if it can't be
  // initialized.
  std::pair<std::string, policy::CloudPolicyClient*> InitBrowserReportingClient(
      const std::string& dm_token);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  std::pair<std::string, policy::CloudPolicyClient*> InitProfileReportingClient(
      const std::string& dm_token);
#endif

  // Determines if the real-time reporting feature is enabled.
  // Obtain settings to apply to a reporting event from ConnectorsService.
  // base::nullopt represents that reporting should not be done.
  base::Optional<enterprise_connectors::ReportingSettings>
  GetReportingSettings();

  // Called whenever the real-time reporting policy changes.
  void RealtimeReportingPrefChanged(const std::string& pref);

  // Create a privately owned cloud policy client for events routing.
  void CreatePrivateCloudPolicyClient(
      const std::string& policy_client_desc,
      policy::DeviceManagementService* device_management_service,
      const std::string& client_id,
      const std::string& dm_token);

  // Handle the availability of a cloud policy client.
  void OnCloudPolicyClientAvailable(const std::string& policy_client_desc,
                                    policy::CloudPolicyClient* client);

#if BUILDFLAG(IS_CHROMEOS_ASH)

  // Return the Chrome OS user who is subject to reporting, or nullptr if
  // the user cannot be deterined.
  static const user_manager::User* GetChromeOSUser();

#endif

  // Determines if real-time reporting is available based on platform and user.
  static bool IsRealtimeReportingAvailable();

  // Returns the Gaia email address of the account signed in to the profile or
  // an empty string if the profile is not signed in.
  std::string GetProfileUserName() const;

  // Notifies listeners that deep scanning detected a dangerous download.
  void OnDangerousDeepScanningResult(
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
      const std::string& evidence_locker_filepath);

  // Notifies listeners that the analysis connector detected a violation.
  void OnSensitiveDataEvent(
      const GURL& url,
      const std::string& file_name,
      const std::string& download_digest_sha256,
      const std::string& mime_type,
      const std::string& trigger,
      const enterprise_connectors::ContentAnalysisResponse::Result& result,
      const int64_t content_size,
      safe_browsing::EventResult event_result);

  void RemoveDmTokenFromRejectedSet(const std::string& dm_token);

  content::BrowserContext* context_;
  signin::IdentityManager* identity_manager_ = nullptr;
  EventRouter* event_router_ = nullptr;

  // The cloud policy clients used to upload browser events and profile events
  // to the cloud. These clients are never used to fetch policies. These
  // pointers are not owned by the class.
  policy::CloudPolicyClient* browser_client_ = nullptr;
  policy::CloudPolicyClient* profile_client_ = nullptr;

  // The private clients are used on platforms where we cannot just get a
  // client and we create our own (used through the above client pointers).
  std::unique_ptr<policy::CloudPolicyClient> browser_private_client_;
  std::unique_ptr<policy::CloudPolicyClient> profile_private_client_;

  // When a request is rejected for a given DM token, wait 24 hours before
  // trying again for this specific DM Token.
  base::flat_map<std::string, std::unique_ptr<base::OneShotTimer>>
      rejected_dm_token_timers_;

  base::WeakPtrFactory<SafeBrowsingPrivateEventRouter> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingPrivateEventRouter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SAFE_BROWSING_PRIVATE_SAFE_BROWSING_PRIVATE_EVENT_ROUTER_H_
