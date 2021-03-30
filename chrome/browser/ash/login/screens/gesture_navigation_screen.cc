// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/gesture_navigation_screen.h"

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/tablet_mode.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace {

constexpr const char kUserActionExitPressed[] = "exit";

// The name used for each page on the gesture navigation screen.
constexpr const char kGestureIntroPage[] = "gestureIntro";
constexpr const char kGestureHomePage[] = "gestureHome";
constexpr const char kGestureOverviewPage[] = "gestureOverview";
constexpr const char kGestureBackPage[] = "gestureBack";

}  // namespace

// static
std::string GestureNavigationScreen::GetResultString(Result result) {
  switch (result) {
    case Result::NEXT:
      return "Next";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
}

GestureNavigationScreen::GestureNavigationScreen(
    GestureNavigationScreenView* view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(GestureNavigationScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  view_->Bind(this);
}

GestureNavigationScreen::~GestureNavigationScreen() {
  if (view_)
    view_->Bind(nullptr);
}

void GestureNavigationScreen::GesturePageChange(const std::string& new_page) {
  page_times_[current_page_] += base::TimeTicks::Now() - start_time_;
  start_time_ = base::TimeTicks::Now();
  current_page_ = new_page;
}

bool GestureNavigationScreen::MaybeSkip(WizardContext* context) {
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  if (chrome_user_manager_util::IsPublicSessionOrEphemeralLogin() ||
      !ash::features::IsHideShelfControlsInTabletModeEnabled() ||
      ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
          ash::prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled) ||
      accessibility_manager->IsSpokenFeedbackEnabled() ||
      accessibility_manager->IsAutoclickEnabled() ||
      accessibility_manager->IsSwitchAccessEnabled()) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }

  // Skip the screen if the device is not in tablet mode, unless tablet mode
  // first user run is forced on the device.
  if (!ash::TabletMode::Get()->InTabletMode() &&
      !chromeos::switches::ShouldOobeUseTabletModeFirstRun()) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }
  return false;
}

void GestureNavigationScreen::ShowImpl() {
  // Begin keeping track of current page and start time for the page shown time
  // metrics.
  current_page_ = kGestureIntroPage;
  start_time_ = base::TimeTicks::Now();

  view_->Show();
}

void GestureNavigationScreen::HideImpl() {
  view_->Hide();
}

void GestureNavigationScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionExitPressed) {
    // Make sure the user does not see a notification about the new gestures
    // since they have already gone through this gesture education screen.
    ProfileManager::GetActiveUserProfile()->GetPrefs()->SetBoolean(
        ash::prefs::kGestureEducationNotificationShown, true);

    RecordPageShownTimeMetrics();
    was_shown_ = true;
    exit_callback_.Run(Result::NEXT);
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

void GestureNavigationScreen::RecordPageShownTimeMetrics() {
  page_times_[current_page_] += base::TimeTicks::Now() - start_time_;

  UmaHistogramMediumTimes("OOBE.GestureNavigationScreen.PageShownTime.Intro",
                          page_times_[kGestureIntroPage]);
  UmaHistogramMediumTimes("OOBE.GestureNavigationScreen.PageShownTime.Home",
                          page_times_[kGestureHomePage]);
  UmaHistogramMediumTimes("OOBE.GestureNavigationScreen.PageShownTime.Overview",
                          page_times_[kGestureOverviewPage]);
  UmaHistogramMediumTimes("OOBE.GestureNavigationScreen.PageShownTime.Back",
                          page_times_[kGestureBackPage]);
}

}  // namespace chromeos
