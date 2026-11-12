// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/gemini_intro_screen.h"

#include "ai_intro_screen.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_login_pref_names.h"
#include "ash/constants/ash_pref_names.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/ai_intro_screen.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/webui/ash/login/gemini_intro_screen_handler.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"

namespace ash {

// static
std::string GeminiIntroScreen::GetResultString(Result result) {
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
bool GeminiIntroScreen::ShouldBeSkipped(
    const user_manager::User* onboarding_user) {
  if (onboarding_user && onboarding_user->IsChild()) {
    return true;
  }

  // May run before anybody has signed in, and every check below is about
  // the signed-in user.
  const PrefService* const pref_service =
      onboarding_user ? onboarding_user->GetProfilePrefs() : nullptr;
  if (!pref_service) {
    return false;
  }

  // Skip the screen if the perk was already shown to the user in perks
  // discovery.
  if (features::IsOobePerksDiscoveryEnabled() &&
      pref_service->GetBoolean(prefs::kOobePerksDiscoveryGamgeeShown)) {
    return true;
  }

  // Skip the screen if `kShowGeminiIntroScreenEnabled` preference is set by
  // managed user default or admin to false.
  const PrefService::Preference* pref =
      pref_service->FindPreference(prefs::kShowGeminiIntroScreenEnabled);
  if (pref->IsManaged() && !pref->GetValue()->GetBool()) {
    return true;
  }

  return false;
}

GeminiIntroScreen::GeminiIntroScreen(base::WeakPtr<GeminiIntroScreenView> view,
                       const ScreenExitCallback& exit_callback)
    : BaseScreen(GeminiIntroScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      OobeMojoBinder(this),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

GeminiIntroScreen::~GeminiIntroScreen() = default;

bool GeminiIntroScreen::MaybeSkip(WizardContext& context) {
  if (context.skip_post_login_screens_for_tests) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }

  if (GeminiIntroScreen::ShouldBeSkipped(context.onboarding_user)) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }

  return false;
}

void GeminiIntroScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  base::DictValue params = base::DictValue().Set(
      "backButtonVisible",
      !AiIntroScreen::ShouldBeSkipped(context()->onboarding_user));
  view_->Show(std::move(params));
}

void GeminiIntroScreen::HideImpl() {}

void GeminiIntroScreen::OnBackClicked() {
  if (is_hidden()) {
    return;
  }

  exit_callback_.Run(Result::kBack);
}

void GeminiIntroScreen::OnNextClicked() {
  if (is_hidden()) {
    return;
  }

  exit_callback_.Run(Result::kNext);
}

}  // namespace ash
