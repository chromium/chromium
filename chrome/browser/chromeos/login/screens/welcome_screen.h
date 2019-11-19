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
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "ui/base/ime/chromeos/input_method_manager.h"

namespace chromeos {

class InputEventsBlocker;
class WelcomeView;
class ScreenManager;

namespace locale_util {
struct LanguageSwitchResult;
}

class WelcomeScreen : public BaseScreen,
                      public input_method::InputMethodManager::Observer {
 public:
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when language list is reloaded.
    virtual void OnLanguageListReloaded() = 0;
  };

  WelcomeScreen(WelcomeView* view, const base::RepeatingClosure& exit_callback);
  ~WelcomeScreen() override;

  static WelcomeScreen* Get(ScreenManager* manager);

  // Called when |view| has been destroyed. If this instance is destroyed before
  // the |view| it should call view->Unbind().
  void OnViewDestroyed(WelcomeView* view);

  const std::string& language_list_locale() const {
    return language_list_locale_;
  }
  const base::ListValue* language_list() const { return language_list_.get(); }

  void UpdateLanguageList();

  // Set locale and input method. If |locale| is empty or doesn't change, set
  // the |input_method| directly. If |input_method| is empty or ineligible, we
  // don't change the current |input_method|.
  void SetApplicationLocaleAndInputMethod(const std::string& locale,
                                          const std::string& input_method);
  std::string GetApplicationLocale();
  std::string GetInputMethod() const;

  void SetApplicationLocale(const std::string& locale);
  void SetInputMethod(const std::string& input_method);
  void SetTimezone(const std::string& timezone_id);
  std::string GetTimezone() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // BaseScreen implementation:
  void Show() override;
  void Hide() override;
  void OnUserAction(const std::string& action_id) override;

 protected:
  // Exposes exit callback to test overrides.
  base::RepeatingClosure* exit_callback() { return &exit_callback_; }

 private:

  // InputMethodManager::Observer implementation:
  void InputMethodChanged(input_method::InputMethodManager* manager,
                          Profile* profile,
                          bool show_message) override;

  // Called when continue button is pressed.
  void OnContinueButtonPressed();

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

  WelcomeView* view_ = nullptr;
  base::RepeatingClosure exit_callback_;

  std::string input_method_;
  std::string timezone_;

  // Creation of language list happens on Blocking Pool, so we cache
  // resolved data.
  std::string language_list_locale_;
  std::unique_ptr<base::ListValue> language_list_;

  // The exact language code selected by user in the menu.
  std::string selected_language_code_;

  base::ObserverList<Observer>::Unchecked observers_;

  base::WeakPtrFactory<WelcomeScreen> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WelcomeScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_WELCOME_SCREEN_H_
