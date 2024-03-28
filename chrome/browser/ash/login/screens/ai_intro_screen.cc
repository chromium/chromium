// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/ai_intro_screen.h"

#include "ai_intro_screen.h"
#include "ash/constants/ash_features.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/webui/ash/login/ai_intro_screen_handler.h"
#include "components/user_manager/user_manager.h"

namespace ash {
namespace {

constexpr char kUserActionNextButtonClicked[] = "next";

}  // namespace

// static
std::string AiIntroScreen::GetResultString(Result result) {
  switch (result) {
    case Result::kNext:
      return "Next";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
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

  return false;
}

AiIntroScreen::AiIntroScreen(base::WeakPtr<AiIntroScreenView> view,
                               const ScreenExitCallback& exit_callback)
    : BaseScreen(AiIntroScreenView::kScreenId, OobeScreenPriority::DEFAULT),
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

  view_->Show();
}

void AiIntroScreen::HideImpl() {}

void AiIntroScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionNextButtonClicked) {
    exit_callback_.Run(Result::kNext);
  } else {
    BaseScreen::OnUserAction(args);
  }
}

}  // namespace ash
