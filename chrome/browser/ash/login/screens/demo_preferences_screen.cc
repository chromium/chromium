// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/demo_preferences_screen.h"

#include "ash/constants/ash_features.h"
#include "base/check_op.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/screens/welcome_screen.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/ash/login/demo_preferences_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace {

constexpr char kUserActionContinue[] = "continue-setup";
constexpr char kUserActionClose[] = "close-setup";
constexpr char kUserActionSetDemoModeCountry[] = "set-demo-mode-country";

}  // namespace

// static
std::string DemoPreferencesScreen::GetResultString(Result result) {
  switch (result) {
    case Result::COMPLETED:
    case Result::COMPLETED_CONSOLIDATED_CONSENT:
      return "Completed";
    case Result::CANCELED:
      return "Canceled";
  }
}

DemoPreferencesScreen::DemoPreferencesScreen(
    base::WeakPtr<DemoPreferencesScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(DemoPreferencesScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

DemoPreferencesScreen::~DemoPreferencesScreen() = default;

void DemoPreferencesScreen::ShowImpl() {
  if (view_)
    view_->Show();
}

void DemoPreferencesScreen::HideImpl() {}

void DemoPreferencesScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionContinue) {
    CHECK_EQ(args.size(), 3u);
    std::string country(
        g_browser_process->local_state()->GetString(prefs::kDemoModeCountry));
    if (country == DemoSession::kCountryNotSelectedId) {
      return;
    }

    // Pass retailer_name and store_input to DemoSetupController to set as prefs
    // once user has proceeded through setup
    const std::string& retailer_name_input = args[1].GetString();
    const std::string& store_number_input = args[2].GetString();
    DemoSetupController* demo_setup_controller =
        WizardController::default_controller()->demo_setup_controller();
    demo_setup_controller->set_retailer_name(retailer_name_input);
    demo_setup_controller->set_store_number(store_number_input);

    exit_callback_.Run(features::IsOobeConsolidatedConsentEnabled()
                           ? Result::COMPLETED_CONSOLIDATED_CONSENT
                           : Result::COMPLETED);
  } else if (action_id == kUserActionClose) {
    exit_callback_.Run(Result::CANCELED);
  } else if (action_id == kUserActionSetDemoModeCountry) {
    CHECK_EQ(args.size(), 2u);
    g_browser_process->local_state()->SetString(prefs::kDemoModeCountry,
                                                args[1].GetString());
  } else {
    BaseScreen::OnUserAction(args);
  }
}

}  // namespace ash
