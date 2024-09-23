// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_WELCOME_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_WELCOME_SCREEN_H_

#include <memory>
#include <string>

#include "ash/public/cpp/locale_update_controller.h"
#include "ash/system/tray/system_tray_observer.h"
#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/chromevox_hint/chromevox_hint_detector.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "ui/base/ime/ash/input_method_manager.h"

namespace ash {

class InputEventsBlocker;
class WelcomeView;
struct AccessibilityStatusEventDetails;

namespace locale_util {
struct LanguageSwitchResult;
}

class WelcomeScreen : public BaseScreen,
                      public input_method::InputMethodManager::Observer,
                      public ChromeVoxHintDetector::Observer,
                      public SystemTrayObserver {
 public:
  using TView = WelcomeView;

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/enums.xml, and should always reflect it (do not
  // change one without changing the other).  Entries should be never modified
  // or deleted.  Only additions possible.
  enum class A11yUserAction {
    kEnableSpokenFeedback = 0,
    kDisableSpokenFeedback = 1,
    kEnableLargeCursor = 2,
    kDisableLargeCursor = 3,
    kEnableHighContrast = 4,
    kDisableHighContrast = 5,
    kEnableScreenMagnifier = 6,
    kDisableScreenMagnifier = 7,
    kEnableSelectToSpeak = 8,
    kDisableSelectToSpeak = 9,
    kEnableDockedMagnifier = 10,
    kDisableDockedMagnifier = 11,
    kEnableVirtualKeyboard = 12,
    kDisableVirtualKeyboard = 13,
    kMaxValue = kDisableVirtualKeyboard
  };

  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when language list is reloaded.
    virtual void OnLanguageListReloaded() = 0;
  };

  enum class Result {
    kNext,
    kNextOSInstall,
    kSetupDemo,
    kEnableDebugging,
    kQuickStart
  };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  WelcomeScreen(base::WeakPtr<WelcomeView> view,
                const ScreenExitCallback& exit_callback);

  WelcomeScreen(const WelcomeScreen&) = delete;
  WelcomeScreen& operator=(const WelcomeScreen&) = delete;

  ~WelcomeScreen() override;

  static std::string GetResultString(Result result);

  bool language_list_updated_for_testing() const {
    return !selected_language_code_.empty();
  }

  void UpdateLanguageList();

  // Set locale and input method. If `locale` is empty or doesn't change, set
  // the `input_method` directly. If `input_method` is empty or ineligible, we
  // don't change the current `input_method`.
  void SetApplicationLocaleAndInputMethod(const std::string& locale,
                                          const std::string& input_method);
  std::string GetInputMethod() const;

  void SetApplicationLocale(const std::string& locale, const bool is_from_ui);
  void SetInputMethod(const std::string& input_method);
  void SetTimezone(const std::string& timezone_id);
  std::string GetTimezone() const;

  void SetDeviceRequisition(const std::string& requisition);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  const base::Value::Dict& GetConfigurationForTesting() const {
    return context()->configuration;
  }

  // ChromeVox hint.
  ChromeVoxHintDetector* GetChromeVoxHintDetectorForTesting();

 protected:
  // Exposes exit callback to test overrides.
  ScreenExitCallback* exit_callback() { return &exit_callback_; }

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& action_id) override;
  bool HandleAccelerator(LoginAcceleratorAction action) override;

  void CancelChromeVoxHintIdleDetection();
  // ChromeVoxHintDetector::Observer:
  void OnShouldGiveChromeVoxHint() override;

  // SystemTrayObserver:
  void OnFocusLeavingSystemTray(bool reverse) override;
  void OnSystemTrayBubbleShown() override;

  // InputMethodManager::Observer:
  void InputMethodChanged(input_method::InputMethodManager* manager,
                          Profile* profile,
                          bool show_message) override;

  void SetQuickStartButtonVisibility(bool visible);

  // Handlers for various user actions:
  // Proceed with common user flow.
  void OnContinueButtonPressed();
  // Proceed with Demo mode setup.
  void OnSetupDemoMode();
  // Proceed with Enable debugging features flow.
  void OnEnableDebugging();

  // Async callback after ReloadResourceBundle(locale) completed.
  void OnLanguageChangedCallback(
      const InputEventsBlocker* input_events_blocker,
      const std::string& input_method,
      const locale_util::LanguageSwitchResult& result);

  // Starts resolving language list on BlockingPool.
  void ScheduleResolveLanguageList(
      std::unique_ptr<locale_util::LanguageSwitchResult>
          language_switch_result);

  // Callback for ResolveUILanguageList() (from l10n_util).
  void OnLanguageListResolved(base::Value::List new_language_list,
                              const std::string& new_language_list_locale,
                              const std::string& new_selected_language);

  // Callback when the system timezone settings is changed.
  void OnSystemTimezoneChanged();

  // Notifies locale change via LocaleUpdateController.
  void NotifyLocaleChange();
  void OnLocaleChangeResult(LocaleNotificationResult result);

  void OnAccessibilityStatusChanged(
      const AccessibilityStatusEventDetails& details);
  void UpdateA11yState();

  // Starts the QuickStart flow
  void OnQuickStartClicked();

  // Adds data to the OOBE.WelcomeScreen.UserChangedLocale metric and calls
  // exit_callback with given Result
  void Exit(Result result) const;

  base::WeakPtr<WelcomeView> view_;
  ScreenExitCallback exit_callback_;

  std::unique_ptr<ChromeVoxHintDetector> chromevox_hint_detector_;

  std::string input_method_;
  std::string timezone_;

  // The exact language code selected by user in the menu.
  std::string selected_language_code_;

  // Whether the QuickStart entry point visibility has already been determined.
  // This flag prevents duplicate histogram entries.
  bool has_emitted_quick_start_visible = false;

  base::ObserverList<Observer>::Unchecked observers_;

  base::CallbackListSubscription accessibility_subscription_;

  // WeakPtrFactory used to schedule and cancel tasks related to language update
  // in this object.
  base::WeakPtrFactory<WelcomeScreen> language_weak_ptr_factory_{this};

  // WeakPtrFactory used to schedule other tasks in this object.
  base::WeakPtrFactory<WelcomeScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_WELCOME_SCREEN_H_
