// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/gaia_info_screen.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/ui/webui/ash/login/gaia_info_screen_handler.h"

namespace ash {

namespace {

constexpr char kUserActionBack[] = "back";
constexpr char kUserActionNext[] = "next";

}  // namespace

// static
std::string GaiaInfoScreen::GetResultString(Result result) {
  switch (result) {
    case Result::kNext:
      return "Next";
    case Result::kBack:
      return "Back";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
}

GaiaInfoScreen::GaiaInfoScreen(base::WeakPtr<GaiaInfoScreenView> view,
                               const ScreenExitCallback& exit_callback)
    : BaseScreen(GaiaInfoScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

GaiaInfoScreen::~GaiaInfoScreen() = default;

bool GaiaInfoScreen::MaybeSkip(WizardContext& context) {
  if (g_browser_process->platform_part()
          ->browser_policy_connector_ash()
          ->IsDeviceEnterpriseManaged() ||
      context.is_add_person_flow || context.skip_to_login_for_tests) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }
  return false;
}

void GaiaInfoScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  view_->Show();
}

void GaiaInfoScreen::HideImpl() {}

void GaiaInfoScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionBack) {
    exit_callback_.Run(Result::kBack);
  } else if (action_id == kUserActionNext) {
    exit_callback_.Run(Result::kNext);
  } else {
    BaseScreen::OnUserAction(args);
  }
}

}  // namespace ash
