// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_requirement_checker.h"

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "chrome/browser/ash/arc/arc_optin_uma.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/optin/arc_terms_of_service_default_negotiator.h"
#include "chrome/browser/ash/arc/optin/arc_terms_of_service_oobe_negotiator.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
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

// Updates UMA with user cancel only if error is not currently shown.
// TODO(hashimoto): Remove the duplicate in arc_session_manager.cc.
void MaybeUpdateOptInCancelUMA(const ArcSupportHost* support_host) {
  if (!support_host ||
      support_host->ui_page() == ArcSupportHost::UIPage::NO_PAGE ||
      support_host->ui_page() == ArcSupportHost::UIPage::ERROR) {
    return;
  }

  UpdateOptInCancelUMA(OptInCancelReason::USER_CANCEL);
}

}  // namespace

ArcRequirementChecker::ArcRequirementChecker(Delegate* delegate,
                                             Profile* profile,
                                             ArcSupportHost* support_host)
    : delegate_(delegate), profile_(profile), support_host_(support_host) {}

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

void ArcRequirementChecker::EmulateRequirementCheckCompletionForTesting() {
  if (state_ == State::kNegotiatingTermsOfService)
    OnTermsOfServiceNegotiated(true);
  if (state_ == State::kCheckingAndroidManagement) {
    OnAndroidManagementChecked(
        ArcAndroidManagementChecker::CheckResult::ALLOWED);
  }
}

void ArcRequirementChecker::StartRequirementChecks(
    bool is_terms_of_service_negotiation_needed) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::kStopped);
  DCHECK(profile_);
  DCHECK(!terms_of_service_negotiator_);

  state_ = State::kNegotiatingTermsOfService;

  if (!is_terms_of_service_negotiation_needed) {
    // Moves to next state, Android management check, immediately, as if
    // Terms of Service negotiation is done successfully.
    StartAndroidManagementCheck();
    return;
  }

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
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcRequirementChecker::StartBackgroundAndroidManagementCheck() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::kStopped);
  DCHECK(!android_management_checker_);

  state_ = State::kCheckingAndroidManagementBackground;

  android_management_checker_ = std::make_unique<ArcAndroidManagementChecker>(
      profile_, true /* retry_on_error */);
  android_management_checker_->StartCheck(base::BindOnce(
      &ArcRequirementChecker::OnBackgroundAndroidManagementChecked,
      weak_ptr_factory_.GetWeakPtr()));
}

void ArcRequirementChecker::OnTermsOfServiceNegotiated(bool accepted) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::kNegotiatingTermsOfService);
  DCHECK(profile_);
  DCHECK(terms_of_service_negotiator_ || !g_ui_enabled);
  terms_of_service_negotiator_.reset();

  if (!accepted) {
    VLOG(1) << "Terms of services declined";
    state_ = State::kStopped;
    // User does not accept the Terms of Service. Disable Google Play Store.
    MaybeUpdateOptInCancelUMA(support_host_);
    SetArcPlayStoreEnabledForProfile(profile_, false);
    return;
  }

  // Terms were accepted.
  VLOG(1) << "Terms of services accepted";
  profile_->GetPrefs()->SetBoolean(prefs::kArcTermsAccepted, true);
  StartAndroidManagementCheck();
}

void ArcRequirementChecker::StartAndroidManagementCheck() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::kNegotiatingTermsOfService);

  state_ = State::kCheckingAndroidManagement;

  // Show loading UI only if ARC support app's window is already shown.
  // User may not see any ARC support UI if everything needed is done in
  // background. In such a case, showing loading UI here (then closed sometime
  // soon later) would look just noisy.
  if (support_host_ &&
      support_host_->ui_page() != ArcSupportHost::UIPage::NO_PAGE) {
    support_host_->ShowArcLoading();
  }

  delegate_->OnArcOptInManagementCheckStarted();

  if (!g_ui_enabled)
    return;

  android_management_checker_ = std::make_unique<ArcAndroidManagementChecker>(
      profile_, false /* retry_on_error */);
  android_management_checker_->StartCheck(
      base::BindOnce(&ArcRequirementChecker::OnAndroidManagementChecked,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcRequirementChecker::OnAndroidManagementChecked(
    ArcAndroidManagementChecker::CheckResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::kCheckingAndroidManagement);
  DCHECK(android_management_checker_ || !g_ui_enabled);
  android_management_checker_.reset();
  state_ = State::kStopped;
  delegate_->OnAndroidManagementChecked(result);
}

void ArcRequirementChecker::OnBackgroundAndroidManagementChecked(
    ArcAndroidManagementChecker::CheckResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::kCheckingAndroidManagementBackground);
  DCHECK(android_management_checker_);
  android_management_checker_.reset();
  state_ = State::kStopped;
  delegate_->OnBackgroundAndroidManagementChecked(result);
}

}  // namespace arc
