// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/optin/arc_terms_of_service_oobe_negotiator.h"

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/chromeos/login/arc_terms_of_service_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/consolidated_consent_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"

namespace arc {

namespace {

chromeos::ArcTermsOfServiceScreenView* g_view_for_testing = nullptr;

chromeos::ArcTermsOfServiceScreenView* GetScreenView() {
  // Inject testing instance.
  if (g_view_for_testing)
    return g_view_for_testing;

  auto* host = ash::LoginDisplayHost::default_host();
  DCHECK(host);

  // Ensure WebUI is loaded
  host->GetWizardController();

  return host->GetOobeUI()->GetView<chromeos::ArcTermsOfServiceScreenHandler>();
}

chromeos::ConsolidatedConsentScreen* GetConsolidatedConsentScreen() {
  // TODO: Inject testing instance.
  chromeos::LoginDisplayHost* host = chromeos::LoginDisplayHost::default_host();
  DCHECK(host);
  DCHECK(host->GetWizardController());
  return host->GetWizardController()
      ->GetScreen<chromeos::ConsolidatedConsentScreen>();
}

}  // namespace

// static
void ArcTermsOfServiceOobeNegotiator::SetArcTermsOfServiceScreenViewForTesting(
    chromeos::ArcTermsOfServiceScreenView* view) {
  g_view_for_testing = view;
}

ArcTermsOfServiceOobeNegotiator::ArcTermsOfServiceOobeNegotiator() = default;

ArcTermsOfServiceOobeNegotiator::~ArcTermsOfServiceOobeNegotiator() {
  // During tests shutdown screen_view_ might still be alive.
  if (!screen_view_)
    return;

  DCHECK(g_browser_process->IsShuttingDown());
  // Handle test shutdown gracefully.
  screen_view_->RemoveObserver(this);
}

void ArcTermsOfServiceOobeNegotiator::StartNegotiationImpl() {
  if (chromeos::features::IsOobeConsolidatedConsentEnabled()) {
    consolidated_consent_observation_.Observe(GetConsolidatedConsentScreen());
  } else {
    DCHECK(!screen_view_);
    screen_view_ = GetScreenView();
    DCHECK(screen_view_);
    screen_view_->AddObserver(this);
  }
}

void ArcTermsOfServiceOobeNegotiator::HandleTermsAccepted(bool accepted) {
  if (chromeos::features::IsOobeConsolidatedConsentEnabled()) {
    consolidated_consent_observation_.Reset();
  } else {
    DCHECK(screen_view_);
    screen_view_->RemoveObserver(this);
    screen_view_ = nullptr;
  }
  ReportResult(accepted);
}

void ArcTermsOfServiceOobeNegotiator::OnAccept(bool /* review_arc_settings */) {
  HandleTermsAccepted(true);
}

void ArcTermsOfServiceOobeNegotiator::OnViewDestroyed(
    chromeos::ArcTermsOfServiceScreenView* view) {
  DCHECK_EQ(view, screen_view_);
  HandleTermsAccepted(false);
}

void ArcTermsOfServiceOobeNegotiator::OnConsolidatedConsentAccept() {
  DCHECK(chromeos::features::IsOobeConsolidatedConsentEnabled());
  HandleTermsAccepted(true);
}

void ArcTermsOfServiceOobeNegotiator::OnConsolidatedConsentScreenDestroyed() {
  DCHECK(chromeos::features::IsOobeConsolidatedConsentEnabled());
  HandleTermsAccepted(false);
}

}  // namespace arc
