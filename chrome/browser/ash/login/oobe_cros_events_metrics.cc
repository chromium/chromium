// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_cros_events_metrics.h"

#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"

namespace ash {

namespace {
bool IsFlexFlow() {
  return switches::IsOsInstallAllowed() || switches::IsRevenBranding();
}

bool IsDemoModeFlow() {
  return DemoSetupController::IsOobeDemoSetupFlowInProgress() ||
         DemoSession::IsDeviceInDemoMode();
}

bool IsOwnerUser() {
  auto* user_manager = user_manager::UserManager::Get();
  if (!user_manager) {
    return false;
  }
  return !ash::InstallAttributes::Get()->IsEnterpriseManaged() &&
         (user_manager->IsCurrentUserOwner() ||
          user_manager->GetUsers().size() == 1);
}

bool IsEphemeralOrMGS() {
  return chrome_user_manager_util::IsManagedGuestSessionOrEphemeralLogin();
}

bool IsFirstOnboarding() {
  // OOBE start time is cleared after the completion of the first onboarding.
  base::Time oobe_time =
      g_browser_process->local_state()->GetTime(prefs::kOobeStartTime);
  return !oobe_time.is_null();
}

namespace cros_events = metrics::structured::events::v2::cr_os_events;
}  // namespace

OobeCrosEventsMetrics::OobeCrosEventsMetrics(
    OobeMetricsHelper* oobe_metrics_helper) {
  oobe_metrics_helper->AddObserver(this);
}

OobeCrosEventsMetrics::~OobeCrosEventsMetrics() = default;

void OobeCrosEventsMetrics::OnPreLoginOobeFirstStarted() {
  metrics::structured::StructuredMetricsClient::Record(std::move(
      cros_events::OOBE_OobeStarted()
          .SetIsFlexFlow(IsFlexFlow())
          .SetChromeMilestone(version_info::GetMajorVersionNumberAsInt())));
}

void OobeCrosEventsMetrics::OnPreLoginOobeCompleted(
    OobeMetricsHelper::CompletedPreLoginOobeFlowType flow_type) {
  metrics::structured::StructuredMetricsClient::Record(std::move(
      cros_events::OOBE_PreLoginOobeCompleted()
          .SetCompletedFlowType(static_cast<int>(flow_type))
          .SetIsFlexFlow(IsFlexFlow())
          .SetIsDemoModeFlow(IsDemoModeFlow())
          .SetChromeMilestone(version_info::GetMajorVersionNumberAsInt())));
}

void OobeCrosEventsMetrics::OnOnboardingStarted() {
  metrics::structured::StructuredMetricsClient::Record(std::move(
      cros_events::OOBE_OnboardingStarted()
          .SetIsFlexFlow(IsFlexFlow())
          .SetIsDemoModeFlow(IsDemoModeFlow())
          .SetIsOwnerUser(IsOwnerUser())
          .SetIsEphemeralOrMGS(IsEphemeralOrMGS())
          .SetChromeMilestone(version_info::GetMajorVersionNumberAsInt())
          .SetIsFirstOnboarding(IsFirstOnboarding())));
}

void OobeCrosEventsMetrics::OnOnboardingCompleted() {
  metrics::structured::StructuredMetricsClient::Record(std::move(
      cros_events::OOBE_OnboardingCompleted()
          .SetIsFlexFlow(IsFlexFlow())
          .SetIsDemoModeFlow(IsDemoModeFlow())
          .SetIsOwnerUser(IsOwnerUser())
          .SetIsEphemeralOrMGS(IsEphemeralOrMGS())
          .SetIsFirstOnboarding(IsFirstOnboarding())
          .SetChromeMilestone(version_info::GetMajorVersionNumberAsInt())));

  if (IsFirstOnboarding()) {
    metrics::structured::StructuredMetricsClient::Record(std::move(
        cros_events::OOBE_OobeCompleted()
            .SetIsFlexFlow(IsFlexFlow())
            .SetIsDemoModeFlow(IsDemoModeFlow())
            .SetIsOwnerUser(IsOwnerUser())
            .SetIsEphemeralOrMGS(IsEphemeralOrMGS())
            .SetIsFirstOnboarding(IsFirstOnboarding())
            .SetChromeMilestone(version_info::GetMajorVersionNumberAsInt())));
  }
}

void OobeCrosEventsMetrics::OnDeviceRegistered() {
  metrics::structured::StructuredMetricsClient::Record(std::move(
      cros_events::OOBE_DeviceRegistered()
          .SetIsFlexFlow(IsFlexFlow())
          .SetIsDemoModeFlow(IsDemoModeFlow())
          .SetIsFirstOnboarding(IsFirstOnboarding())
          .SetChromeMilestone(version_info::GetMajorVersionNumberAsInt())));
}

void OobeCrosEventsMetrics::OnScreenShownStatusChanged(
    OobeScreenId screen,
    OobeMetricsHelper::ScreenShownStatus status) {
  if (status == OobeMetricsHelper::ScreenShownStatus::kShown) {
    metrics::structured::StructuredMetricsClient::Record(std::move(
        cros_events::OOBE_PageEntered()
            .SetPageId(screen.name)
            .SetIsFlexFlow(IsFlexFlow())
            .SetIsDemoModeFlow(IsDemoModeFlow())
            .SetIsOwnerUser(IsOwnerUser())
            .SetIsEphemeralOrMGS(IsEphemeralOrMGS())
            .SetIsFirstOnboarding(IsFirstOnboarding())
            .SetChromeMilestone(version_info::GetMajorVersionNumberAsInt())));
  } else {
    metrics::structured::StructuredMetricsClient::Record(std::move(
        cros_events::OOBE_PageSkippedBySystem()
            .SetPageId(screen.name)
            .SetIsFlexFlow(IsFlexFlow())
            .SetIsDemoModeFlow(IsDemoModeFlow())
            .SetIsOwnerUser(IsOwnerUser())
            .SetIsEphemeralOrMGS(IsEphemeralOrMGS())
            .SetIsFirstOnboarding(IsFirstOnboarding())
            .SetChromeMilestone(version_info::GetMajorVersionNumberAsInt())));
  }
}

void OobeCrosEventsMetrics::OnScreenExited(OobeScreenId screen,
                                           const std::string& exit_reason) {
  metrics::structured::StructuredMetricsClient::Record(std::move(
      cros_events::OOBE_PageLeft()
          .SetPageId(screen.name)
          .SetExitReason(exit_reason)
          .SetIsFlexFlow(IsFlexFlow())
          .SetIsDemoModeFlow(IsDemoModeFlow())
          .SetIsOwnerUser(IsOwnerUser())
          .SetIsEphemeralOrMGS(IsEphemeralOrMGS())
          .SetIsFirstOnboarding(IsFirstOnboarding())
          .SetChromeMilestone(version_info::GetMajorVersionNumberAsInt())));
}

void OobeCrosEventsMetrics::OnGaiaSignInRequested(
    GaiaView::GaiaLoginVariant variant) {
  metrics::structured::StructuredMetricsClient::Record(std::move(
      cros_events::OOBE_GaiaSigninRequested()
          .SetIsReauthentication(variant ==
                                 GaiaView::GaiaLoginVariant::kOnlineSignin)
          .SetIsFlexFlow(IsFlexFlow())
          .SetIsDemoModeFlow(IsDemoModeFlow())
          .SetIsOwnerUser(IsOwnerUser())
          .SetIsEphemeralOrMGS(IsEphemeralOrMGS())
          .SetIsFirstOnboarding(IsFirstOnboarding())
          .SetChromeMilestone(version_info::GetMajorVersionNumberAsInt())));
}

void OobeCrosEventsMetrics::OnGaiaSignInCompleted(
    GaiaView::GaiaLoginVariant variant) {
  metrics::structured::StructuredMetricsClient::Record(std::move(
      cros_events::OOBE_GaiaSigninCompleted()
          .SetIsReauthentication(variant ==
                                 GaiaView::GaiaLoginVariant::kOnlineSignin)
          .SetIsFlexFlow(IsFlexFlow())
          .SetIsDemoModeFlow(IsDemoModeFlow())
          .SetIsOwnerUser(IsOwnerUser())
          .SetIsEphemeralOrMGS(IsEphemeralOrMGS())
          .SetIsFirstOnboarding(IsFirstOnboarding())
          .SetChromeMilestone(version_info::GetMajorVersionNumberAsInt())));
}

void OobeCrosEventsMetrics::OnPreLoginOobeResumed(OobeScreenId screen) {
  metrics::structured::StructuredMetricsClient::Record(std::move(
      cros_events::OOBE_PreLoginOobeResumed()
          .SetPendingPageId(screen.name)
          .SetIsFlexFlow(IsFlexFlow())
          .SetIsDemoModeFlow(IsDemoModeFlow())
          .SetIsOwnerUser(IsOwnerUser())
          .SetIsEphemeralOrMGS(IsEphemeralOrMGS())
          .SetIsFirstOnboarding(IsFirstOnboarding())
          .SetChromeMilestone(version_info::GetMajorVersionNumberAsInt())));
}

void OobeCrosEventsMetrics::OnOnboardingResumed(OobeScreenId screen) {
  metrics::structured::StructuredMetricsClient::Record(std::move(
      cros_events::OOBE_OnboardingResumed()
          .SetPendingPageId(screen.name)
          .SetIsFlexFlow(IsFlexFlow())
          .SetIsDemoModeFlow(IsDemoModeFlow())
          .SetIsOwnerUser(IsOwnerUser())
          .SetIsEphemeralOrMGS(IsEphemeralOrMGS())
          .SetIsFirstOnboarding(IsFirstOnboarding())
          .SetChromeMilestone(version_info::GetMajorVersionNumberAsInt())));
}

void OobeCrosEventsMetrics::OnChoobeResumed() {
  metrics::structured::StructuredMetricsClient::Record(std::move(
      cros_events::OOBE_ChoobeResumed()
          .SetIsFlexFlow(IsFlexFlow())
          .SetIsDemoModeFlow(IsDemoModeFlow())
          .SetIsOwnerUser(IsOwnerUser())
          .SetIsEphemeralOrMGS(IsEphemeralOrMGS())
          .SetIsFirstOnboarding(IsFirstOnboarding())
          .SetChromeMilestone(version_info::GetMajorVersionNumberAsInt())));
}

}  // namespace ash
