// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/personalized_recommend_apps_screen.h"

#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/personalized_recommend_apps_screen_handler.h"

namespace ash {
namespace {

constexpr const char kUserActionNext[] = "next";
constexpr const char kUserActionSkip[] = "skip";

}  // namespace

// static
std::string PersonalizedRecommendAppsScreen::GetResultString(Result result) {
  switch (result) {
    case Result::kNext:
      return "Next";
    case Result::kSkip:
      return "Skip";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
}

PersonalizedRecommendAppsScreen::PersonalizedRecommendAppsScreen(
    base::WeakPtr<PersonalizedRecommendAppsScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(PersonalizedRecommendAppsScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

PersonalizedRecommendAppsScreen::~PersonalizedRecommendAppsScreen() = default;

bool PersonalizedRecommendAppsScreen::MaybeSkip(WizardContext& context) {
  if (context.skip_post_login_screens_for_tests) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }

  return false;
}

void PersonalizedRecommendAppsScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  // TODO(b/339789465) : get the list of apps and categories
  // user selected and generate the data for the screen
  // map[category]: list<Apps>

  view_->Show();
}

void PersonalizedRecommendAppsScreen::HideImpl() {}

void PersonalizedRecommendAppsScreen::OnUserAction(
    const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();

  if (action_id == kUserActionSkip) {
    exit_callback_.Run(Result::kSkip);
    return;
  }

  if (action_id == kUserActionNext) {
    CHECK_EQ(args.size(), 2u);
    // TODO(b/339789465) : the install logic of the apps.
    exit_callback_.Run(Result::kNext);
    return;
  }

  BaseScreen::OnUserAction(args);
}

}  // namespace ash
