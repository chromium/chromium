// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SAFE_BROWSING_PRIVATE_SAFE_BROWSING_PRIVATE_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SAFE_BROWSING_PRIVATE_SAFE_BROWSING_PRIVATE_EVENT_ROUTER_H_

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class EventRouter;
}

class GURL;

namespace safe_browsing {
enum class DeepScanAccessPoint;
}

namespace extensions {

// An event router that observes Safe Browsing events and notifies listeners.
// The router also uploads events to the chrome reporting server side API if
// the kRealtimeReportingFeature feature is enabled.
class SafeBrowsingPrivateEventRouter : public KeyedService {
 public:
  // Key names used with when building the dictionary to pass to the real-time
  // reporting API.
  static const char kKeyUrl[];
  static const char kKeySource[];
  static const char kKeyDestination[];
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
  static const char kKeyScanId[];
  static const char kKeyIsFederated[];
  static const char kKeyFederatedOrigin[];
  static const char kKeyLoginUserName[];
  static const char kKeyPasswordBreachIdentities[];
  static const char kKeyPasswordBreachIdentitiesUrl[];
  static const char kKeyPasswordBreachIdentitiesUsername[];
  static const char kKeyUserJustification[];
  static const char kKeyUrlCategory[];
  static const char kKeyAction[];
  static const char kKeyTabUrl[];

  // All new event names should be added to the array
  // `enterprise_connectors::ReportingServiceSettings::kAllReportingEvents` in
  // chrome/browser/enterprise/connectors/reporting/reporting_service_settings.h
  static constexpr char kKeyUrlFilteringInterstitialEvent[] =
      "urlFilteringInterstitialEvent";
  static constexpr char kKeyPasswordReuseEvent[] = "passwordReuseEvent";
  static constexpr char kKeyPasswordChangedEvent[] = "passwordChangedEvent";
  static constexpr char kKeyDangerousDownloadEvent[] = "dangerousDownloadEvent";
  static constexpr char kKeyInterstitialEvent[] = "interstitialEvent";
  static constexpr char kKeySensitiveDataEvent[] = "sensitiveDataEvent";
  static constexpr char kKeyUnscannedFileEvent[] = "unscannedFileEvent";
  static constexpr char kKeyLoginEvent[] = "loginEvent";
  static constexpr char kKeyPasswordBreachEvent[] = "passwordBreachEvent";

  static const char kKeyUnscannedReason[];

  // String constants for the "trigger" event field.  This corresponds to
  // an enterprise connector.
  static const char kTriggerFileDownload[];
  static const char kTriggerFileUpload[];
  static const char kTriggerWebContentUpload[];
  static const char kTriggerPagePrint[];
  static const char kTriggerFileTransfer[];

  explicit SafeBrowsingPrivateEventRouter(content::BrowserContext* context);

  SafeBrowsingPrivateEventRouter(const SafeBrowsingPrivateEventRouter&) =
      delete;
  SafeBrowsingPrivateEventRouter& operator=(
      const SafeBrowsingPrivateEventRouter&) = delete;

  ~SafeBrowsingPrivateEventRouter() override;

  // Notifies listeners that the user reused a protected password.
  // - `url` is the URL where the password was reused
  // - `user_name` is the user associated with the reused password
  // - `is_phising_url` is whether the URL is thought to be a phishing one
  // - `warning_shown` is whether a warning dialog was shown to the user
  void OnPolicySpecifiedPasswordReuseDetected(const GURL& url,
                                              const std::string& user_name,
                                              bool is_phishing_url,
                                              bool warning_shown);

  // Notifies listeners that the user changed the password associated with
  // |user_name|.
  void OnPolicySpecifiedPasswordChanged(const std::string& user_name);

  // Notifies listeners that the user just opened a dangerous download.
  void OnDangerousDownloadOpened(const GURL& download_url,
                                 const GURL& tab_url,
                                 const std::string& file_name,
                                 const std::string& download_digest_sha256,
                                 const std::string& mime_type,
                                 const std::string& scan_id,
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
      const GURL& tab_url,
      const std::string& source,
      const std::string& destination,
      const std::string& file_name,
      const std::string& download_digest_sha256,
      const std::string& mime_type,
      const std::string& trigger,
      const std::string& scan_id,
      safe_browsing::DeepScanAccessPoint access_point,
      const enterprise_connectors::ContentAnalysisResponse::Result& result,
      const int64_t content_size,
      safe_browsing::EventResult event_result);

