// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/welcome_screen.h"

#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/ash/base/locale_util.h"
#include "chrome/browser/ash/customization/customization_document.h"
#include "chrome/browser/ash/login/configuration_keys.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/ash/system/timezone_resolver_manager.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/input_events_blocker.h"
#include "chrome/browser/ui/ash/login/login_screen_client_impl.h"
#include "chrome/browser/ui/webui/ash/login/l10n_util.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/quick_start/quick_start_metrics.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace ash {

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
constexpr const char kUserActionQuickStartClicked[] = "quickStartClicked";
constexpr const char kWelcomeScreenLocaleChangeMetric[] =
    "OOBE.WelcomeScreen.UserChangedLocale";
constexpr const char kSetLocaleId[] = "setLocaleId";
constexpr const char kSetInputMethodId[] = "setInputMethodId";
constexpr const char kSetTimezoneId[] = "setTimezoneId";
constexpr const char kSetDeviceRequisition[] = "setDeviceRequisition";

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
  NOTREACHED_IN_MIGRATION() << "Unexpected action id: " << action_id;
}

// Returns true if is a Meet Device or the remora requisition bit has been set
// for testing. Note: Can be overridden with the command line switch
// --enable-requisition-edits.
bool IsRemoraRequisitionConfigurable() {
  return policy::EnrollmentRequisitionManager::IsMeetDevice() ||
         switches::IsDeviceRequisitionConfigurable();
}

std::string GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// WelcomeScreen, public:

// static
std::string WelcomeScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::kNext:
      return "Next";
    case Result::kNextOSInstall:
      return "StartOsInstall";
    case Result::kSetupDemo:
      return "SetupDemo";
    case Result::kEnableDebugging:
      return "EnableDebugging";
    case Result::kQuickStart:
      return "QuickStart";
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

