// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/sct_reporting_service.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/escape.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/google_api_keys.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"

constexpr net::NetworkTrafficAnnotationTag kSCTAuditReportTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("sct_auditing", R"(
        semantics {
          sender: "Safe Browsing"
          description:
            "When a user connects to a site, opted-in clients may upload "
            "a report about the Signed Certificate Timestamps used for meeting "
            "Chrome's Certificate Transparency Policy to Safe Browsing to "
            "detect misbehaving Certificate Transparency logs. This helps "
            "improve the security and trustworthiness of the HTTPS ecosystem."
          trigger:
            "The browser will upload a report to Google when a connection to a "
            "website includes Signed Certificate Timestamps, and the user is "
            "opted in to extended reporting."
          data:
            "The time of the request, the hostname and port being requested, "
            "the certificate chain, and the Signed Certificate Timestamps "
            "observed on the connection."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can enable or disable this feature by enabling or disabling "
            "'Enhanced Protection' in Chrome's settings under Security, "
            "Safe Browsing, or by enabling or disabling 'Help improve security "
            "on the web for everyone' under 'Standard Protection' in Chrome's "
            "settings under Security, Safe Browsing. The feature is disabled "
            "by default."
          chrome_policy {
            SafeBrowsingExtendedReportingEnabled {
              policy_options {mode: MANDATORY}
              SafeBrowsingExtendedReportingEnabled: false
            }
          }
        })");

constexpr net::NetworkTrafficAnnotationTag kSCTHashdanceTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("sct_auditing_hashdance", R"(
        semantics {
          sender: "Safe Browsing"
          description:
            "When a user connects to a site, clients with Safe Browsing "
            "enabled may query Google about similar Signed Certificate "
            "Timestamps. If the SCT has not been seen before, this indicates a "
            "security incident and the client will upload a full report. This "
            "helps improve the security and trustworthiness of the HTTPS "
            "ecosystem."
          trigger:
            "The browser will query Google when a connection to a website "
            "includes Signed Certificate Timestamps, and the user is opted in "
            "to Safe Browsing."
          data:
            "A short prefix of the SCT leaf hash, the length of the prefix, "
            "and a short user agent string."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can enable or disable this feature by enabling or disabling "
            "'Safe Browsing' in Chrome's settings under Security, "
            "Safe Browsing. This feature is enabled by default."
          chrome_policy {
            SafeBrowsingProtectionLevel {
              policy_options {mode: MANDATORY}
              SafeBrowsingProtectionLevel: 0
            }
          }
        })");

constexpr char kSBSCTAuditingReportURL[] =
    "https://safebrowsing.google.com/safebrowsing/clientreport/"
    "chrome-sct-auditing";

constexpr char kHashdanceLookupQueryURL[] =
    "https://sctauditing-pa.googleapis.com/v1/knownscts/"
    "length/$1/prefix/$2?key=";

// The maximum number of reports currently allowed to be sent by hashdance
// clients, browser-wide. When this limit is reached, no more auditing reports
// will be sent by the client.
// NOTE: If this is changed, then the histogram "Security.SCTAuditing.OptOut.
// ReportCount" that is logged in CanSendSCTAuditingReport() will also need to
// be changed, as it sets its max bucket to `kSCTAuditingHashdanceMaxReports+1`.
constexpr int kSCTAuditingHashdanceMaxReports = 3;

// static
GURL& SCTReportingService::GetReportURLInstance() {
  static base::NoDestructor<GURL> instance(kSBSCTAuditingReportURL);
  return *instance;
}

// static
GURL& SCTReportingService::GetHashdanceLookupQueryURLInstance() {
  static base::NoDestructor<GURL> instance(
      std::string(kHashdanceLookupQueryURL) +
      base::EscapeQueryParamValue(google_apis::GetAPIKey(), /*use_plus=*/true));
  return *instance;
}

