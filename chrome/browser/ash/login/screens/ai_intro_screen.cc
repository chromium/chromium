// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/ai_intro_screen.h"

#include "ai_intro_screen.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/ai_intro_screen_handler.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace ash {

// static
std::string AiIntroScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::kNext:
      return "Next";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

// static
bool AiIntroScreen::ShouldBeSkipped() {
  if (!features::IsOobeAiIntroEnabled()) {
    return true;
  }

  auto* user_manager = user_manager::UserManager::Get();
  if (user_manager->IsLoggedInAsChildUser()) {
    return true;
  }

  // Skip the screen if `kShowAiIntroScreenEnabled` preference is set by
  // managed user default or admin to false.
  const PrefService::Preference* pref =
      ProfileManager::GetActiveUserProfile()->GetPrefs()->FindPreference(
          prefs::kShowAiIntroScreenEnabled);
  if (pref->IsManaged() && !pref->GetValue()->GetBool()) {
    return true;
  }

  return false;
}

AiIntroScreen::AiIntroScreen(base::WeakPtr<AiIntroScreenView> view,
                             const ScreenExitCallback& exit_callback)
    : BaseScreen(AiIntroScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      OobeMojoBinder(this),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

AiIntroScreen::~AiIntroScreen() = default;

bool AiIntroScreen::MaybeSkip(WizardContext& context) {
  if (context.skip_post_login_screens_for_tests) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }

  if (AiIntroScreen::ShouldBeSkipped()) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }

  return false;
}

void AiIntroScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  // AccessibilityManager::Get() can be nullptr in unittests.
  if (AccessibilityManager::Get()) {
    AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
    accessibility_subscription_ = accessibility_manager->RegisterCallback(
        base::BindRepeating(&AiIntroScreen::OnAccessibilityStatusChanged,
                            weak_ptr_factory_.GetWeakPtr()));
    // Remote/frontend is missing during some tests.
    if (GetRemote()->is_bound()) {
      (*GetRemote())
          ->SetAutoTransition(
              !AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
    }
  }

  view_->Show();
}

void AiIntroScreen::HideImpl() {
  accessibility_subscription_ = {};
  // Remote/frontend is missing during some tests.
  if (GetRemote()->is_bound()) {
    (*GetRemote())->SetAutoTransition(false);
  }
}

void AiIntroScreen::OnAccessibilityStatusChanged(
    const AccessibilityStatusEventDetails& details) {
  if (details.notification_type ==
      AccessibilityNotificationType::kManagerShutdown) {
    accessibility_subscription_ = {};
    return;
  }
  // Remote/frontend is missing during some tests.
  // AccessibilityManager::Get() can be nullptr in unittests.
  if (GetRemote()->is_bound() && AccessibilityManager::Get()) {
    (*GetRemote())
        ->SetAutoTransition(
            !AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  }
}

void AiIntroScreen::OnNextClicked() {
  if (is_hidden()) {
    return;
  }

  exit_callback_.Run(Result::kNext);
}

}  // namespace ash
