// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/arc_terms_of_service_screen.h"

#include "base/feature_list.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/chromeos/login/arc_terms_of_service_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"

namespace {

constexpr char kUserActionBack[] = "go-back";

}  // namespace

namespace chromeos {

// static
void ArcTermsOfServiceScreen::MaybeLaunchArcSettings(Profile* profile) {
  if (profile->GetPrefs()->GetBoolean(prefs::kShowArcSettingsOnSessionStart)) {
    profile->GetPrefs()->ClearPref(prefs::kShowArcSettingsOnSessionStart);
    // TODO(jhorwich) Handle the case where the user chooses to review both ARC
    // settings and sync settings - currently the Settings window will only
    // show one settings page. See crbug.com/901184#c4 for details.
    if (base::FeatureList::IsEnabled(chromeos::features::kSplitSettings)) {
      chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
          profile, chrome::kAndroidAppsDetailsSubPage);
    } else {
      chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
          profile, chrome::kAndroidAppsDetailsSubPageInBrowserSettings);
    }
  }
}

ArcTermsOfServiceScreen::ArcTermsOfServiceScreen(
    ArcTermsOfServiceScreenView* view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(ArcTermsOfServiceScreenView::kScreenId),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  if (view_) {
    view_->AddObserver(this);
    view_->Bind(this);
  }
}

ArcTermsOfServiceScreen::~ArcTermsOfServiceScreen() {
  if (view_) {
    view_->RemoveObserver(this);
    view_->Bind(nullptr);
  }
}

void ArcTermsOfServiceScreen::Show() {
  if (!view_)
    return;

  // Show the screen.
  view_->Show();
}

void ArcTermsOfServiceScreen::Hide() {
  if (view_)
    view_->Hide();
}

void ArcTermsOfServiceScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionBack) {
    exit_callback_.Run(Result::BACK);
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

void ArcTermsOfServiceScreen::OnSkip() {
  exit_callback_.Run(Result::SKIPPED);
}

void ArcTermsOfServiceScreen::OnAccept(bool review_arc_settings) {
  if (review_arc_settings) {
    Profile* const profile = ProfileManager::GetActiveUserProfile();
    CHECK(profile);
    profile->GetPrefs()->SetBoolean(prefs::kShowArcSettingsOnSessionStart,
                                    true);
  }
  exit_callback_.Run(Result::ACCEPTED);
}

void ArcTermsOfServiceScreen::OnViewDestroyed(
    ArcTermsOfServiceScreenView* view) {
  DCHECK_EQ(view, view_);
  view_->RemoveObserver(this);
  view_ = nullptr;
}

}  // namespace chromeos