// static
void SCTReportingService::ReconfigureAfterNetworkRestart() {
  network::mojom::SCTAuditingConfigurationPtr configuration(std::in_place);
  configuration->sampling_rate = features::kSCTAuditingSamplingRate.Get();
  configuration->log_expected_ingestion_delay =
      features::kSCTLogExpectedIngestionDelay.Get();
  configuration->log_max_ingestion_random_delay =
      features::kSCTLogMaxIngestionRandomDelay.Get();
  configuration->report_uri = SCTReportingService::GetReportURLInstance();
  configuration->hashdance_lookup_uri =
      SCTReportingService::GetHashdanceLookupQueryURLInstance();
  configuration->traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(kSCTAuditReportTrafficAnnotation);
  configuration->hashdance_traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(kSCTHashdanceTrafficAnnotation);
  content::GetNetworkService()->ConfigureSCTAuditing(std::move(configuration));
}

// static
bool SCTReportingService::CanSendSCTAuditingReport() {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state) {
    // Fail safe by returning `false` if we can't get the pref state.
    return false;
  }
  int report_count =
      local_state->GetInteger(prefs::kSCTAuditingHashdanceReportCount);
  // Log a histogram for the report count. This uses an "exact linear" bucketing
  // scheme so it captures precise counts, and a max of one more than the
  // max-reports limit so that only cases where the client has exceeded the
  // limit are logged into the overflow bucket.
  base::UmaHistogramExactLinear("Security.SCTAuditing.OptOut.ReportCount",
                                report_count,
                                kSCTAuditingHashdanceMaxReports + 1);
  return report_count < kSCTAuditingHashdanceMaxReports;
}

// static
void SCTReportingService::OnNewSCTAuditingReportSent() {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state) {
    return;
  }
  int report_count =
      local_state->GetInteger(prefs::kSCTAuditingHashdanceReportCount);

  // We should not ever send more than kSCTAuditingHashdanceMaxReports full
  // reports. DCHECK to make it very clear in testing if this is not the case.
  DCHECK_LE(report_count, kSCTAuditingHashdanceMaxReports);

  // Note: Pref updates won't be persisted for Incognito profiles, but SCT
  // auditing is disabled entirely for Incognito profiles.
  local_state->SetInteger(prefs::kSCTAuditingHashdanceReportCount,
                          ++report_count);
}

SCTReportingService::SCTReportingService(
    safe_browsing::SafeBrowsingService* safe_browsing_service,
    Profile* profile)
    : safe_browsing_service_(safe_browsing_service),
      pref_service_(*profile->GetPrefs()),
      profile_(profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // SCT auditing should stay disabled for Incognito/OTR profiles, so we don't
  // need to subscribe to the prefs.
  if (profile_->IsOffTheRecord())
    return;

  // Subscribe to SafeBrowsing preference change notifications. The initial Safe
  // Browsing state gets emitted to subscribers during Profile creation.
  safe_browsing_state_subscription_ =
      safe_browsing_service_->RegisterStateCallback(base::BindRepeating(
          &SCTReportingService::OnPreferenceChanged, base::Unretained(this)));
}

SCTReportingService::~SCTReportingService() = default;

network::mojom::SCTAuditingMode SCTReportingService::GetReportingMode() {
  if (profile_->IsOffTheRecord() ||
      !base::FeatureList::IsEnabled(features::kSCTAuditing)) {
    return network::mojom::SCTAuditingMode::kDisabled;
  }
  if (safe_browsing::IsSafeBrowsingEnabled(*pref_service_)) {
    if (safe_browsing::IsExtendedReportingEnabled(*pref_service_)) {
      return network::mojom::SCTAuditingMode::kEnhancedSafeBrowsingReporting;
    }
    if (base::FeatureList::IsEnabled(features::kSCTAuditingHashdance)) {
      return network::mojom::SCTAuditingMode::kHashdance;
    }
  }
  return network::mojom::SCTAuditingMode::kDisabled;
}

void SCTReportingService::OnPreferenceChanged() {
  network::mojom::SCTAuditingMode mode = GetReportingMode();

  // Iterate over StoragePartitions for this Profile, and for each get the
  // NetworkContext and set the SCT auditing mode.
  profile_->ForEachLoadedStoragePartition(
      [mode](content::StoragePartition* partition) {
        partition->GetNetworkContext()->SetSCTAuditingMode(mode);
      });

  if (mode == network::mojom::SCTAuditingMode::kDisabled)
    content::GetNetworkService()->ClearSCTAuditingCache();
}
