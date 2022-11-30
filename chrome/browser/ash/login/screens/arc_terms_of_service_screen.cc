// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/arc_terms_of_service_screen.h"

#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/ash/login/arc_terms_of_service_screen_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace {

constexpr char kUserActionAcceptButtonClicked[] = "accept";
constexpr char kUserActionNextButtonClicked[] = "next";
constexpr char kUserActionRetryButtonClicked[] = "retry";
constexpr char kUserActionBackButtonClicked[] = "go-back";

constexpr char kUserActionMetricsLearnMoreClicked[] = "metrics-learn-more";
constexpr char kUserActionBackupRestoreLearnMoreClicked[] =
    "backup-restore-learn-more";
constexpr char kUserActionLocationServiceLearnMoreClicked[] =
    "location-service-learn-more";
constexpr char kUserActionPlayAutoInstallLearnMoreClicked[] =
    "play-auto-install-learn-more";
constexpr char kUserActionPolicyLinkClicked[] = "policy-link";

struct ArcTosUserAction {
  const char* name_;
  ArcTermsOfServiceScreen::UserAction uma_name_;
};

const ArcTosUserAction actions[] = {
    {kUserActionAcceptButtonClicked,
     ArcTermsOfServiceScreen::UserAction::kAcceptButtonClicked},
    {kUserActionNextButtonClicked,
     ArcTermsOfServiceScreen::UserAction::kNextButtonClicked},
    {kUserActionRetryButtonClicked,
     ArcTermsOfServiceScreen::UserAction::kRetryButtonClicked},
    {kUserActionBackButtonClicked,
     ArcTermsOfServiceScreen::UserAction::kBackButtonClicked},

    {kUserActionMetricsLearnMoreClicked,
     ArcTermsOfServiceScreen::UserAction::kMetricsLearnMoreClicked},
    {kUserActionBackupRestoreLearnMoreClicked,
     ArcTermsOfServiceScreen::UserAction::kBackupRestoreLearnMoreClicked},

    {kUserActionLocationServiceLearnMoreClicked,
     ArcTermsOfServiceScreen::UserAction::kLocationServiceLearnMoreClicked},
    {kUserActionPlayAutoInstallLearnMoreClicked,
     ArcTermsOfServiceScreen::UserAction::kPlayAutoInstallLearnMoreClicked},
    {kUserActionPolicyLinkClicked,
     ArcTermsOfServiceScreen::UserAction::kPolicyLinkClicked}

};

void RecordArcTosScreenAction(ArcTermsOfServiceScreen::UserAction value) {
  base::UmaHistogramEnumeration("OOBE.ArcTermsOfServiceScreen.UserActions",
                                value);
}

bool IsArcTosUserAction(const std::string& action_id) {
  for (const auto& el : actions) {
    if (action_id == el.name_)
      return true;
  }
  return false;
}

void RecordUserAction(const std::string& action_id) {
  for (const auto& el : actions) {
    if (action_id == el.name_) {
      RecordArcTosScreenAction(el.uma_name_);
      return;
    }
  }
  NOTREACHED() << "Unexpected action id: " << action_id;
}

}  // namespace

// static
std::string ArcTermsOfServiceScreen::GetResultString(Result result) {
  switch (result) {
    case Result::ACCEPTED:
    case Result::ACCEPTED_DEMO_ONLINE:
      return "Accepted";
    case Result::BACK:
      return "Back";
    case Result::NOT_APPLICABLE:
    case Result::NOT_APPLICABLE_DEMO_ONLINE:
    case Result::NOT_APPLICABLE_CONSOLIDATED_CONSENT_ARC_ENABLED:
      return BaseScreen::kNotApplicable;
  }
}

// static
void ArcTermsOfServiceScreen::MaybeLaunchArcSettings(Profile* profile) {
  if (profile->GetPrefs()->GetBoolean(prefs::kShowArcSettingsOnSessionStart)) {
    profile->GetPrefs()->ClearPref(prefs::kShowArcSettingsOnSessionStart);
    // TODO(jhorwich) Handle the case where the user chooses to review both ARC
    // settings and sync settings - currently the Settings window will only
    // show one settings page. See crbug.com/901184#c4 for details.
    chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
        profile, chromeos::settings::mojom::kGooglePlayStoreSubpagePath);
  }
}

