// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_requirement_checker.h"

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "chrome/browser/ash/arc/arc_optin_uma.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/optin/arc_terms_of_service_default_negotiator.h"
#include "chrome/browser/ash/arc/optin/arc_terms_of_service_oobe_negotiator.h"
#include "chrome/browser/ash/arc/policy/arc_policy_util.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace arc {

namespace {

constexpr base::TimeDelta kWaitForPoliciesTimeout = base::Seconds(20);

// Flags used to control behaviors for tests.
// TODO(b/241886729): Remove or simplify these flags.

// Allows the session manager to skip creating UI in unit tests.
bool g_ui_enabled = true;

// Allows the session manager to create ArcTermsOfServiceOobeNegotiator in
// tests, even when the tests are set to skip creating UI.
bool g_enable_arc_terms_of_service_oobe_negotiator_in_tests = false;

std::optional<bool> g_enable_check_android_management_in_tests;

policy::DeviceManagementService* GetDeviceManagementService() {
  policy::BrowserPolicyConnectorAsh* const connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  return connector->device_management_service();
}

// Returns the Device Account Id. Assumes that |profile| is the only Profile
// on Chrome OS.
CoreAccountId GetDeviceAccountId(Profile* profile) {
  const auto* const identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  // The account is the same whether or not the user consented to browser sync.
  return identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
}

std::unique_ptr<ArcAndroidManagementChecker> CreateAndroidManagementChecker(
    Profile* profile,
    bool retry_on_error) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  const CoreAccountId device_account_id = GetDeviceAccountId(profile);
  return std::make_unique<ArcAndroidManagementChecker>(
      profile, identity_manager, device_account_id, retry_on_error,
      std::make_unique<policy::AndroidManagementClientImpl>(
          GetDeviceManagementService(),
          g_browser_process->system_network_context_manager()
              ->GetSharedURLLoaderFactory(),
          device_account_id, identity_manager));
}

}  // namespace

// static
ArcRequirementChecker::AndroidManagementCheckerFactory
ArcRequirementChecker::GetDefaultAndroidManagementCheckerFactory() {
  return base::BindRepeating(&CreateAndroidManagementChecker);
}

ArcRequirementChecker::ArcRequirementChecker(
    Profile* profile,
    ArcSupportHost* support_host,
    AndroidManagementCheckerFactory android_management_checker_factory)
    : profile_(profile),
      support_host_(support_host),
      android_management_checker_factory_(android_management_checker_factory) {}

ArcRequirementChecker::~ArcRequirementChecker() {
  profile_->GetProfilePolicyConnector()->policy_service()->RemoveObserver(
      policy::POLICY_DOMAIN_CHROME, this);
}

void ArcRequirementChecker::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ArcRequirementChecker::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

// static
void ArcRequirementChecker::SetUiEnabledForTesting(bool enabled) {
  g_ui_enabled = enabled;
}

// static
void ArcRequirementChecker::SetArcTermsOfServiceOobeNegotiatorEnabledForTesting(
    bool enabled) {
  g_enable_arc_terms_of_service_oobe_negotiator_in_tests = enabled;
}

// static
void ArcRequirementChecker::EnableCheckAndroidManagementForTesting(
    bool enable) {
  g_enable_check_android_management_in_tests = enable;
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
    bool is_terms_of_service_negotiation_needed,
    StartRequirementChecksCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::kStopped);
  DCHECK(profile_);
  DCHECK(!terms_of_service_negotiator_);
  DCHECK(!requirement_check_callback_);

  state_ = State::kNegotiatingTermsOfService;
  requirement_check_callback_ = std::move(callback);

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
            profile_->GetPrefs(), support_host_,
            g_browser_process->metrics_service());
  } else {
    DCHECK(!g_ui_enabled) << "Negotiator is not created on production.";
    return;
  }

  terms_of_service_negotiator_->StartNegotiation(
      base::BindOnce(&ArcRequirementChecker::OnTermsOfServiceNegotiated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcRequirementChecker::StartBackgroundChecks(
    StartBackgroundChecksCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::kStopped);
  DCHECK(!android_management_checker_);
  DCHECK(!background_check_callback_);
  DCHECK(!wait_for_policy_timer_.IsRunning());

  state_ = State::kCheckingAndroidManagementBackground;
  background_check_callback_ = std::move(callback);

  // Skip Android management check for testing.
  if (!g_enable_check_android_management_in_tests.value_or(g_ui_enabled))
    return;

  android_management_checker_ = android_management_checker_factory_.Run(
      profile_.get(), true /* retry_on_error */);
  android_management_checker_->StartCheck(base::BindOnce(
      &ArcRequirementChecker::OnBackgroundAndroidManagementChecked,
      weak_ptr_factory_.GetWeakPtr()));
}

void ArcRequirementChecker::OnFirstPoliciesLoaded(policy::PolicyDomain domain) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::kWaitingForPoliciesBackground);
  DCHECK_EQ(domain, policy::POLICY_DOMAIN_CHROME);

  wait_for_policy_timer_.Stop();
  OnFirstPoliciesLoadedOrTimeout();
}

