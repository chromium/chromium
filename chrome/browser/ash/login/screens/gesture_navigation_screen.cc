// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/gesture_navigation_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/tablet_mode.h"
#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/gesture_navigation_screen_handler.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace {

constexpr const char kUserActionExitPressed[] = "exit";
constexpr const char kUserActionSkip[] = "skip";
constexpr const char kUserActionGesturePageChange[] = "gesture-page-change";

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
    case Result::SKIP:
      return "Skip";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
}

GestureNavigationScreen::GestureNavigationScreen(
    base::WeakPtr<GestureNavigationScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(GestureNavigationScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {
  DCHECK(view_);
}

GestureNavigationScreen::~GestureNavigationScreen() = default;

void GestureNavigationScreen::GesturePageChange(const std::string& new_page) {
  page_times_[current_page_] += base::TimeTicks::Now() - start_time_;
  start_time_ = base::TimeTicks::Now();
  current_page_ = new_page;
}

bool GestureNavigationScreen::MaybeSkip(WizardContext& context) {
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  if (context.skip_post_login_screens_for_tests ||
      chrome_user_manager_util::IsPublicSessionOrEphemeralLogin() ||
      !features::IsHideShelfControlsInTabletModeEnabled() ||
      ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
          prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled) ||
      accessibility_manager->IsSpokenFeedbackEnabled() ||
      accessibility_manager->IsAutoclickEnabled() ||
      accessibility_manager->IsSwitchAccessEnabled()) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }

  // Skip the screen if the device is not in tablet mode, unless tablet mode
  // first user run is forced on the device.
  if (!TabletMode::Get()->InTabletMode() &&
      !switches::ShouldOobeUseTabletModeFirstRun()) {
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
  context()->is_gesture_navigation_screen_was_shown = true;
  if (view_) {
    view_->Show();
  }
}

void GestureNavigationScreen::HideImpl() {}

void GestureNavigationScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionExitPressed) {
    // Make sure the user does not see a notification about the new gestures
    // since they have already gone through this gesture education screen.
    ProfileManager::GetActiveUserProfile()->GetPrefs()->SetBoolean(
        prefs::kGestureEducationNotificationShown, true);

    RecordPageShownTimeMetrics();
    exit_callback_.Run(Result::NEXT);
    return;
  }
  if (action_id == kUserActionSkip) {
    exit_callback_.Run(Result::SKIP);
    return;
  }
  if (action_id == kUserActionGesturePageChange) {
    CHECK_EQ(args.size(), 2u);
    const std::string& new_page = args[1].GetString();
    GesturePageChange(new_page);
    return;
  }
  BaseScreen::OnUserAction(args);
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

}  // namespace ash
