// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"

#include "base/base64.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/signal_reporter.h"

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

DeviceTrustService::DeviceTrustService(
    PrefService* pref_service,
    std::unique_ptr<AttestationService> attestation_service,
    std::unique_ptr<DeviceTrustSignalReporter> signal_reporter)
    : DeviceTrustService(pref_service,
                         std::move(attestation_service),
                         std::move(signal_reporter),
                         base::BindOnce(&DeviceTrustService::OnSignalReported,
                                        base::Unretained(this))) {}

DeviceTrustService::DeviceTrustService(
    PrefService* pref_service,
    std::unique_ptr<AttestationService> attestation_service,
    std::unique_ptr<DeviceTrustSignalReporter> signal_reporter,
    SignalReportCallback signal_report_callback)
    : prefs_(pref_service),
      attestation_service_(std::move(attestation_service)),
      signal_reporter_(std::move(signal_reporter)),
      signal_report_callback_(std::move(signal_report_callback)) {
  // Using Unretained is ok here because pref_observer_ is owned by this class,
  // and we Remove() this path from pref_observer_ in Shutdown().
  pref_observer_.Init(prefs_);
  pref_observer_.Add(kContextAwareAccessSignalsAllowlistPref,
                     base::BindRepeating(&DeviceTrustService::OnPolicyUpdated,
                                         base::Unretained(this)));

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
  // signal_reporter_.SendReport(), and signal_reporter_ is owned by this class.
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

  if (!signal_reporter_) {
    // Bypass reporter initialization because it already failed previously.
    return OnReporterInitialized(false);
  }

  if (!first_report_sent_ && IsEnabled()) {  // Policy enabled for the 1st time.
    signal_reporter_->Init(
        MakePolicyCheck(),
        base::BindOnce(&DeviceTrustService::OnReporterInitialized,
                       weak_factory_.GetWeakPtr()));
  }
}

void DeviceTrustService::OnReporterInitialized(bool success) {
  DVLOG(1) << "DeviceTrustService::OnReporterInitialized success=" << success;

  if (!success) {
    // Initialization failed, so reset signal_reporter_ to prevent retrying
    // Init().
    signal_reporter_.reset(nullptr);
    // Bypass SendReport and run callback with report failure.
    if (signal_report_callback_) {
      std::move(signal_report_callback_).Run(false);
    }
    return;
  }

  DeviceTrustReportEvent report;
  attestation_service_->StampReport(report);

  DVLOG(1) << "DeviceTrustService::OnReporterInitialized report sent";
  signal_reporter_->SendReport(&report, std::move(signal_report_callback_));
}

void DeviceTrustService::OnSignalReported(bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to send device trust signal report. Reason: "
               << (signal_reporter_.get() ? "Reporter failure"
                                          : "<no reporter>");
    // TODO(https://crbug.com/1186413) Handle failure cases.
  } else {
    first_report_sent_ = true;
  }
}

void DeviceTrustService::BuildChallengeResponse(const std::string& challenge,
                                                AttestationCallback callback) {
  attestation_service_->BuildChallengeResponseForVAChallenge(
      challenge, std::move(callback));
}

base::CallbackListSubscription
DeviceTrustService::RegisterTrustedUrlPatternsChangedCallback(
    TrustedUrlPatternsChangedCallback callback) {
  // Notify the callback right away so that caller can initialize itself.
  if (IsEnabled())
    callback.Run(GetTrustedUrlPatterns(prefs_));

  return callbacks_.Add(callback);
}

}  // namespace enterprise_connectors
