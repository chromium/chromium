// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"

#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/device_trust/signal_reporter.h"
#include "chrome/browser/profiles/profile.h"

namespace enterprise_connectors {

DeviceTrustService::DeviceTrustService(Profile* profile)
    : prefs_(profile->GetPrefs()),
      first_report_sent_(false),
      signal_report_callback_(
          base::BindOnce(&DeviceTrustService::OnSignalReported,
                         base::Unretained(this))) {
#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
  attestation_service_ = std::make_unique<AttestationService>();
#endif  // defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
  pref_observer_.Init(prefs_);
  pref_observer_.Add(kContextAwareAccessSignalsAllowlistPref,
                     base::BindRepeating(&DeviceTrustService::OnPolicyUpdated,
                                         base::Unretained(this)));
  // Using Unretained is ok here because pref_observer_ is owned by this class,
  // and we Remove() this path from pref_observer_ in Shutdown().
  reporter_ =
      std::make_unique<enterprise_connectors::DeviceTrustSignalReporter>();
}

DeviceTrustService::~DeviceTrustService() = default;

void DeviceTrustService::Shutdown() {
  pref_observer_.Remove(kContextAwareAccessSignalsAllowlistPref);
}

bool DeviceTrustService::IsEnabled() const {
  return prefs_->HasPrefPath(kContextAwareAccessSignalsAllowlistPref) &&
         !prefs_->GetList(kContextAwareAccessSignalsAllowlistPref)
              ->GetList()
              .empty();
}

base::RepeatingCallback<bool()> DeviceTrustService::MakePolicyCheck() {
  // Have to make lambda here because weak_ptrs can only bind to methods without
  // return values. Unretained is ok here since this callback is only used in
  // reporter_.SendReport(), and reporter_ is owned by this class.
  return base::BindRepeating(
      [](DeviceTrustService* self) { return self->IsEnabled(); },
      base::Unretained(this));
}

void DeviceTrustService::OnPolicyUpdated() {
  if (!reporter_) {
    // Bypass reporter initialization because it already failed previously.
    return OnReporterInitialized(false);
  }

  if (!first_report_sent_ && IsEnabled()) {  // Policy enabled for the 1st time.
    reporter_->Init(MakePolicyCheck(),
                    base::BindOnce(&DeviceTrustService::OnReporterInitialized,
                                   weak_factory_.GetWeakPtr()));
  }
}

void DeviceTrustService::OnReporterInitialized(bool success) {
  if (!success) {
    // Initialization failed, so reset reporter_ to prevent retrying Init().
    reporter_.reset(nullptr);
    // Bypass SendReport and run callback with report failure.
    if (signal_report_callback_) {
      std::move(signal_report_callback_).Run(false);
    }
    return;
  }

  DeviceTrustReportEvent report;

#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
  auto* credential = report.mutable_attestation_credential();
  credential->set_format(
      DeviceTrustReportEvent::Credential::EC_NID_X9_62_PRIME256V1_PUBLIC_DER);
  credential->set_credential(attestation_service_->ExportPublicKey());
#endif  // defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)

  reporter_->SendReport(&report, std::move(signal_report_callback_));
}

void DeviceTrustService::OnSignalReported(bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to send device trust signal report. Reason: "
               << (reporter_.get() ? "Reporter failure" : "<no reporter>");
    // TODO(https://crbug.com/1186413) Handle failure cases.
  } else {
    first_report_sent_ = true;
  }
}

void DeviceTrustService::SetSignalReporterForTesting(
    std::unique_ptr<DeviceTrustSignalReporter> reporter) {
  reporter_ = std::move(reporter);
}

void DeviceTrustService::SetSignalReportCallbackForTesting(
    SignalReportCallback cb) {
  signal_report_callback_ = base::BindOnce(
      [](DeviceTrustService* self, SignalReportCallback test_cb, bool success) {
        self->OnSignalReported(success);
        std::move(test_cb).Run(success);
      },
      base::Unretained(this), std::move(cb));
}

#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
std::string DeviceTrustService::GetAttestationCredentialForTesting() const {
  return attestation_service_->ExportPublicKey();
}

void DeviceTrustService::BuildChallengeResponse(const std::string& challenge,
                                                AttestationCallback callback) {
  attestation_service_->BuildChallengeResponseForVAChallenge(
      challenge, std::move(callback));
}
#endif  // defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)

}  // namespace enterprise_connectors
