// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/optin/arc_terms_of_service_default_negotiator.h"

#include <string>

#include "chrome/browser/ash/arc/arc_optin_uma.h"
#include "chrome/browser/ash/arc/optin/arc_optin_preference_handler.h"

namespace arc {

ArcTermsOfServiceDefaultNegotiator::ArcTermsOfServiceDefaultNegotiator(
    PrefService* pref_service,
    ArcSupportHost* support_host,
    metrics::MetricsService* metrics_service)
    : pref_service_(pref_service),
      support_host_(support_host),
      metrics_service_(metrics_service) {
  DCHECK(pref_service_);
  DCHECK(support_host_);
  DCHECK(metrics_service_);
}

ArcTermsOfServiceDefaultNegotiator::~ArcTermsOfServiceDefaultNegotiator() {
  support_host_->SetTermsOfServiceDelegate(nullptr);
}

void ArcTermsOfServiceDefaultNegotiator::StartNegotiationImpl() {
  DCHECK(!preference_handler_);
  preference_handler_ = std::make_unique<ArcOptInPreferenceHandler>(
      this, pref_service_, metrics_service_);
  // This automatically updates all preferences.
  preference_handler_->Start();

  support_host_->SetTermsOfServiceDelegate(this);
  support_host_->ShowTermsOfService();
}

void ArcTermsOfServiceDefaultNegotiator::OnTermsRejected() {
  // User cancels terms-of-service agreement UI by clicking "Cancel" button
  // or closing the window directly.
  DCHECK(preference_handler_);
  support_host_->SetTermsOfServiceDelegate(nullptr);
  preference_handler_.reset();

  ReportResult(false);
}

void ArcTermsOfServiceDefaultNegotiator::OnTermsAgreed(
    bool is_metrics_enabled,
    bool is_backup_and_restore_enabled,
    bool is_location_service_enabled) {
  DCHECK(preference_handler_);
  support_host_->SetTermsOfServiceDelegate(nullptr);

  // Update the preferences with the value passed from UI.
  preference_handler_->EnableBackupRestore(is_backup_and_restore_enabled);
  preference_handler_->EnableLocationService(is_location_service_enabled);

  // This should be called last since the callback resets |preference_handler_|
  // and makes the callback that terms of service has successfully negotiated.
  preference_handler_->EnableMetrics(
      is_metrics_enabled,
      base::BindOnce(&ArcTermsOfServiceDefaultNegotiator::OnMetricsPrefsUpdated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcTermsOfServiceDefaultNegotiator::OnTermsLoadResult(bool success) {
  UpdateOptinTosLoadResultUMA(success);
}

void ArcTermsOfServiceDefaultNegotiator::OnTermsRetryClicked() {
  support_host_->ShowTermsOfService();
}

void ArcTermsOfServiceDefaultNegotiator::OnMetricsModeChanged(bool enabled,
                                                              bool managed) {
  support_host_->SetMetricsPreferenceCheckbox(enabled, managed);
}

void ArcTermsOfServiceDefaultNegotiator::OnBackupAndRestoreModeChanged(
    bool enabled,
    bool managed) {
  support_host_->SetBackupAndRestorePreferenceCheckbox(enabled, managed);
}

void ArcTermsOfServiceDefaultNegotiator::OnLocationServicesModeChanged(
    bool enabled,
    bool managed) {
  support_host_->SetLocationServicesPreferenceCheckbox(enabled, managed);
}

void ArcTermsOfServiceDefaultNegotiator::OnMetricsPrefsUpdated() {
  preference_handler_.reset();
  ReportResult(true);
}

}  // namespace arc