void ArcRequirementChecker::OnTermsOfServiceNegotiated(bool accepted) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::kNegotiatingTermsOfService);
  DCHECK(profile_);
  DCHECK(terms_of_service_negotiator_ || !g_ui_enabled);
  DCHECK(requirement_check_callback_);
  terms_of_service_negotiator_.reset();

  if (!accepted) {
    VLOG(1) << "Terms of services declined";
    state_ = State::kStopped;
    std::move(requirement_check_callback_)
        .Run(RequirementCheckResult::kTermsOfServicesDeclined);
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

  for (auto& observer : observers_)
    observer.OnArcOptInManagementCheckStarted();

  if (!g_ui_enabled)
    return;

  android_management_checker_ = android_management_checker_factory_.Run(
      profile_.get(), false /* retry_on_error */);
  android_management_checker_->StartCheck(
      base::BindOnce(&ArcRequirementChecker::OnAndroidManagementChecked,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcRequirementChecker::OnAndroidManagementChecked(
    ArcAndroidManagementChecker::CheckResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::kCheckingAndroidManagement);
  DCHECK(android_management_checker_ || !g_ui_enabled);
  DCHECK(requirement_check_callback_);
  android_management_checker_.reset();
  state_ = State::kStopped;

  switch (result) {
    case ArcAndroidManagementChecker::CheckResult::ALLOWED:
      std::move(requirement_check_callback_).Run(RequirementCheckResult::kOk);
      return;
    case ArcAndroidManagementChecker::CheckResult::DISALLOWED:
      std::move(requirement_check_callback_)
          .Run(RequirementCheckResult::kDisallowedByAndroidManagement);
      return;
    case ArcAndroidManagementChecker::CheckResult::ERROR:
      std::move(requirement_check_callback_)
          .Run(RequirementCheckResult::kAndroidManagementCheckError);
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void ArcRequirementChecker::OnBackgroundAndroidManagementChecked(
    ArcAndroidManagementChecker::CheckResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::kCheckingAndroidManagementBackground);
  DCHECK(background_check_callback_);

  if (g_enable_check_android_management_in_tests.value_or(true)) {
    DCHECK(android_management_checker_);
    android_management_checker_.reset();
  }

  switch (result) {
    case ArcAndroidManagementChecker::CheckResult::ALLOWED:
      // Do nothing. ARC should be started already.
      state_ = State::kStopped;
      std::move(background_check_callback_)
          .Run(BackgroundCheckResult::kNoActionRequired);
      break;
    case ArcAndroidManagementChecker::CheckResult::DISALLOWED:
      if (base::FeatureList::IsEnabled(
              arc::kEnableUnmanagedToManagedTransitionFeature)) {
        state_ = State::kWaitingForPoliciesBackground;
        WaitForPoliciesLoad();
      } else {
        state_ = State::kStopped;
        std::move(background_check_callback_)
            .Run(BackgroundCheckResult::kArcShouldBeDisabled);
      }
      break;
    case ArcAndroidManagementChecker::CheckResult::ERROR:
      // This code should not be reached. For background check,
      // retry_on_error should be set.
      NOTREACHED_IN_MIGRATION();
  }
}

void ArcRequirementChecker::WaitForPoliciesLoad() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::kWaitingForPoliciesBackground);

  auto* policy_service =
      profile_->GetProfilePolicyConnector()->policy_service();

  // User might be transitioning to managed state, wait for policies load
  // to confirm.
  if (policy_service->IsFirstPolicyLoadComplete(policy::POLICY_DOMAIN_CHROME)) {
    OnFirstPoliciesLoadedOrTimeout();
  } else {
    profile_->GetProfilePolicyConnector()->policy_service()->AddObserver(
        policy::POLICY_DOMAIN_CHROME, this);
    wait_for_policy_timer_.Start(
        FROM_HERE, kWaitForPoliciesTimeout,
        base::BindOnce(&ArcRequirementChecker::OnFirstPoliciesLoadedOrTimeout,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void ArcRequirementChecker::OnFirstPoliciesLoadedOrTimeout() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::kWaitingForPoliciesBackground);
  DCHECK(background_check_callback_);

  state_ = State::kStopped;

  profile_->GetProfilePolicyConnector()->policy_service()->RemoveObserver(
      policy::POLICY_DOMAIN_CHROME, this);

  // OnFirstPoliciesLoaded callback is triggered for both unmanaged and managed
  // users, we need to check user state here.
  // If timeout comes before policies are loaded, we fallback to calling
  // SetArcPlayStoreEnabledForProfile(profile_, false).
  if (arc::policy_util::IsAccountManaged(profile_)) {
    // User has become managed, notify ARC by setting transition preference,
    // which is eventually passed to ARC via ArcSession parameters.
    profile_->GetPrefs()->SetInteger(
        arc::prefs::kArcManagementTransition,
        static_cast<int>(arc::ArcManagementTransition::UNMANAGED_TO_MANAGED));

    // Restart ARC to perform managed re-provisioning.
    // kArcIsManaged and kArcSignedIn are not reset during the restart.
    // In case of successful re-provisioning, OnProvisioningFinished is called
    // and kArcIsManaged is updated.
    // In case of re-provisioning failure, ARC data is removed and transition
    // preference is reset.
    // In case Chrome is terminated during re-provisioning, user transition will
    // be detected in ProfileManager::InitProfileUserPrefs, on next startup.
    std::move(background_check_callback_)
        .Run(BackgroundCheckResult::kArcShouldBeRestarted);
  } else {
    std::move(background_check_callback_)
        .Run(BackgroundCheckResult::kArcShouldBeDisabled);
  }
}

}  // namespace arc
