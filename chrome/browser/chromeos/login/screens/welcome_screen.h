// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_WELCOME_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_WELCOME_SCREEN_H_

#include <memory>
#include <string>

#include "ash/public/cpp/locale_update_controller.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_mode_detector.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/wizard_context.h"
#include "ui/base/ime/chromeos/input_method_manager.h"

namespace chromeos {

class InputEventsBlocker;
class WelcomeView;

namespace locale_util {
struct LanguageSwitchResult;
}

class WelcomeScreen : public BaseScreen,
                      public input_method::InputMethodManager::Observer,
                      public DemoModeDetector::Observer {
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
    virtual ~Observer() {}

    // Called when language list is reloaded.
    virtual void OnLanguageListReloaded() = 0;
  };

  enum class Result { NEXT, START_DEMO, SETUP_DEMO, ENABLE_DEBUGGING };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  WelcomeScreen(WelcomeView* view, const ScreenExitCallback& exit_callback);
  ~WelcomeScreen() override;

  static std::string GetResultString(Result result);

  // Called when `view` has been destroyed. If this instance is destroyed before
  // the `view` it should call view->Unbind().
  void OnViewDestroyed(WelcomeView* view);

  const std::string& language_list_locale() const {
    return language_list_locale_;
  }
  const base::ListValue* language_list() const { return language_list_.get(); }

  void UpdateLanguageList();

  // Set locale and input method. If `locale` is empty or doesn't change, set
  // the `input_method` directly. If `input_method` is empty or ineligible, we
  // don't change the current `input_method`.
  void SetApplicationLocaleAndInputMethod(const std::string& locale,
                                          const std::string& input_method);
  std::string GetApplicationLocale();
  std::string GetInputMethod() const;

  void SetApplicationLocale(const std::string& locale);
  void SetInputMethod(const std::string& input_method);
  void SetTimezone(const std::string& timezone_id);
  std::string GetTimezone() const;

  void SetDeviceRequisition(const std::string& requisition);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  base::Value* GetConfigurationForTesting() {
    return &(context()->configuration);
  }

  void CancelChromeVoxHintTimer();
  void GiveChromeVoxHintForTesting();
  bool GetChromeVoxHintTimerCancelledForTesting() {
    return chromevox_hint_timer_cancelled_for_testing_;
  }

 protected:
  // Exposes exit callback to test overrides.
  ScreenExitCallback* exit_callback() { return &exit_callback_; }

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;
  bool HandleAccelerator(ash::LoginAcceleratorAction action) override;

  // DemoModeDetector::Observer:
  void OnShouldStartDemoMode() override;

  // InputMethodManager::Observer:
  void InputMethodChanged(input_method::InputMethodManager* manager,
                          Profile* profile,
                          bool show_message) override;

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

  // Callback for chromeos::ResolveUILanguageList() (from l10n_util).
  void OnLanguageListResolved(
      std::unique_ptr<base::ListValue> new_language_list,
      const std::string& new_language_list_locale,
      const std::string& new_selected_language);

  // Callback when the system timezone settings is changed.
  void OnSystemTimezoneChanged();

  // Notifies locale change via ash::LocaleUpdateController.
  void NotifyLocaleChange();
  void OnLocaleChangeResult(ash::LocaleNotificationResult result);

  // ChromeVox hint.
  void StartChromeVoxHintTimer();
  void GiveChromeVoxHint();

  WelcomeView* view_ = nullptr;
  ScreenExitCallback exit_callback_;

  std::unique_ptr<DemoModeDetector> demo_mode_detector_;

  std::string input_method_;
  std::string timezone_;

  // Creation of language list happens on Blocking Pool, so we cache
  // resolved data.
  std::string language_list_locale_;
  std::unique_ptr<base::ListValue> language_list_;

  // The exact language code selected by user in the menu.
  std::string selected_language_code_;

  base::ObserverList<Observer>::Unchecked observers_;

  base::OneShotTimer chromevox_hint_timer_;

  bool chromevox_hint_timer_activated_ = false;

  bool chromevox_hint_timer_cancelled_for_testing_ = false;

  base::WeakPtrFactory<WelcomeScreen> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WelcomeScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_WELCOME_SCREEN_H_
