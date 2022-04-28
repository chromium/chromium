// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_WELCOME_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_WELCOME_SCREEN_H_

#include <memory>
#include <string>

#include "ash/public/cpp/locale_update_controller.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
// TODO(https://crbug.com/1164001): forward declare LanguageSwitchResult
// after this file is moved to ash.
#include "chrome/browser/ash/base/locale_util.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/chromevox_hint/chromevox_hint_detector.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ash/login/ui/input_events_blocker.h"
#include "chrome/browser/ash/login/wizard_context.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "ui/base/ime/ash/input_method_manager.h"

namespace ash {

class WelcomeScreen : public BaseScreen,
                      public input_method::InputMethodManager::Observer,
                      public ChromeVoxHintDetector::Observer {
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
    NEXT,
    NEXT_OS_INSTALL,
    SETUP_DEMO,
    ENABLE_DEBUGGING,
    QUICK_START
  };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  WelcomeScreen(WelcomeView* view, const ScreenExitCallback& exit_callback);

  WelcomeScreen(const WelcomeScreen&) = delete;
  WelcomeScreen& operator=(const WelcomeScreen&) = delete;

  ~WelcomeScreen() override;

  static std::string GetResultString(Result result);

  // Called when `view` has been destroyed. If this instance is destroyed before
  // the `view` it should call view->Unbind().
  void OnViewDestroyed(WelcomeView* view);

  const std::string& language_list_locale() const {
    return language_list_locale_;
  }
  const base::Value& language_list() const { return language_list_; }

  void UpdateLanguageList();

  // Set locale and input method. If `locale` is empty or doesn't change, set
  // the `input_method` directly. If `input_method` is empty or ineligible, we
  // don't change the current `input_method`.
  void SetApplicationLocaleAndInputMethod(const std::string& locale,
                                          const std::string& input_method);
  std::string GetApplicationLocale();
  std::string GetInputMethod() const;

  void SetApplicationLocale(const std::string& locale, const bool is_from_ui);
  void SetInputMethod(const std::string& input_method);
  void SetTimezone(const std::string& timezone_id);
  std::string GetTimezone() const;

  void SetDeviceRequisition(const std::string& requisition);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  base::Value* GetConfigurationForTesting() {
    return &(context()->configuration);
  }

  // ChromeVox hint.
  void CancelChromeVoxHintIdleDetection();
  ChromeVoxHintDetector* GetChromeVoxHintDetectorForTesting();

 protected:
  // Exposes exit callback to test overrides.
  ScreenExitCallback* exit_callback() { return &exit_callback_; }

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserActionDeprecated(const std::string& action_id) override;
  bool HandleAccelerator(LoginAcceleratorAction action) override;

  // ChromeVoxHintDetector::Observer:
  void OnShouldGiveChromeVoxHint() override;

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

  // Notifies locale change via LocaleUpdateController.
  void NotifyLocaleChange();
  void OnLocaleChangeResult(LocaleNotificationResult result);

  // Updates the local variable according to the existence of the Chromad
  // migration flag file. Then, simulates a user action, if the flag is set and
  // the screen is not hidden.
  void UpdateChromadMigrationOobeFlow(bool exists);

  // Adds data to the OOBE.WelcomeScreen.UserChangedLocale metric and calls
  // exit_callback with given Result
  void Exit(Result result) const;

  WelcomeView* view_ = nullptr;
  ScreenExitCallback exit_callback_;

  std::unique_ptr<ChromeVoxHintDetector> chromevox_hint_detector_;

  std::string input_method_;
  std::string timezone_;

  // Creation of language list happens on Blocking Pool, so we cache
  // resolved data.
  std::string language_list_locale_;
  base::Value language_list_{base::Value::Type::LIST};

  // The exact language code selected by user in the menu.
  std::string selected_language_code_;

  base::ObserverList<Observer>::Unchecked observers_;

  // This local flag should be true if the OOBE flow is operating as part of the
  // Chromad to cloud device migration. If so, this screen should be skipped.
  bool is_chromad_migration_oobe_flow_ = false;

  // This local flag should be true if there was a language change from the UI,
  // it's value will be written into the OOBE.WelcomeScreen.UserChangedLocale
  // metric when we exit the WelcomeScreen.
  bool is_locale_changed_ = false;

  // WeakPtrFactory used to schedule and cancel tasks related to language update
  // in this object.
  base::WeakPtrFactory<WelcomeScreen> language_weak_ptr_factory_{this};

  // WeakPtrFactory used to schedule other tasks in this object.
  base::WeakPtrFactory<WelcomeScreen> weak_ptr_factory_{this};
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::WelcomeScreen;
}

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::WelcomeScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_WELCOME_SCREEN_H_
