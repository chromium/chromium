// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/optin/arc_terms_of_service_oobe_negotiator.h"

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/login/screens/consolidated_consent_screen.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"

namespace arc {

namespace {

ash::ConsolidatedConsentScreen* GetConsolidatedConsentScreen() {
  auto* host = ash::LoginDisplayHost::default_host();
  DCHECK(host);
  DCHECK(host->GetWizardController());
  return host->GetWizardController()
      ->GetScreen<ash::ConsolidatedConsentScreen>();
}

}  // namespace

ArcTermsOfServiceOobeNegotiator::ArcTermsOfServiceOobeNegotiator() = default;

ArcTermsOfServiceOobeNegotiator::~ArcTermsOfServiceOobeNegotiator() = default;

void ArcTermsOfServiceOobeNegotiator::StartNegotiationImpl() {
  consolidated_consent_observation_.Observe(GetConsolidatedConsentScreen());
}

void ArcTermsOfServiceOobeNegotiator::HandleTermsAccepted(bool accepted) {
  consolidated_consent_observation_.Reset();
  ReportResult(accepted);
}

void ArcTermsOfServiceOobeNegotiator::OnConsolidatedConsentAccept() {
  HandleTermsAccepted(true);
}

void ArcTermsOfServiceOobeNegotiator::OnConsolidatedConsentScreenDestroyed() {
  HandleTermsAccepted(false);
}

}  // namespace arc