WelcomeScreen::WelcomeScreen(base::WeakPtr<WelcomeView> view,
                             const ScreenExitCallback& exit_callback)
    : BaseScreen(WelcomeView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {
  input_method::InputMethodManager::Get()->AddObserver(this);

  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  if (accessibility_manager) {
    accessibility_subscription_ = accessibility_manager->RegisterCallback(
        base::BindRepeating(&WelcomeScreen::OnAccessibilityStatusChanged,
                            base::Unretained(this)));
    UpdateA11yState();
  } else {
    CHECK_IS_TEST();
  }
}

WelcomeScreen::~WelcomeScreen() {
  input_method::InputMethodManager::Get()->RemoveObserver(this);
  CancelChromeVoxHintIdleDetection();
}

////////////////////////////////////////////////////////////////////////////////
// WelcomeScreen, public API, setters and getters for input method and timezone.

void WelcomeScreen::UpdateLanguageList() {
  // Bail if there is already pending request.
  if (language_weak_ptr_factory_.HasWeakPtrs())
    return;

  ScheduleResolveLanguageList(
      std::unique_ptr<locale_util::LanguageSwitchResult>());
}

void WelcomeScreen::SetApplicationLocaleAndInputMethod(
    const std::string& locale,
    const std::string& input_method) {
  const std::string& app_locale = GetApplicationLocale();
  if (app_locale == locale || locale.empty()) {
    // If the locale doesn't change, set input method directly.
    SetInputMethod(input_method);
    return;
  }

  // Cancel pending requests.
  language_weak_ptr_factory_.InvalidateWeakPtrs();

  // Block UI while resource bundle is being reloaded.
  // (InputEventsBlocker will live until callback is finished.)
  locale_util::SwitchLanguageCallback callback(
      base::BindOnce(&WelcomeScreen::OnLanguageChangedCallback,
                     language_weak_ptr_factory_.GetWeakPtr(),
                     base::Owned(new InputEventsBlocker), input_method));
  locale_util::SwitchLanguage(locale, true /* enableLocaleKeyboardLayouts */,
                              true /* login_layouts_only */,
                              std::move(callback),
                              ProfileManager::GetActiveUserProfile());
}

std::string WelcomeScreen::GetInputMethod() const {
  return input_method_;
}

void WelcomeScreen::SetApplicationLocale(const std::string& locale,
                                         const bool is_from_ui) {
  const std::string& app_locale = GetApplicationLocale();
  if (app_locale == locale || locale.empty()) {
    if (selected_language_code_.empty())
      UpdateLanguageList();
    return;
  }

  // Cancel pending requests.
  language_weak_ptr_factory_.InvalidateWeakPtrs();

  // Block UI while resource bundle is being reloaded.
  // (InputEventsBlocker will live until callback is finished.)
  locale_util::SwitchLanguageCallback callback(
      base::BindOnce(&WelcomeScreen::OnLanguageChangedCallback,
                     language_weak_ptr_factory_.GetWeakPtr(),
                     base::Owned(new InputEventsBlocker), std::string()));
  locale_util::SwitchLanguage(locale, true /* enableLocaleKeyboardLayouts */,
                              true /* login_layouts_only */,
                              std::move(callback),
                              ProfileManager::GetActiveUserProfile());
  if (is_from_ui) {
    // Write into the local state to save data about locale changes in case of
    // reboot of device after forced update.
    PrefService* local_state = g_browser_process->local_state();
    local_state->SetBoolean(prefs::kOobeLocaleChangedOnWelcomeScreen, true);
  }
}

void WelcomeScreen::SetInputMethod(const std::string& input_method) {
  const std::vector<std::string>& input_methods =
      input_method::InputMethodManager::Get()
          ->GetActiveIMEState()
          ->GetEnabledInputMethodIds();
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
  system::SetSystemAndSigninScreenTimezone(timezone_id);
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
        ::prefs::kResolveDeviceTimezoneByGeolocationMethod,
        static_cast<int>(
            system::TimeZoneResolverManager::TimeZoneResolveMethod::DISABLED));
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
    std::string stored_locale = g_browser_process->local_state()->GetString(
        language::prefs::kApplicationLocale);

    if (!stored_locale.empty()) {
      SetApplicationLocale(stored_locale,
                           /*is_from_ui=*/false);
    } else {
      const StartupCustomizationDocument* startup_manifest =
          StartupCustomizationDocument::GetInstance();
      SetApplicationLocale(startup_manifest->initial_locale_default(),
                           /*is_from_ui=*/false);
    }
  }

  // TODO(crbug.com/1105387): Part of initial screen logic.
  PrefService* prefs = g_browser_process->local_state();
  if (prefs->GetBoolean(::prefs::kDebuggingFeaturesRequested)) {
    OnEnableDebugging();
    return;
  }

  chromevox_hint_detector_ = std::make_unique<ChromeVoxHintDetector>(
      base::DefaultTickClock::GetInstance(), this);
  if (view_)
    view_->Show();

  // Determine the QuickStart button visibility
  WizardController::default_controller()
      ->quick_start_controller()
      ->DetermineEntryPointVisibility(
          base::BindRepeating(&WelcomeScreen::SetQuickStartButtonVisibility,
                              weak_ptr_factory_.GetWeakPtr()));

  if (LoginScreenClientImpl::HasInstance()) {
    LoginScreenClientImpl::Get()->AddSystemTrayObserver(this);
  }
}

void WelcomeScreen::HideImpl() {
  CancelChromeVoxHintIdleDetection();
}

void WelcomeScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionQuickStartClicked) {
    OnQuickStartClicked();
    return;
  }
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
    HandleAccelerator(LoginAcceleratorAction::kStartDemoMode);
    return;
  }
  if (action_id == kUserActionActivateChromeVoxFromHint) {
    base::UmaHistogramBoolean("OOBE.WelcomeScreen.AcceptChromeVoxHint", true);
    AccessibilityManager::Get()->EnableSpokenFeedbackWithTutorial();
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
    HandleAccelerator(LoginAcceleratorAction::kDeviceRequisitionRemora);
    return;
  }

  if (action_id == kUserActionEditDeviceRequisition) {
    HandleAccelerator(LoginAcceleratorAction::kEditDeviceRequisition);
    return;
  }

  if (action_id == kSetLocaleId) {
    CHECK_EQ(args.size(), 2u);
    SetApplicationLocale(args[1].GetString(), /*is_from_ui=*/true);
    return;
  }

  if (action_id == kSetInputMethodId) {
    CHECK_EQ(args.size(), 2u);
    SetInputMethod(args[1].GetString());
    return;
  }

  if (action_id == kSetTimezoneId) {
    CHECK_EQ(args.size(), 2u);
    SetTimezone(args[1].GetString());
    return;
  }

  if (action_id == kSetDeviceRequisition) {
    CHECK_EQ(args.size(), 2u);
    SetDeviceRequisition(args[1].GetString());
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
    BaseScreen::OnUserAction(args);
  }
}

