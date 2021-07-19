// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"

#include "base/base64.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/signal_reporter.h"
#include "chrome/browser/profiles/profile.h"
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/attestation/tpm_challenge_key_with_timeout.h"
#endif

namespace enterprise_connectors {

static const base::ListValue* GetTrustedUrlPatterns(PrefService* prefs) {
  return prefs->HasPrefPath(kContextAwareAccessSignalsAllowlistPref)
             ? prefs->GetList(kContextAwareAccessSignalsAllowlistPref)
             : nullptr;
}

// static
bool DeviceTrustService::IsEnabled(PrefService* prefs) {
  auto* list = GetTrustedUrlPatterns(prefs);
  return list && !list->GetList().empty();
}

DeviceTrustService::DeviceTrustService(Profile* profile)
    : prefs_(profile->GetPrefs()),
      profile_(profile),
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

  // OnPolicyUpdated() won't get called until the policy actually changes.
  // To make sure everything is initialized on start up, call it now to kick
  // things off.
  OnPolicyUpdated();
}

DeviceTrustService::~DeviceTrustService() {
  DCHECK(callbacks_.empty());
}

void DeviceTrustService::Shutdown() {
  pref_observer_.Remove(kContextAwareAccessSignalsAllowlistPref);
}

bool DeviceTrustService::IsEnabled() const {
  return is_enabled_;
}

// static
base::RepeatingCallback<bool()> DeviceTrustService::MakePolicyCheck() {
  // Have to make lambda here because weak_ptrs can only bind to methods without
  // return values. Unretained is ok here since this callback is only used in
  // reporter_.SendReport(), and reporter_ is owned by this class.
  return base::BindRepeating(
      [](DeviceTrustService* self) { return self->IsEnabled(); },
      base::Unretained(this));
}

void DeviceTrustService::OnPolicyUpdated() {
  DVLOG(1) << "DeviceTrustService::OnPolicyUpdated";

  // Cache "is enabled" because some callers of IsEnabled() cannot access the
  // PrefService.
  is_enabled_ = IsEnabled(prefs_);

  if (IsEnabled() && !callbacks_.empty())
    callbacks_.Notify(GetTrustedUrlPatterns(prefs_));

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
  DVLOG(1) << "DeviceTrustService::OnReporterInitialized success=" << success;

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

  DVLOG(1) << "DeviceTrustService::OnReporterInitialized report sent";
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
#endif  // defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)

void DeviceTrustService::BuildChallengeResponse(const std::string& challenge,
                                                AttestationCallback callback) {
#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
  attestation_service_->BuildChallengeResponseForVAChallenge(
      challenge, std::move(callback));
#else
  tpm_key_challenger_ =
      std::make_unique<ash::attestation::TpmChallengeKeyWithTimeout>();
  tpm_key_challenger_->BuildResponse(
      base::TimeDelta::FromSeconds(15), ash::attestation::KEY_DEVICE, profile_,
      base::BindOnce(&DeviceTrustService::ReturnResult,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      JsonChallengeToProtobufChallenge(challenge), /*register_key=*/false,
      /*key_name_for_spkac=*/"");
#endif  // defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MAC)
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void DeviceTrustService::ReturnResult(
    AttestationCallback callback,
    const ash::attestation::TpmChallengeKeyResult& result) {
  if (!result.IsSuccess())
    LOG(WARNING) << "Device attestation error: " << result.GetErrorMessage();

  std::string encoded_response;
  base::Base64Encode(result.challenge_response, &encoded_response);
  std::move(callback).Run(encoded_response);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

base::CallbackListSubscription
DeviceTrustService::RegisterTrustedUrlPatternsChangedCallback(
    TrustedUrlPatternsChangedCallback callback) {
  // Notify the callback right away so that caller can initialize itself.
  if (IsEnabled())
    callback.Run(GetTrustedUrlPatterns(prefs_));

  return callbacks_.Add(callback);
}

}  // namespace enterprise_connectors
