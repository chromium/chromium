// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/tuna_screen.h"

#include "ai_intro_screen.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/ai_intro_screen.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/tuna_screen_handler.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace ash {
namespace {

constexpr char kUserActionBackButtonClicked[] = "back";
constexpr char kUserActionNextButtonClicked[] = "next";

}  // namespace

// static
std::string TunaScreen::GetResultString(Result result) {
  switch (result) {
    case Result::kNext:
      return "Next";
    case Result::kBack:
      return "Back";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
}

// static
bool TunaScreen::ShouldBeSkipped() {
  if (!features::IsOobeTunaEnabled()) {
    return true;
  }

  auto* user_manager = user_manager::UserManager::Get();
  if (user_manager->IsLoggedInAsChildUser()) {
    return true;
  }

  // Skip the screen if `kShowTunaScreenEnabled` preference is set by
  // managed user default or admin to false.
  const PrefService::Preference* pref =
      ProfileManager::GetActiveUserProfile()->GetPrefs()->FindPreference(
          prefs::kShowTunaScreenEnabled);
  if (pref->IsManaged() && !pref->GetValue()->GetBool()) {
    return true;
  }

  return false;
}

TunaScreen::TunaScreen(base::WeakPtr<TunaScreenView> view,
                               const ScreenExitCallback& exit_callback)
    : BaseScreen(TunaScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

TunaScreen::~TunaScreen() = default;

bool TunaScreen::MaybeSkip(WizardContext& context) {
  if (context.skip_post_login_screens_for_tests) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }

  if (TunaScreen::ShouldBeSkipped()) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }

  return false;
}

void TunaScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  base::Value::Dict params = base::Value::Dict()
    .Set("backButtonVisible", !AiIntroScreen::ShouldBeSkipped());
  view_->Show(std::move(params));
}

void TunaScreen::HideImpl() {}

void TunaScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionNextButtonClicked) {
    exit_callback_.Run(Result::kNext);
  } else if (action_id == kUserActionBackButtonClicked) {
    exit_callback_.Run(Result::kBack);
  } else {
    BaseScreen::OnUserAction(args);
  }
}

}  // namespace ash
