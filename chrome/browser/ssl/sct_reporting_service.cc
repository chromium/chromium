// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/sct_reporting_service.h"

#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/chrome_features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"

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

constexpr char kSBSCTAuditingReportURL[] =
    "https://safebrowsing.google.com/safebrowsing/clientreport/"
    "chrome-sct-auditing";

// static
GURL& SCTReportingService::GetReportURLInstance() {
  static base::NoDestructor<GURL> instance(kSBSCTAuditingReportURL);
  return *instance;
}

// static
void SCTReportingService::ReconfigureAfterNetworkRestart() {
  bool is_sct_auditing_enabled =
      base::FeatureList::IsEnabled(features::kSCTAuditing);
  double sct_sampling_rate = features::kSCTAuditingSamplingRate.Get();
  mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_client;
  SystemNetworkContextManager::GetInstance()->GetURLLoaderFactory()->Clone(
      factory_client.InitWithNewPipeAndPassReceiver());
  content::GetNetworkService()->ConfigureSCTAuditing(
      is_sct_auditing_enabled, sct_sampling_rate,
      SCTReportingService::GetReportURLInstance(),
      net::MutableNetworkTrafficAnnotationTag(kSCTAuditReportTrafficAnnotation),
      std::move(factory_client));
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

namespace {
void SetSCTAuditingEnabledForStoragePartition(
    bool enabled,
    content::StoragePartition* storage_partition) {
  storage_partition->GetNetworkContext()->SetSCTAuditingEnabled(enabled);
}
}  // namespace

void SCTReportingService::SetReportingEnabled(bool enabled) {
  // Iterate over StoragePartitions for this Profile, and for each get the
  // NetworkContext and enable or disable SCT auditing.
  content::BrowserContext::ForEachStoragePartition(
      profile_,
      base::BindRepeating(&SetSCTAuditingEnabledForStoragePartition, enabled));

  if (!enabled)
    content::GetNetworkService()->ClearSCTAuditingCache();
}

void SCTReportingService::OnPreferenceChanged() {
  const bool enabled = safe_browsing_service_ &&
                       safe_browsing_service_->enabled_by_prefs() &&
                       safe_browsing::IsExtendedReportingEnabled(pref_service_);
  SetReportingEnabled(enabled);
}
