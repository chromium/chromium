// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/split_modifier_keyboard_info_screen.h"

#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/split_modifier_keyboard_info_screen_handler.h"
#include "ui/events/ash/keyboard_capability.h"

namespace ash {
namespace {

constexpr char kUserActionNextButtonClicked[] = "next";

}  // namespace

// static
std::string SplitModifierKeyboardInfoScreen::GetResultString(Result result) {
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
bool SplitModifierKeyboardInfoScreen::ShouldBeSkipped() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  bool is_managed_account = profile->GetProfilePolicyConnector()->IsManaged();
  bool is_child_account = user_manager->IsLoggedInAsChildUser();
  if (is_managed_account || is_child_account) {
    return true;
  }

  if (!Shell::Get()->keyboard_capability()->HasFunctionKeyOnAnyKeyboard() &&
      !ash::switches::ShouldSkipSplitModifierCheckForTesting()) {
    return true;
  }

  // Check flag in the end to not activate experiment clients before we check
  // for all conditions.
  if (!features::IsOobeSplitModifierKeyboardInfoEnabled()) {
    return true;
  }

  return false;
}

SplitModifierKeyboardInfoScreen::SplitModifierKeyboardInfoScreen(
    base::WeakPtr<SplitModifierKeyboardInfoScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(SplitModifierKeyboardInfoScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

SplitModifierKeyboardInfoScreen::~SplitModifierKeyboardInfoScreen() = default;

bool SplitModifierKeyboardInfoScreen::MaybeSkip(WizardContext& context) {
  if (context.skip_post_login_screens_for_tests) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }

  if (SplitModifierKeyboardInfoScreen::ShouldBeSkipped()) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }

  return false;
}

void SplitModifierKeyboardInfoScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  view_->Show();
}

void SplitModifierKeyboardInfoScreen::HideImpl() {}

void SplitModifierKeyboardInfoScreen::OnUserAction(
    const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionNextButtonClicked) {
    exit_callback_.Run(Result::kNext);
  } else {
    BaseScreen::OnUserAction(args);
  }
}

}  // namespace ash
