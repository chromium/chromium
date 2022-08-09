// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_requirement_checker.h"

#include "ash/components/arc/arc_util.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/optin/arc_terms_of_service_default_negotiator.h"
#include "chrome/browser/ash/arc/optin/arc_terms_of_service_oobe_negotiator.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

namespace arc {

namespace {

// Flags used to control behaviors for tests.
// TODO(b/241886729): Remove or simplify these flags.

// Allows the session manager to skip creating UI in unit tests.
bool g_ui_enabled = true;

// Allows the session manager to create ArcTermsOfServiceOobeNegotiator in
// tests, even when the tests are set to skip creating UI.
bool g_enable_arc_terms_of_service_oobe_negotiator_in_tests = false;

}  // namespace

ArcRequirementChecker::ArcRequirementChecker(Profile* profile,
                                             ArcSupportHost* support_host)
    : profile_(profile), support_host_(support_host) {}

ArcRequirementChecker::~ArcRequirementChecker() = default;

// static
void ArcRequirementChecker::SetUiEnabledForTesting(bool enabled) {
  g_ui_enabled = enabled;
}

// static
void ArcRequirementChecker::SetArcTermsOfServiceOobeNegotiatorEnabledForTesting(
    bool enabled) {
  g_enable_arc_terms_of_service_oobe_negotiator_in_tests = enabled;
}

void ArcRequirementChecker::StartTermsOfServiceNegotiation(
    BoolCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);
  DCHECK(!terms_of_service_negotiator_);

  if (IsArcOobeOptInActive()) {
    if (!g_enable_arc_terms_of_service_oobe_negotiator_in_tests &&
        !g_ui_enabled) {
      return;
    }
    VLOG(1) << "Use OOBE negotiator.";
    terms_of_service_negotiator_ =
        std::make_unique<ArcTermsOfServiceOobeNegotiator>();
  } else if (support_host_) {
    VLOG(1) << "Use default negotiator.";
    terms_of_service_negotiator_ =
        std::make_unique<ArcTermsOfServiceDefaultNegotiator>(
            profile_->GetPrefs(), support_host_);
  } else {
    DCHECK(!g_ui_enabled) << "Negotiator is not created on production.";
    return;
  }

  terms_of_service_negotiator_->StartNegotiation(
      base::BindOnce(&ArcRequirementChecker::OnTermsOfServiceNegotiated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcRequirementChecker::OnTermsOfServiceNegotiated(BoolCallback callback,
                                                       bool accepted) {
  DCHECK(profile_);
  DCHECK(terms_of_service_negotiator_);
  terms_of_service_negotiator_.reset();
  std::move(callback).Run(accepted);
}

}  // namespace arc
