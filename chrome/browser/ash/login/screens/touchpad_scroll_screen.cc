// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/touchpad_scroll_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/schedule_enums.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/touchpad_scroll_screen_handler.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"

namespace ash {
namespace {

constexpr const char kUserActionNext[] = "next";
constexpr const char kUserActionUpdateScrollDirection[] = "update-scroll";

}  // namespace

// static
std::string TouchpadScrollScreen::GetResultString(Result result) {
  switch (result) {
    case Result::kNext:
      return "Next";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
}

TouchpadScrollScreen::TouchpadScrollScreen(
    base::WeakPtr<TouchpadScrollScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(TouchpadScrollScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

TouchpadScrollScreen::~TouchpadScrollScreen() = default;

bool TouchpadScrollScreen::ShouldBeSkipped(const WizardContext& context) const {
  if (context.skip_post_login_screens_for_tests) {
    return true;
  }

  if (features::IsOobeTouchpadScrollEnabled()) {
    auto* choobe_controller =
        WizardController::default_controller()->choobe_flow_controller();
    if (choobe_controller) {
      return choobe_controller->ShouldScreenBeSkipped(
          TouchpadScrollScreenView::kScreenId);
    }
  }

  return false;
}

bool TouchpadScrollScreen::MaybeSkip(WizardContext& context) {
  if (!ShouldBeSkipped(context)) {
    return false;
  }

  exit_callback_.Run(Result::kNotApplicable);
  return true;
}

void TouchpadScrollScreen::OnScrollUpdate(bool is_reverse_scroll) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  // The pref is true if touchpad reverse scroll is enabled.
  profile->GetPrefs()->SetBoolean(prefs::kNaturalScroll, is_reverse_scroll);
}

bool TouchpadScrollScreen::GetNaturalScrollPrefValue() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  bool is_reverse_scrolling =
      profile->GetPrefs()->GetBoolean(prefs::kNaturalScroll);
  return is_reverse_scrolling;
}

void TouchpadScrollScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  view_->SetReverseScrolling(GetNaturalScrollPrefValue());
  view_->Show();
}

void TouchpadScrollScreen::HideImpl() {}

void TouchpadScrollScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();

  if (action_id == kUserActionNext) {
    exit_callback_.Run(Result::kNext);
    return;
  }

  if (action_id == kUserActionUpdateScrollDirection) {
    CHECK_EQ(args.size(), 2u);
    OnScrollUpdate(args[1].GetBool());
    return;
  }

  BaseScreen::OnUserAction(args);
}

ScreenSummary TouchpadScrollScreen::GetScreenSummary() {
  ScreenSummary summary;
  summary.screen_id = TouchpadScrollScreenView::kScreenId;
  summary.icon_id = "oobe-32:scroll-direction";
  summary.title_id = "choobeTouchpadScrollTitle";
  summary.is_revisitable = false;
  summary.is_synced = !ProfileManager::GetActiveUserProfile()
                           ->GetPrefs()
                           ->FindPreference(prefs::kNaturalScroll)
                           ->IsDefaultValue();
  return summary;
}

}  // namespace ash