bool WelcomeScreen::HandleAccelerator(LoginAcceleratorAction action) {
  if (action == LoginAcceleratorAction::kStartDemoMode) {
    if (!DemoSetupController::IsDemoModeAllowed())
      return true;
    if (!view_)
      return true;
    const auto key =
        context()
            ->configuration.FindBool(configuration::kEnableDemoMode)
            .value_or(false);
    if (key) {
      OnSetupDemoMode();
      return true;
    }

    view_->ShowDemoModeConfirmationDialog();
    return true;
  } else if (action == LoginAcceleratorAction::kStartEnrollment) {
    context()->enrollment_triggered_early = true;
    return true;
  } else if (action == LoginAcceleratorAction::kEnableDebugging) {
    OnEnableDebugging();
    return true;
  } else if (action == LoginAcceleratorAction::kEditDeviceRequisition &&
             switches::IsDeviceRequisitionConfigurable()) {
    if (view_)
      view_->ShowEditRequisitionDialog(
          policy::EnrollmentRequisitionManager::GetDeviceRequisition());
    return true;
  } else if (action == LoginAcceleratorAction::kDeviceRequisitionRemora &&
             IsRemoraRequisitionConfigurable()) {
    if (view_)
      view_->ShowRemoraRequisitionDialog();
    return true;
  } else if (action == LoginAcceleratorAction::kEnableQuickStart) {
    // Quick Start can be enabled either by feature flag or by keyboard
    // shortcut. The shortcut method enables a simpler workflow for testers,
    // while the feature flag will enable us to perform a first run field trial.
    WizardController::default_controller()
        ->quick_start_controller()
        ->ForceEnableQuickStart();

    // Update the entry point button visibility.
    WizardController::default_controller()
        ->quick_start_controller()
        ->DetermineEntryPointVisibility(
            base::BindRepeating(&WelcomeScreen::SetQuickStartButtonVisibility,
                                weak_ptr_factory_.GetWeakPtr()));
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

void WelcomeScreen::SetQuickStartButtonVisibility(bool visible) {
  if (!view_) {
    return;
  }

  if (visible) {
    view_->SetQuickStartEnabled();
    base::UmaHistogramBoolean(
        "QuickStart.WelcomeScreen.QuickStartButtonVisible", visible);
    if (!has_emitted_quick_start_visible) {
      has_emitted_quick_start_visible = true;
      quick_start::QuickStartMetrics::RecordEntryPointVisible(
          quick_start::QuickStartMetrics::EntryPoint::WELCOME_SCREEN);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// WelcomeScreen, private:

void WelcomeScreen::OnContinueButtonPressed() {
  if (switches::IsOsInstallAllowed())
    Exit(Result::kNextOSInstall);
  else
    Exit(Result::kNext);
}

void WelcomeScreen::OnSetupDemoMode() {
  Exit(Result::kSetupDemo);
}

void WelcomeScreen::OnEnableDebugging() {
  Exit(Result::kEnableDebugging);
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
  language_weak_ptr_factory_.InvalidateWeakPtrs();

  ResolveUILanguageList(
      std::move(language_switch_result),
      input_method::InputMethodManager::Get(),
      base::BindOnce(&WelcomeScreen::OnLanguageListResolved,
                     language_weak_ptr_factory_.GetWeakPtr()));
}

void WelcomeScreen::OnLanguageListResolved(
    base::Value::List new_language_list,
    const std::string& new_language_list_locale,
    const std::string& new_selected_language) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (new_language_list_locale != GetApplicationLocale()) {
    UpdateLanguageList();
    return;
  }

  selected_language_code_ = new_selected_language;

  g_browser_process->local_state()->SetString(
      language::prefs::kApplicationLocale, selected_language_code_);
  if (view_)
    view_->SetLanguageList(std::move(new_language_list));
  for (auto& observer : observers_)
    observer.OnLanguageListReloaded();
}

void WelcomeScreen::NotifyLocaleChange() {
  LocaleUpdateController::Get()->OnLocaleChanged();
}

void WelcomeScreen::CancelChromeVoxHintIdleDetection() {
  chromevox_hint_detector_.reset();
  if (LoginScreenClientImpl::HasInstance()) {
    LoginScreenClientImpl::Get()->RemoveSystemTrayObserver(this);
  }
}

void WelcomeScreen::OnShouldGiveChromeVoxHint() {
  if (is_hidden())
    return;
  if (view_) {
    view_->GiveChromeVoxHint();
    chromevox_hint_detector_.reset();
  }
}

void WelcomeScreen::OnFocusLeavingSystemTray(bool reverse) {}

void WelcomeScreen::OnSystemTrayBubbleShown() {
  CancelChromeVoxHintIdleDetection();
}

ChromeVoxHintDetector* WelcomeScreen::GetChromeVoxHintDetectorForTesting() {
  return chromevox_hint_detector_.get();
}

void WelcomeScreen::OnAccessibilityStatusChanged(
    const AccessibilityStatusEventDetails& details) {
  if (details.notification_type ==
      AccessibilityNotificationType::kManagerShutdown) {
    accessibility_subscription_ = {};
  } else {
    UpdateA11yState();
  }
}

void WelcomeScreen::UpdateA11yState() {
  DCHECK(MagnificationManager::Get());
  DCHECK(AccessibilityManager::Get());
  const WelcomeView::A11yState a11y_state{
      .high_contrast = AccessibilityManager::Get()->IsHighContrastEnabled(),
      .large_cursor = AccessibilityManager::Get()->IsLargeCursorEnabled(),
      .spoken_feedback = AccessibilityManager::Get()->IsSpokenFeedbackEnabled(),
      .select_to_speak = AccessibilityManager::Get()->IsSelectToSpeakEnabled(),
      .screen_magnifier = MagnificationManager::Get()->IsMagnifierEnabled(),
      .docked_magnifier =
          MagnificationManager::Get()->IsDockedMagnifierEnabled(),
      .virtual_keyboard =
          AccessibilityManager::Get()->IsVirtualKeyboardEnabled()};
  if (a11y_state.spoken_feedback)
    CancelChromeVoxHintIdleDetection();
  if (view_) {
    view_->UpdateA11yState(a11y_state);
  }
}

void WelcomeScreen::OnQuickStartClicked() {
  CHECK(context()->quick_start_enabled);
  CHECK(!context()->quick_start_setup_ongoing);
  Exit(Result::kQuickStart);
}

void WelcomeScreen::Exit(Result result) const {
  PrefService* local_state = g_browser_process->local_state();
  base::UmaHistogramBoolean(
      kWelcomeScreenLocaleChangeMetric,
      local_state->GetBoolean(prefs::kOobeLocaleChangedOnWelcomeScreen));
  exit_callback_.Run(result);
}

}  // namespace ash
