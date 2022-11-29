// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/trial_comparison_cert_verifier_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/cert_verifier_configuration.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/security_interstitials/content/certificate_error_report.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/features.h"
#include "net/net_buildflags.h"

namespace {

// Certificate reports are only sent from official builds, but this flag can be
// set by tests.
bool g_is_fake_official_build_for_cert_verifier_testing = false;

}  // namespace

TrialComparisonCertVerifierController::TrialComparisonCertVerifierController(
    Profile* profile)
    : profile_(profile) {
  if (!MaybeAllowedForProfile(profile_)) {
    // Don't bother registering pref change notifier if the trial could never be
    // enabled.
    return;
  }

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kSafeBrowsingScoutReportingEnabled,
      base::BindRepeating(&TrialComparisonCertVerifierController::RefreshState,
                          base::Unretained(this)));
}

TrialComparisonCertVerifierController::
    ~TrialComparisonCertVerifierController() = default;

// static
bool TrialComparisonCertVerifierController::MaybeAllowedForProfile(
    Profile* profile) {
  bool is_official_build = g_is_fake_official_build_for_cert_verifier_testing;
#if defined(OFFICIAL_BUILD) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  is_official_build = true;
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  // If the Chrome Root Store is enabled as part of the default verifier, the
  // trial does not make sense.
  if (GetChromeCertVerifierServiceParams(/*local_state=*/nullptr)
          ->use_chrome_root_store)
    return false;
#endif

  return is_official_build &&
         base::FeatureList::IsEnabled(
             net::features::kCertDualVerificationTrialFeature) &&
         !profile->IsOffTheRecord();
}

void TrialComparisonCertVerifierController::AddClient(
    mojo::PendingRemote<
        cert_verifier::mojom::TrialComparisonCertVerifierConfigClient>
        config_client,
    mojo::PendingReceiver<
        cert_verifier::mojom::TrialComparisonCertVerifierReportClient>
        report_client_receiver) {
  receiver_set_.Add(this, std::move(report_client_receiver));
  config_client_set_.Add(std::move(config_client));
}

bool TrialComparisonCertVerifierController::IsAllowed() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Only allow on non-incognito profiles which have SBER opt-in set.
  // See design doc for more details:
  // https://docs.google.com/document/d/1AM1CD42bC6LHWjKg-Hkid_RLr2DH6OMzstH9-pGSi-g

  if (!MaybeAllowedForProfile(profile_))
    return false;

  const PrefService& prefs = *profile_->GetPrefs();

  return safe_browsing::IsExtendedReportingEnabled(prefs);
}

void TrialComparisonCertVerifierController::SendTrialReport(
    const std::string& hostname,
    const scoped_refptr<net::X509Certificate>& unverified_cert,
    bool enable_rev_checking,
    bool require_rev_checking_local_anchors,
    bool enable_sha1_local_anchors,
    bool disable_symantec_enforcement,
    const std::vector<uint8_t>& stapled_ocsp,
    const std::vector<uint8_t>& sct_list,
    const net::CertVerifyResult& primary_result,
    const net::CertVerifyResult& trial_result,
    cert_verifier::mojom::CertVerifierDebugInfoPtr debug_info) {
  if (!IsAllowed() || base::GetFieldTrialParamByFeatureAsBool(
                          net::features::kCertDualVerificationTrialFeature,
                          "uma_only", false)) {
    return;
  }

  CertificateErrorReport report(
      hostname, *unverified_cert, enable_rev_checking,
      require_rev_checking_local_anchors, enable_sha1_local_anchors,
      disable_symantec_enforcement,
      std::string(stapled_ocsp.begin(), stapled_ocsp.end()),
      std::string(sct_list.begin(), sct_list.end()), primary_result,
      trial_result, std::move(debug_info));

  report.AddNetworkTimeInfo(g_browser_process->network_time_tracker());
  report.AddChromeChannel(chrome::GetChannel());

  std::string serialized_report;
  if (!report.Serialize(&serialized_report))
    return;

  CertificateReportingServiceFactory::GetForBrowserContext(profile_)->Send(
      serialized_report);
}

// static
void TrialComparisonCertVerifierController::SetFakeOfficialBuildForTesting(
    bool fake_official_build) {
  g_is_fake_official_build_for_cert_verifier_testing = fake_official_build;
}

void TrialComparisonCertVerifierController::RefreshState() {
  const bool is_allowed = IsAllowed();
  for (auto& client : config_client_set_)
    client->OnTrialConfigUpdated(is_allowed);
}
