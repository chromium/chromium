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

// static
std::string TunaScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::kNext:
      return "Next";
    case Result::kBack:
      return "Back";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
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
      OobeMojoBinder(this),
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

void TunaScreen::OnBackClicked() {
  if (is_hidden()) {
    return;
  }

  exit_callback_.Run(Result::kBack);
}

void TunaScreen::OnNextClicked() {
  if (is_hidden()) {
    return;
  }

  exit_callback_.Run(Result::kNext);
}

}  // namespace ash