  // Notifies listeners that an analysis connector violation was bypassed.
  void OnAnalysisConnectorWarningBypassed(
      const GURL& url,
      const GURL& tab_url,
      const std::string& source,
      const std::string& destination,
      const std::string& file_name,
      const std::string& download_digest_sha256,
      const std::string& mime_type,
      const std::string& trigger,
      const std::string& scan_id,
      safe_browsing::DeepScanAccessPoint access_point,
      const enterprise_connectors::ContentAnalysisResponse::Result& result,
      const int64_t content_size,
      absl::optional<std::u16string> user_justification);

  // Notifies listeners that deep scanning failed, for the given |reason|.
  void OnUnscannedFileEvent(const GURL& url,
                            const GURL& tab_url,
                            const std::string& source,
                            const std::string& destination,
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
                                const GURL& tab_url,
                                const std::string& file_name,
                                const std::string& download_digest_sha256,
                                const std::string& threat_type,
                                const std::string& mime_type,
                                const std::string& scan_id,
                                const int64_t content_size,
                                safe_browsing::EventResult event_result);
  void OnDangerousDownloadEvent(const GURL& url,
                                const GURL& tab_url,
                                const std::string& file_name,
                                const std::string& download_digest_sha256,
                                const download::DownloadDangerType danger_type,
                                const std::string& mime_type,
                                const std::string& scan_id,
                                const int64_t content_size,
                                safe_browsing::EventResult event_result);

  // Notifies listeners that the user bypassed a download warning.
  // - |url| is the download URL
  // - |file_name| is the path on disk
  // - |download_digest_sha256| is the hex-encoded SHA256
  // - |threat_type| is the danger type of the download.
  void OnDangerousDownloadWarningBypassed(
      const GURL& url,
      const GURL& tab_url,
      const std::string& file_name,
      const std::string& download_digest_sha256,
      const std::string& threat_type,
      const std::string& mime_type,
      const std::string& scan_id,
      const int64_t content_size);
  void OnDangerousDownloadWarningBypassed(
      const GURL& url,
      const GURL& tab_url,
      const std::string& file_name,
      const std::string& download_digest_sha256,
      const download::DownloadDangerType danger_type,
      const std::string& mime_type,
      const std::string& scan_id,
      const int64_t content_size);

  void OnLoginEvent(const GURL& url,
                    bool is_federated,
                    const url::Origin& federated_origin,
                    const std::u16string& username);

  void OnPasswordBreach(
      const std::string& trigger,
      const std::vector<std::pair<GURL, std::u16string>>& identities);

  // Notifies listeners that the user saw an enterprise policy related
  // interstitial.
  void OnUrlFilteringInterstitial(
      const GURL& url,
      const std::string& threat_type,
      const safe_browsing::RTLookupResponse& response);

 private:
  // Returns filename with full path if full path is required;
  // Otherwise returns only the basename without full path.
  static std::string GetFileName(const std::string& filename,
                                 const bool include_full_path);

  // Returns the Gaia email address of the account signed in to the profile or
  // an empty string if the profile is not signed in.
  std::string GetProfileUserName() const;

  // Notifies listeners that deep scanning detected a dangerous download.
  void OnDangerousDeepScanningResult(
      const GURL& download_url,
      const GURL& tab_url,
      const std::string& source,
      const std::string& destination,
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
      const std::string& scan_id);

  // Notifies listeners that the analysis connector detected a violation.
  void OnSensitiveDataEvent(
      const GURL& url,
      const GURL& tab_url,
      const std::string& source,
      const std::string& destination,
      const std::string& file_name,
      const std::string& download_digest_sha256,
      const std::string& mime_type,
      const std::string& trigger,
      const std::string& scan_id,
      const enterprise_connectors::ContentAnalysisResponse::Result& result,
      const int64_t content_size,
      safe_browsing::EventResult event_result);

  raw_ptr<content::BrowserContext> context_;
  raw_ptr<EventRouter> event_router_ = nullptr;
  raw_ptr<enterprise_connectors::RealtimeReportingClient> reporting_client_ =
      nullptr;

  // The private clients are used on platforms where we cannot just get a
  // client and we create our own (used through the above client pointers).
  std::unique_ptr<policy::CloudPolicyClient> browser_private_client_;
  std::unique_ptr<policy::CloudPolicyClient> profile_private_client_;

  // When a request is rejected for a given DM token, wait 24 hours before
  // trying again for this specific DM Token.
  base::flat_map<std::string, std::unique_ptr<base::OneShotTimer>>
      rejected_dm_token_timers_;

  base::WeakPtrFactory<SafeBrowsingPrivateEventRouter> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SAFE_BROWSING_PRIVATE_SAFE_BROWSING_PRIVATE_EVENT_ROUTER_H_
