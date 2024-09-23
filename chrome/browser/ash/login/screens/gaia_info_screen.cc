// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/gaia_info_screen.h"

#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/ui/webui/ash/login/gaia_info_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/mojom/screens_common.mojom.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/quick_start/quick_start_metrics.h"

namespace ash {


// static
std::string GaiaInfoScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::kManual:
      return "Manual";
    case Result::kEnterQuickStart:
      return "EnterQuickStart";
    case Result::kQuickStartOngoing:
      return BaseScreen::kNotApplicable;
    case Result::kBack:
      return "Back";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

GaiaInfoScreen::GaiaInfoScreen(base::WeakPtr<GaiaInfoScreenView> view,
                               const ScreenExitCallback& exit_callback)
    : BaseScreen(GaiaInfoScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      OobeMojoBinder(this),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

GaiaInfoScreen::~GaiaInfoScreen() = default;

bool GaiaInfoScreen::MaybeSkip(WizardContext& context) {
  if (ash::InstallAttributes::Get()->IsEnterpriseManaged() ||
      context.is_add_person_flow || context.skip_to_login_for_tests) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }

  // Continue QuickStart flow if there is an ongoing setup. This is checked in
  // the GaiaScreen as well in case the GaiaInfoScreen is not shown to a Quick
  // Start user.
  if (context.quick_start_setup_ongoing) {
    exit_callback_.Run(Result::kQuickStartOngoing);
    return true;
  }

  return false;
}

void GaiaInfoScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  // Determine the QuickStart entrypoint button visibility
  WizardController::default_controller()
      ->quick_start_controller()
      ->DetermineEntryPointVisibility(
          base::BindRepeating(&GaiaInfoScreen::SetQuickStartButtonVisibility,
                              weak_ptr_factory_.GetWeakPtr()));

  view_->Show();
}

void GaiaInfoScreen::HideImpl() {}

void GaiaInfoScreen::OnBackClicked() {
  if (is_hidden()) {
    return;
  }
  exit_callback_.Run(Result::kBack);
}

void GaiaInfoScreen::OnNextClicked(UserCreationFlowType user_flow) {
  if (is_hidden()) {
    return;
  }
  if (user_flow == UserCreationFlowType::kManual) {
    exit_callback_.Run(Result::kManual);
  } else {
    CHECK(context()->quick_start_enabled);
    CHECK(!context()->quick_start_setup_ongoing);
    exit_callback_.Run(Result::kEnterQuickStart);
  }
}

void GaiaInfoScreen::SetQuickStartButtonVisibility(bool visible) {
  if (visible && GetRemote()->is_bound()) {
    (*GetRemote())->SetQuickStartVisible();

    if (!has_emitted_quick_start_visible) {
      has_emitted_quick_start_visible = true;
      quick_start::QuickStartMetrics::RecordEntryPointVisible(
          quick_start::QuickStartMetrics::EntryPoint::GAIA_INFO_SCREEN);
    }
  }
}

}  // namespace ash
