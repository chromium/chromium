// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/welcome_screen.h"

#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/ash/base/locale_util.h"
#include "chrome/browser/ash/login/configuration_keys.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/ui/input_events_blocker.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/system/timezone_resolver_manager.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/policy/enrollment_requisition_manager.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/l10n_util.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/components/chromebox_for_meetings/buildflags/buildflags.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {
namespace {

constexpr const char kRemoraRequisitionIdentifier[] = "remora";
constexpr char kUserActionContinueButtonClicked[] = "continue";
constexpr const char kUserActionEnableSpokenFeedback[] =
    "accessibility-spoken-feedback-enable";
constexpr const char kUserActionDisableSpokenFeedback[] =
    "accessibility-spoken-feedback-disable";
constexpr const char kUserActionEnableLargeCursor[] =
    "accessibility-large-cursor-enable";
constexpr const char kUserActionDisableLargeCursor[] =
    "accessibility-large-cursor-disable";
constexpr const char kUserActionEnableHighContrast[] =
    "accessibility-high-contrast-enable";
constexpr const char kUserActionDisableHighContrast[] =
    "accessibility-high-contrast-disable";
constexpr const char kUserActionEnableScreenMagnifier[] =
    "accessibility-screen-magnifier-enable";
constexpr const char kUserActionDisableScreenMagnifier[] =
    "accessibility-screen-magnifier-disable";
constexpr const char kUserActionEnableSelectToSpeak[] =
    "accessibility-select-to-speak-enable";
constexpr const char kUserActionDisableSelectToSpeak[] =
    "accessibility-select-to-speak-disable";
constexpr const char kUserActionEnableDockedMagnifier[] =
    "accessibility-docked-magnifier-enable";
constexpr const char kUserActionDisableDockedMagnifier[] =
    "accessibility-docked-magnifier-disable";
constexpr const char kUserActionEnableVirtualKeyboard[] =
    "accessibility-virtual-keyboard-enable";
constexpr const char kUserActionDisableVirtualKeyboard[] =
    "accessibility-virtual-keyboard-disable";
constexpr const char kUserActionSetupDemoMode[] = "setupDemoMode";
constexpr const char kUserActionSetupDemoModeGesture[] = "setupDemoModeGesture";
constexpr const char kUserActionEnableDebugging[] = "enableDebugging";
constexpr const char kUserActionActivateChromeVoxFromHint[] =
    "activateChromeVoxFromHint";
constexpr const char kUserActionDismissChromeVoxHint[] = "dismissChromeVoxHint";
constexpr const char kUserActionCancelChromeVoxHint[] = "cancelChromeVoxHint";
constexpr const char kUserActionActivateRemoraRequisition[] =
    "activateRemoraRequisition";
constexpr const char kUserActionEditDeviceRequisition[] =
    "editDeviceRequisition";

struct WelcomeScreenA11yUserAction {
  const char* name_;
  WelcomeScreen::A11yUserAction uma_name_;
};

const WelcomeScreenA11yUserAction actions[] = {
    {kUserActionEnableSpokenFeedback,
     WelcomeScreen::A11yUserAction::kEnableSpokenFeedback},
    {kUserActionDisableSpokenFeedback,
     WelcomeScreen::A11yUserAction::kDisableSpokenFeedback},
    {kUserActionEnableLargeCursor,
     WelcomeScreen::A11yUserAction::kEnableLargeCursor},
    {kUserActionDisableLargeCursor,
     WelcomeScreen::A11yUserAction::kDisableLargeCursor},
    {kUserActionEnableHighContrast,
     WelcomeScreen::A11yUserAction::kEnableHighContrast},
    {kUserActionDisableHighContrast,
     WelcomeScreen::A11yUserAction::kDisableHighContrast},
    {kUserActionEnableScreenMagnifier,
     WelcomeScreen::A11yUserAction::kEnableScreenMagnifier},
    {kUserActionDisableScreenMagnifier,
     WelcomeScreen::A11yUserAction::kDisableScreenMagnifier},
    {kUserActionEnableSelectToSpeak,
     WelcomeScreen::A11yUserAction::kEnableSelectToSpeak},
    {kUserActionDisableSelectToSpeak,
     WelcomeScreen::A11yUserAction::kDisableSelectToSpeak},
    {kUserActionEnableDockedMagnifier,
     WelcomeScreen::A11yUserAction::kEnableDockedMagnifier},
    {kUserActionDisableDockedMagnifier,
     WelcomeScreen::A11yUserAction::kDisableDockedMagnifier},
    {kUserActionEnableVirtualKeyboard,
     WelcomeScreen::A11yUserAction::kEnableVirtualKeyboard},
    {kUserActionDisableVirtualKeyboard,
     WelcomeScreen::A11yUserAction::kDisableVirtualKeyboard},
};

bool IsA11yUserAction(const std::string& action_id) {
  for (const auto& el : actions) {
    if (action_id == el.name_) {
      return true;
    }
  }
  return false;
}

void RecordA11yUserAction(const std::string& action_id) {
  for (const auto& el : actions) {
    if (action_id == el.name_) {
      base::UmaHistogramEnumeration("OOBE.WelcomeScreen.A11yUserActions",
                                    el.uma_name_);
      return;
    }
  }
  NOTREACHED() << "Unexpected action id: " << action_id;
}

// Returns true if is a Meet Device or the remora requisition bit has been set
// for testing. Note: Can be overridden with the command line switch
// --enable-requisition-edits.
bool IsRemoraRequisitionConfigurable() {
#if BUILDFLAG(PLATFORM_CFM)
  return true;
#else
  return policy::EnrollmentRequisitionManager::IsRemoraRequisition() ||
         chromeos::switches::IsDeviceRequisitionConfigurable();
#endif
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// WelcomeScreen, public:

// static
std::string WelcomeScreen::GetResultString(Result result) {
  switch (result) {
    case Result::NEXT:
      return "Next";
    case Result::SETUP_DEMO:
      return "SetupDemo";
    case Result::ENABLE_DEBUGGING:
      return "EnableDebugging";
  }
}

WelcomeScreen::WelcomeScreen(WelcomeView* view,
                             const ScreenExitCallback& exit_callback)
    : BaseScreen(WelcomeView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  if (view_)
    view_->Bind(this);

  input_method::InputMethodManager::Get()->AddObserver(this);
  UpdateLanguageList();
}

WelcomeScreen::~WelcomeScreen() {
  if (view_)
    view_->Unbind();

  input_method::InputMethodManager::Get()->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// WelcomeScreen, public API, setters and getters for input method and timezone.

void WelcomeScreen::OnViewDestroyed(WelcomeView* view) {
  if (view_ == view) {
    view_ = nullptr;
  }
}

void WelcomeScreen::UpdateLanguageList() {
  // Bail if there is already pending request.
  if (weak_factory_.HasWeakPtrs())
    return;

  ScheduleResolveLanguageList(
      std::unique_ptr<locale_util::LanguageSwitchResult>());
}

void WelcomeScreen::SetApplicationLocaleAndInputMethod(
    const std::string& locale,
    const std::string& input_method) {
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  if (app_locale == locale || locale.empty()) {
    // If the locale doesn't change, set input method directly.
    SetInputMethod(input_method);
    return;
  }

  // Cancel pending requests.
  weak_factory_.InvalidateWeakPtrs();

  // Block UI while resource bundle is being reloaded.
  // (InputEventsBlocker will live until callback is finished.)
  locale_util::SwitchLanguageCallback callback(base::BindOnce(
      &WelcomeScreen::OnLanguageChangedCallback, weak_factory_.GetWeakPtr(),
      base::Owned(new chromeos::InputEventsBlocker), input_method));
  locale_util::SwitchLanguage(locale, true /* enableLocaleKeyboardLayouts */,
                              true /* login_layouts_only */,
                              std::move(callback),
                              ProfileManager::GetActiveUserProfile());
}

std::string WelcomeScreen::GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

std::string WelcomeScreen::GetInputMethod() const {
  return input_method_;
}

void WelcomeScreen::SetApplicationLocale(const std::string& locale) {
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  if (app_locale == locale || locale.empty())
    return;

  // Cancel pending requests.
  weak_factory_.InvalidateWeakPtrs();

  // Block UI while resource bundle is being reloaded.
  // (InputEventsBlocker will live until callback is finished.)
  locale_util::SwitchLanguageCallback callback(base::BindOnce(
      &WelcomeScreen::OnLanguageChangedCallback, weak_factory_.GetWeakPtr(),
      base::Owned(new chromeos::InputEventsBlocker), std::string()));
  locale_util::SwitchLanguage(locale, true /* enableLocaleKeyboardLayouts */,
                              true /* login_layouts_only */,
                              std::move(callback),
                              ProfileManager::GetActiveUserProfile());
}

void WelcomeScreen::SetInputMethod(const std::string& input_method) {
  const std::vector<std::string>& input_methods =
      input_method::InputMethodManager::Get()
          ->GetActiveIMEState()
          ->GetActiveInputMethodIds();
  if (input_method.empty() || !base::Contains(input_methods, input_method)) {
    LOG(WARNING) << "The input method is empty or ineligible!";
    return;
  }

  if (input_method_ == input_method)
    return;

  input_method_ = input_method;
  input_method::InputMethodManager::Get()
      ->GetActiveIMEState()
      ->ChangeInputMethod(input_method_, false /* show_message */);
}

void WelcomeScreen::SetTimezone(const std::string& timezone_id) {
  if (timezone_id.empty())
    return;

  timezone_ = timezone_id;
  chromeos::system::SetSystemAndSigninScreenTimezone(timezone_id);
}

std::string WelcomeScreen::GetTimezone() const {
  return timezone_;
}

void WelcomeScreen::SetDeviceRequisition(const std::string& requisition) {
  if (requisition == kRemoraRequisitionIdentifier) {
    if (!IsRemoraRequisitionConfigurable())
      return;
  } else {
    if (!switches::IsDeviceRequisitionConfigurable())
      return;
  }

  std::string initial_requisition =
      policy::EnrollmentRequisitionManager::GetDeviceRequisition();
  policy::EnrollmentRequisitionManager::SetDeviceRequisition(requisition);

  if (policy::EnrollmentRequisitionManager::IsRemoraRequisition()) {
    // CfM devices default to static timezone.
    g_browser_process->local_state()->SetInteger(
        prefs::kResolveDeviceTimezoneByGeolocationMethod,
        static_cast<int>(chromeos::system::TimeZoneResolverManager::
                             TimeZoneResolveMethod::DISABLED));
  }

  // Exit Chrome to force the restart as soon as a new requisition is set.
  if (initial_requisition !=
      policy::EnrollmentRequisitionManager::GetDeviceRequisition()) {
    chrome::AttemptRestart();
  }
}

void WelcomeScreen::AddObserver(Observer* observer) {
  if (observer)
    observers_.AddObserver(observer);
}

void WelcomeScreen::RemoveObserver(Observer* observer) {
  if (observer)
    observers_.RemoveObserver(observer);
}

////////////////////////////////////////////////////////////////////////////////
// BaseScreen implementation:

void WelcomeScreen::ShowImpl() {
  // Here we should handle default locales, for which we do not have UI
  // resources. This would load fallback, but properly show "selected" locale
  // in the UI.
  if (selected_language_code_.empty()) {
    const StartupCustomizationDocument* startup_manifest =
        StartupCustomizationDocument::GetInstance();
    SetApplicationLocale(startup_manifest->initial_locale_default());
  }

  // Automatically continue if we are using hands-off enrollment.
  if (WizardController::UsingHandsOffEnrollment()) {
    OnUserAction(kUserActionContinueButtonClicked);
    return;
  }

  // TODO(crbug.com/1105387): Part of initial screen logic.
  PrefService* prefs = g_browser_process->local_state();
  if (prefs->GetBoolean(prefs::kDebuggingFeaturesRequested)) {
    OnEnableDebugging();
    return;
  }

  demo_mode_detector_ = std::make_unique<DemoModeDetector>(
      base::DefaultTickClock::GetInstance(), this);
  chromevox_hint_detector_ = std::make_unique<ChromeVoxHintDetector>(
      base::DefaultTickClock::GetInstance(), this);
  if (view_)
    view_->Show();
}

void WelcomeScreen::HideImpl() {
  if (view_)
    view_->Hide();
  demo_mode_detector_.reset();
  CancelChromeVoxHintIdleDetection();
}

void WelcomeScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionContinueButtonClicked) {
    OnContinueButtonPressed();
    return;
  }
  if (action_id == kUserActionSetupDemoMode) {
    OnSetupDemoMode();
    return;
  }
  if (action_id == kUserActionEnableDebugging) {
    OnEnableDebugging();
    return;
  }
  if (action_id == kUserActionSetupDemoModeGesture) {
    HandleAccelerator(ash::LoginAcceleratorAction::kStartDemoMode);
    return;
  }
  if (action_id == kUserActionActivateChromeVoxFromHint) {
    base::UmaHistogramBoolean("OOBE.WelcomeScreen.AcceptChromeVoxHint", true);
    AccessibilityManager::Get()->EnableSpokenFeedback(true);
    return;
  }
  if (action_id == kUserActionDismissChromeVoxHint) {
    base::UmaHistogramBoolean("OOBE.WelcomeScreen.AcceptChromeVoxHint", false);
    return;
  }
  if (action_id == kUserActionCancelChromeVoxHint) {
    CancelChromeVoxHintIdleDetection();
    return;
  }

  if (action_id == kUserActionActivateRemoraRequisition) {
    HandleAccelerator(ash::LoginAcceleratorAction::kDeviceRequisitionRemora);
    return;
  }

  if (action_id == kUserActionEditDeviceRequisition) {
    HandleAccelerator(ash::LoginAcceleratorAction::kEditDeviceRequisition);
    return;
  }

  if (IsA11yUserAction(action_id)) {
    RecordA11yUserAction(action_id);
    if (action_id == kUserActionEnableSpokenFeedback) {
      AccessibilityManager::Get()->EnableSpokenFeedback(true);
    } else if (action_id == kUserActionDisableSpokenFeedback) {
      AccessibilityManager::Get()->EnableSpokenFeedback(false);
    } else if (action_id == kUserActionEnableLargeCursor) {
      AccessibilityManager::Get()->EnableLargeCursor(true);
    } else if (action_id == kUserActionDisableLargeCursor) {
      AccessibilityManager::Get()->EnableLargeCursor(false);
    } else if (action_id == kUserActionEnableHighContrast) {
      AccessibilityManager::Get()->EnableHighContrast(true);
    } else if (action_id == kUserActionDisableHighContrast) {
      AccessibilityManager::Get()->EnableHighContrast(false);
    } else if (action_id == kUserActionEnableScreenMagnifier) {
      DCHECK(MagnificationManager::Get());
      MagnificationManager::Get()->SetMagnifierEnabled(true);
    } else if (action_id == kUserActionDisableScreenMagnifier) {
      DCHECK(MagnificationManager::Get());
      MagnificationManager::Get()->SetMagnifierEnabled(false);
    } else if (action_id == kUserActionEnableSelectToSpeak) {
      AccessibilityManager::Get()->SetSelectToSpeakEnabled(true);
    } else if (action_id == kUserActionDisableSelectToSpeak) {
      AccessibilityManager::Get()->SetSelectToSpeakEnabled(false);
    } else if (action_id == kUserActionEnableDockedMagnifier) {
      DCHECK(MagnificationManager::Get());
      MagnificationManager::Get()->SetDockedMagnifierEnabled(true);
    } else if (action_id == kUserActionDisableDockedMagnifier) {
      DCHECK(MagnificationManager::Get());
      MagnificationManager::Get()->SetDockedMagnifierEnabled(false);
    } else if (action_id == kUserActionEnableVirtualKeyboard) {
      AccessibilityManager::Get()->EnableVirtualKeyboard(true);
    } else if (action_id == kUserActionDisableVirtualKeyboard) {
      AccessibilityManager::Get()->EnableVirtualKeyboard(false);
    }
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

bool WelcomeScreen::HandleAccelerator(ash::LoginAcceleratorAction action) {
  if (action == ash::LoginAcceleratorAction::kStartDemoMode) {
    if (!DemoSetupController::IsDemoModeAllowed())
      return true;
    if (!view_)
      return true;
    const auto* key = context()->configuration.FindKeyOfType(
        configuration::kEnableDemoMode, base::Value::Type::BOOLEAN);
    const bool value = key && key->GetBool();
    if (value) {
      OnSetupDemoMode();
      return true;
    }

    view_->ShowDemoModeConfirmationDialog();
    return true;
  } else if (action == ash::LoginAcceleratorAction::kStartEnrollment) {
    context()->enrollment_triggered_early = true;
    return true;
  } else if (action == ash::LoginAcceleratorAction::kEnableDebugging) {
    OnEnableDebugging();
    return true;
  } else if (action == ash::LoginAcceleratorAction::kEditDeviceRequisition &&
             switches::IsDeviceRequisitionConfigurable()) {
    if (view_)
      view_->ShowEditRequisitionDialog(
          policy::EnrollmentRequisitionManager::GetDeviceRequisition());
    return true;
  } else if (action == ash::LoginAcceleratorAction::kDeviceRequisitionRemora &&
             IsRemoraRequisitionConfigurable()) {
    if (view_)
      view_->ShowRemoraRequisitionDialog();
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
// WelcomeScreen, InputMethodManager::Observer implementation:

void WelcomeScreen::InputMethodChanged(
    input_method::InputMethodManager* manager,
    Profile* /* proflie */,
    bool /* show_message */) {
  if (view_) {
    view_->SetInputMethodId(
        manager->GetActiveIMEState()->GetCurrentInputMethod().id());
  }
}

////////////////////////////////////////////////////////////////////////////////
// WelcomeScreen, private:

void WelcomeScreen::OnContinueButtonPressed() {
  demo_mode_detector_.reset();
  exit_callback_.Run(Result::NEXT);
}

void WelcomeScreen::OnSetupDemoMode() {
  demo_mode_detector_.reset();
  exit_callback_.Run(Result::SETUP_DEMO);
}

void WelcomeScreen::OnEnableDebugging() {
  demo_mode_detector_.reset();
  exit_callback_.Run(Result::ENABLE_DEBUGGING);
}

void WelcomeScreen::OnLanguageChangedCallback(
    const InputEventsBlocker* /* input_events_blocker */,
    const std::string& input_method,
    const locale_util::LanguageSwitchResult& result) {
  if (!selected_language_code_.empty()) {
    // We still do not have device owner, so owner settings are not applied.
    // But Guest session can be started before owner is created, so we need to
    // save locale settings directly here.
    g_browser_process->local_state()->SetString(
        language::prefs::kApplicationLocale, selected_language_code_);
  }
  ScheduleResolveLanguageList(
      std::make_unique<locale_util::LanguageSwitchResult>(result));

  AccessibilityManager::Get()->OnLocaleChanged();
  SetInputMethod(input_method);
  NotifyLocaleChange();
}

void WelcomeScreen::ScheduleResolveLanguageList(
    std::unique_ptr<locale_util::LanguageSwitchResult> language_switch_result) {
  // Cancel pending requests.
  weak_factory_.InvalidateWeakPtrs();

  ResolveUILanguageList(std::move(language_switch_result),
                        base::BindOnce(&WelcomeScreen::OnLanguageListResolved,
                                       weak_factory_.GetWeakPtr()));
}

void WelcomeScreen::OnLanguageListResolved(
    std::unique_ptr<base::ListValue> new_language_list,
    const std::string& new_language_list_locale,
    const std::string& new_selected_language) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  language_list_ = std::move(new_language_list);
  language_list_locale_ = new_language_list_locale;
  selected_language_code_ = new_selected_language;

  g_browser_process->local_state()->SetString(
      language::prefs::kApplicationLocale, selected_language_code_);
  if (view_)
    view_->ReloadLocalizedContent();
  for (auto& observer : observers_)
    observer.OnLanguageListReloaded();
}

void WelcomeScreen::NotifyLocaleChange() {
  ash::LocaleUpdateController::Get()->OnLocaleChanged();
}

void WelcomeScreen::CancelChromeVoxHintIdleDetection() {
  chromevox_hint_detector_.reset();
}

void WelcomeScreen::OnShouldGiveChromeVoxHint() {
  if (is_hidden())
    return;
  if (view_) {
    view_->GiveChromeVoxHint();
    chromevox_hint_detector_.reset();
  }
}

ChromeVoxHintDetector* WelcomeScreen::GetChromeVoxHintDetectorForTesting() {
  return chromevox_hint_detector_.get();
}

}  // namespace chromeos