ArcTermsOfServiceScreen::ArcTermsOfServiceScreen(
    base::WeakPtr<ArcTermsOfServiceScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(ArcTermsOfServiceScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  view_->AddObserver(this);
}

ArcTermsOfServiceScreen::~ArcTermsOfServiceScreen() {
  if (view_) {
    view_->RemoveObserver(this);
  }
}

bool ArcTermsOfServiceScreen::MaybeSkip(WizardContext& context) {
  if (context.skip_post_login_screens_for_tests) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }

  if (features::IsOobeConsolidatedConsentEnabled()) {
    // In demo mode, the ARC-ToS screen is skipped and shown later in the
    // consolidated consent screen,
    const auto* const demo_setup_controller =
        WizardController::default_controller()->demo_setup_controller();
    if (demo_setup_controller) {
      exit_callback_.Run(Result::NOT_APPLICABLE_DEMO_ONLINE);
      return true;
    }

    // In regular flow, if ARC is enabled, then the user has already accepted
    // ARC-ToS in the consolidated consent screen earlier in the flow.
    Profile* const profile = ProfileManager::GetActiveUserProfile();
    if (arc::IsArcPlayStoreEnabledForProfile(profile)) {
      exit_callback_.Run(
          Result::NOT_APPLICABLE_CONSOLIDATED_CONSENT_ARC_ENABLED);
    } else {
      exit_callback_.Run(Result::NOT_APPLICABLE);
    }
    return true;
  }

  if (!arc::IsArcTermsOfServiceOobeNegotiationNeeded()) {
    const auto* const demo_setup_controller =
        WizardController::default_controller()->demo_setup_controller();

    if (!demo_setup_controller) {
      exit_callback_.Run(Result::NOT_APPLICABLE);
    } else {
      exit_callback_.Run(Result::NOT_APPLICABLE_DEMO_ONLINE);
    }
    return true;
  }
  return false;
}

void ArcTermsOfServiceScreen::ShowImpl() {
  if (!view_)
    return;

  ProfileManager::GetActiveUserProfile()->GetPrefs()->SetBoolean(
      arc::prefs::kArcTermsShownInOobe, true);
  // Show the screen.
  view_->Show();
}

void ArcTermsOfServiceScreen::HideImpl() {
  if (view_)
    view_->Hide();
}

void ArcTermsOfServiceScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (!IsArcTosUserAction(action_id)) {
    BaseScreen::OnUserAction(args);
    return;
  }
  RecordUserAction(action_id);
  if (action_id == kUserActionBackButtonClicked)
    exit_callback_.Run(Result::BACK);
}

void ArcTermsOfServiceScreen::OnAccept(bool review_arc_settings) {
  if (is_hidden())
    return;
  base::UmaHistogramBoolean("OOBE.ArcTermsOfServiceScreen.ReviewFollowingSetup",
                            review_arc_settings);
  if (review_arc_settings) {
    Profile* const profile = ProfileManager::GetActiveUserProfile();
    CHECK(profile);
    profile->GetPrefs()->SetBoolean(prefs::kShowArcSettingsOnSessionStart,
                                    true);
  }

  const DemoSetupController* const demo_setup_controller =
      WizardController::default_controller()->demo_setup_controller();

  if (!demo_setup_controller) {
    exit_callback_.Run(Result::ACCEPTED);
  } else {
    exit_callback_.Run(Result::ACCEPTED_DEMO_ONLINE);
  }
}

void ArcTermsOfServiceScreen::OnViewDestroyed(
    ArcTermsOfServiceScreenView* view) {
  DCHECK_EQ(view, view_.get());
  view_->RemoveObserver(this);
  view_ = nullptr;
}

}  // namespace ash
