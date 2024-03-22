// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/ai_intro_screen.h"

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/webui/ash/login/ai_intro_screen_handler.h"

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

AiIntroScreen::AiIntroScreen(base::WeakPtr<AiIntroScreenView> view,
                               const ScreenExitCallback& exit_callback)
    : BaseScreen(AiIntroScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

AiIntroScreen::~AiIntroScreen() = default;

bool AiIntroScreen::MaybeSkip(WizardContext& context) {
  return true;
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
