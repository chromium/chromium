// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/management_transition_screen.h"

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/session/arc_management_transition.h"
#include "ash/public/cpp/login_screen.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/system/system_tray_client_impl.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/webui/ash/login/management_transition_screen_handler.h"

namespace ash {

namespace {

constexpr base::TimeDelta kWaitingTimeout = base::Minutes(5);

// Management transition screen step names.
constexpr const char kUserActionfinishManagementTransition[] =
    "finish-management-transition";

}  // namespace

ManagementTransitionScreen::ManagementTransitionScreen(
    base::WeakPtr<ManagementTransitionScreenView> view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(ManagementTransitionScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

ManagementTransitionScreen::~ManagementTransitionScreen() {
  timer_.Stop();
}

void ManagementTransitionScreen::ShowImpl() {
  screen_shown_time_ = base::TimeTicks::Now();
  timer_.Start(
      FROM_HERE, kWaitingTimeout,
      base::BindOnce(&ManagementTransitionScreen::OnManagementTransitionFailed,
                     weak_factory_.GetWeakPtr()));

  Profile* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(!ProfileHelper::IsSigninProfile(profile));

  registrar_.Init(profile->GetPrefs());
  registrar_.Add(
      arc::prefs::kArcManagementTransition,
      base::BindRepeating(
          &ManagementTransitionScreen::OnManagementTransitionFinished,
          weak_factory_.GetWeakPtr()));

  // Disable system tray, shutdown button and prevent login as guest when
  // management transition screen is shown.
  SystemTrayClientImpl::Get()->SetPrimaryTrayEnabled(false);
  LoginScreen::Get()->EnableShutdownButton(false);
  LoginScreen::Get()->SetAllowLoginAsGuest(false);
  LoginScreen::Get()->SetIsFirstSigninStep(false);

  arc::ArcManagementTransition arc_management_transition =
      arc::GetManagementTransition(profile);
  std::string management_entity =
      chrome::GetAccountManagerIdentity(profile).value_or(std::string());

  if (view_)
    view_->Show(arc_management_transition, std::move(management_entity));
}

void ManagementTransitionScreen::OnManagementTransitionFailed() {
  LOG(ERROR) << "Management transition failed; resetting ARC++ data.";
  // Prevent ARC++ data removal below from triggering the success flow (since it
  // will reset the management transition pref).
  registrar_.RemoveAll();
  timer_.Stop();
  timed_out_ = true;
  arc::ArcSessionManager::Get()->RequestArcDataRemoval();
  arc::ArcSessionManager::Get()->StopAndEnableArc();
  if (view_) {
    view_->ShowError();
  }
}

void ManagementTransitionScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();

  if (action_id == kUserActionfinishManagementTransition) {
    OnManagementTransitionFinished();
    return;
  }
  BaseScreen::OnUserAction(args);
}

base::OneShotTimer* ManagementTransitionScreen::GetTimerForTesting() {
  return &timer_;
}

void ManagementTransitionScreen::HideImpl() {
  timer_.Stop();
}

void ManagementTransitionScreen::OnManagementTransitionFinished() {
  // This method is called both when management transition succeeds (observing
  // pref changes) and when it fails ("OK" button from error screen, see
  // RegisterMessages()). Once this screen exits, user session will be started,
  // so there's no need to re-enable shutdown button from login screen, only the
  // system tray.
  SystemTrayClientImpl::Get()->SetPrimaryTrayEnabled(true);

  exit_callback_.Run();

  base::UmaHistogramBoolean("Arc.Supervision.Transition.Screen.Successful",
                            !timed_out_);
  if (!timed_out_) {
    base::TimeDelta timeDelta = base::TimeTicks::Now() - screen_shown_time_;
    DVLOG(1) << "Transition succeeded in: " << timeDelta.InSecondsF();
    base::UmaHistogramMediumTimes(
        "Arc.Supervision.Transition.Screen.Success.TimeDelta", timeDelta);
  }
}

}  // namespace ash
