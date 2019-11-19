// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/demo_preferences_screen.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/screens/welcome_screen.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/demo_preferences_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/ime/chromeos/input_method_descriptor.h"

namespace chromeos {

namespace {

constexpr char kUserActionContinue[] = "continue-setup";
constexpr char kUserActionClose[] = "close-setup";

WelcomeScreen* GetWelcomeScreen() {
  const WizardController* wizard_controller =
      WizardController::default_controller();
  DCHECK(wizard_controller);
  return WelcomeScreen::Get(wizard_controller->screen_manager());
}

// Sets locale and input method. If |locale| or |input_method| is empty then
// they will not be changed.
void SetApplicationLocaleAndInputMethod(const std::string& locale,
                                        const std::string& input_method) {
  WelcomeScreen* welcome_screen = GetWelcomeScreen();
  DCHECK(welcome_screen);
  welcome_screen->SetApplicationLocaleAndInputMethod(locale, input_method);
}

}  // namespace

DemoPreferencesScreen::DemoPreferencesScreen(
    DemoPreferencesScreenView* view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(DemoPreferencesScreenView::kScreenId),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  view_->Bind(this);

  // TODO(agawronska): Add tests for locale and input changes.
  input_method::InputMethodManager* input_manager =
      input_method::InputMethodManager::Get();
  UpdateInputMethod(input_manager);
  input_manager_observer_.Add(input_manager);
}

DemoPreferencesScreen::~DemoPreferencesScreen() {
  input_method::InputMethodManager::Get()->RemoveObserver(this);

  if (view_)
    view_->Bind(nullptr);
}

void DemoPreferencesScreen::SetLocale(const std::string& locale) {
  SetApplicationLocaleAndInputMethod(locale, std::string());
}

void DemoPreferencesScreen::SetInputMethod(const std::string& input_method) {
  SetApplicationLocaleAndInputMethod(std::string(), input_method);
}

void DemoPreferencesScreen::SetDemoModeCountry(const std::string& country_id) {
  g_browser_process->local_state()->SetString(prefs::kDemoModeCountry,
                                              country_id);
}

void DemoPreferencesScreen::Show() {
  WelcomeScreen* welcome_screen = GetWelcomeScreen();
  if (welcome_screen) {
    initial_locale_ = welcome_screen->GetApplicationLocale();
    initial_input_method_ = welcome_screen->GetInputMethod();
  }

  if (view_)
    view_->Show();
}

void DemoPreferencesScreen::Hide() {
  initial_locale_.clear();
  initial_input_method_.clear();

  if (view_)
    view_->Hide();
}

void DemoPreferencesScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionContinue) {
    exit_callback_.Run(Result::COMPLETED);
  } else if (action_id == kUserActionClose) {
    // Restore initial locale and input method if the user pressed back button.
    SetApplicationLocaleAndInputMethod(initial_locale_, initial_input_method_);
    exit_callback_.Run(Result::CANCELED);
  } else {
    BaseScreen::OnUserAction(action_id);
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

}  // namespace chromeos
