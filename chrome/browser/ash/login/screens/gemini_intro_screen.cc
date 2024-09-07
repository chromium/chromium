// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/gemini_intro_screen.h"

#include "ai_intro_screen.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/ai_intro_screen.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/gemini_intro_screen_handler.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

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
bool GeminiIntroScreen::ShouldBeSkipped() {
  if (!features::IsOobeGeminiIntroEnabled()) {
    return true;
  }

  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();

  // Skip the screen if the perk was already shown to the user in perks
  // discovery.
  if (features::IsOobePerksDiscoveryEnabled() &&
      prefs->GetBoolean(prefs::kOobePerksDiscoveryGamgeeShown)) {
    return true;
  }

  auto* user_manager = user_manager::UserManager::Get();
  if (user_manager->IsLoggedInAsChildUser()) {
    return true;
  }

  // Skip the screen if `kShowGeminiIntroScreenEnabled` preference is set by
  // managed user default or admin to false.
  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kShowGeminiIntroScreenEnabled);
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

  if (GeminiIntroScreen::ShouldBeSkipped()) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }

  return false;
}

void GeminiIntroScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  base::Value::Dict params = base::Value::Dict()
    .Set("backButtonVisible", !AiIntroScreen::ShouldBeSkipped());
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
