// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/demo_preferences_screen.h"

#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/screens/welcome_screen.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/chromeos/login/demo_preferences_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/ime/ash/input_method_descriptor.h"

namespace ash {
namespace {

constexpr char kUserActionContinue[] = "continue-setup";
constexpr char kUserActionClose[] = "close-setup";

}  // namespace

// static
std::string DemoPreferencesScreen::GetResultString(Result result) {
  switch (result) {
    case Result::COMPLETED:
      return "Completed";
    case Result::CANCELED:
      return "Canceled";
  }
}

DemoPreferencesScreen::DemoPreferencesScreen(
    DemoPreferencesScreenView* view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(DemoPreferencesScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  view_->Bind(this);

  // TODO(agawronska): Add tests for locale and input changes.
  input_method::InputMethodManager* input_manager =
      input_method::InputMethodManager::Get();
  UpdateInputMethod(input_manager);
  input_manager_observation_.Observe(input_manager);
}

DemoPreferencesScreen::~DemoPreferencesScreen() {
  input_method::InputMethodManager::Get()->RemoveObserver(this);

  if (view_)
    view_->Bind(nullptr);
}

void DemoPreferencesScreen::SetDemoModeCountry(const std::string& country_id) {
  g_browser_process->local_state()->SetString(prefs::kDemoModeCountry,
                                              country_id);
}

void DemoPreferencesScreen::ShowImpl() {
  if (view_)
    view_->Show();
}

void DemoPreferencesScreen::HideImpl() {
  if (view_)
    view_->Hide();
}

void DemoPreferencesScreen::OnUserActionDeprecated(
    const std::string& action_id) {
  if (action_id == kUserActionContinue) {
    std::string country(
        g_browser_process->local_state()->GetString(prefs::kDemoModeCountry));
    if (country == DemoSession::kCountryNotSelectedId) {
      return;
    }
    exit_callback_.Run(Result::COMPLETED);
  } else if (action_id == kUserActionClose) {
    exit_callback_.Run(Result::CANCELED);
  } else {
    BaseScreen::OnUserActionDeprecated(action_id);
  }
}

void DemoPreferencesScreen::OnViewDestroyed(DemoPreferencesScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void DemoPreferencesScreen::InputMethodChanged(
    input_method::InputMethodManager* manager,
    Profile* profile,
    bool show_message) {
  UpdateInputMethod(manager);
}

void DemoPreferencesScreen::UpdateInputMethod(
    input_method::InputMethodManager* input_manager) {
  if (view_) {
    const input_method::InputMethodDescriptor input_method =
        input_manager->GetActiveIMEState()->GetCurrentInputMethod();
    view_->SetInputMethodId(input_method.id());
  }
}

}  // namespace ash
