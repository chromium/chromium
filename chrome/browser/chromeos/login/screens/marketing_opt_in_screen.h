// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MARKETING_OPT_IN_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MARKETING_OPT_IN_SCREEN_H_

#include <memory>
#include <unordered_set>
#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "components/prefs/pref_change_registrar.h"

namespace chromeos {

class MarketingOptInScreenView;

// This is Sync settings screen that is displayed as a part of user first
// sign-in flow.
class MarketingOptInScreen : public BaseScreen {
 public:
  using TView = MarketingOptInScreenView;

  enum class Result { NEXT, NOT_APPLICABLE };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. Must coincide with the enum
  // MarketingOptInScreenEvent
  enum class Event {
    kUserOptedInWhenDefaultIsOptIn = 0,
    kUserOptedInWhenDefaultIsOptOut = 1,
    kUserOptedOutWhenDefaultIsOptIn = 2,
    kUserOptedOutWhenDefaultIsOptOut = 3,
    kMaxValue = kUserOptedOutWhenDefaultIsOptOut,
  };

  // Whether the geolocation resolve was successful.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. Must coincide with the enum
  // MarketingOptInScreenEvent
  enum class GeolocationEvent {
    kCouldNotDetermineCountry = 0,
    kCountrySuccessfullyDetermined = 1,
    kMaxValue = kCountrySuccessfullyDetermined,
  };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  MarketingOptInScreen(MarketingOptInScreenView* view,
                       const ScreenExitCallback& exit_callback);
  ~MarketingOptInScreen() override;

  // On "Get Started" button pressed.
  void OnGetStarted(bool chromebook_email_opt_in);

  void SetA11yButtonVisibilityForTest(bool shown);

  void set_exit_callback_for_testing(const ScreenExitCallback& exit_callback) {
    exit_callback_ = exit_callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

 protected:
  // BaseScreen:
  bool MaybeSkip(WizardContext* context) override;
  void ShowImpl() override;
  void HideImpl() override;

 private:
  void OnA11yShelfNavigationButtonPrefChanged();

  // Checks whether this user is managed.
  bool IsCurrentUserManaged();

  // Sets the country to be used if the feature is available in this region.
  void SetCountryFromTimezoneIfAvailable(const std::string& timezone_id);

  bool IsDefaultOptInCountry() {
    return default_opt_in_countries_.count(country_);
  }

  MarketingOptInScreenView* const view_;
  ScreenExitCallback exit_callback_;
  std::unique_ptr<PrefChangeRegistrar> active_user_pref_change_registrar_;

  // Whether the email opt-in toggle is visible.
  bool email_opt_in_visible_ = false;

  // Country code. Unknown IFF empty.
  std::string country_;

  // Default country list.
  const std::unordered_set<std::string> default_countries_{"us", "ca", "gb"};

  // Extended country list. Protected behind the flag:
  // - kOobeMarketingAdditionalCountriesSupported (DEFAULT_ON)
  const std::unordered_set<std::string> additional_countries_{
      "fr", "nl", "fi", "se", "no", "dk", "es", "it", "jp", "au"};

  // Countries with double opt-in.  Behind the flag:
  // - kOobeMarketingDoubleOptInCountriesSupported (DEFAULT_OFF)
  const std::unordered_set<std::string> double_opt_in_countries_{"de"};

  // Countries in which the toggle will be enabled by default.
  const std::unordered_set<std::string> default_opt_in_countries_{"us"};

  base::WeakPtrFactory<MarketingOptInScreen> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(MarketingOptInScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MARKETING_OPT_IN_SCREEN_H_
