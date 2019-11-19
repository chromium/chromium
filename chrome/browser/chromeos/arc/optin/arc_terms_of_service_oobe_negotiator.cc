// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/optin/arc_terms_of_service_oobe_negotiator.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/ui/webui/chromeos/login/arc_terms_of_service_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"

namespace arc {

namespace {

chromeos::ArcTermsOfServiceScreenView* g_view_for_testing = nullptr;

chromeos::ArcTermsOfServiceScreenView* GetScreenView() {
  // Inject testing instance.
  if (g_view_for_testing)
    return g_view_for_testing;

  chromeos::LoginDisplayHost* host = chromeos::LoginDisplayHost::default_host();
  DCHECK(host);
  DCHECK(host->GetOobeUI());
  return host->GetOobeUI()->GetView<chromeos::ArcTermsOfServiceScreenHandler>();
}

}  // namespace

// static
void ArcTermsOfServiceOobeNegotiator::SetArcTermsOfServiceScreenViewForTesting(
    chromeos::ArcTermsOfServiceScreenView* view) {
  g_view_for_testing = view;
}

ArcTermsOfServiceOobeNegotiator::ArcTermsOfServiceOobeNegotiator() = default;

ArcTermsOfServiceOobeNegotiator::~ArcTermsOfServiceOobeNegotiator() {
  DCHECK(!screen_view_);
}

void ArcTermsOfServiceOobeNegotiator::StartNegotiationImpl() {
  DCHECK(!screen_view_);
  screen_view_ = GetScreenView();
  DCHECK(screen_view_);
  screen_view_->AddObserver(this);
}

void ArcTermsOfServiceOobeNegotiator::HandleTermsAccepted(bool accepted) {
  DCHECK(screen_view_);
  screen_view_->RemoveObserver(this);
  screen_view_ = nullptr;
  ReportResult(accepted);
}

void ArcTermsOfServiceOobeNegotiator::OnSkip() {
  HandleTermsAccepted(false);
}

void ArcTermsOfServiceOobeNegotiator::OnAccept(bool /* review_arc_settings */) {
  HandleTermsAccepted(true);
}

void ArcTermsOfServiceOobeNegotiator::OnViewDestroyed(
    chromeos::ArcTermsOfServiceScreenView* view) {
  DCHECK_EQ(view, screen_view_);
  HandleTermsAccepted(false);
}

}  // namespace arc
