// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/marketing_opt_in_screen.h"

#include <cstddef>
#include <string>
#include <unordered_set>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/apps/user_type_filter.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/marketing_backend_connector.h"
#include "chrome/browser/ash/login/screen_manager.h"
#include "chrome/browser/ash/login/screens/gesture_navigation_screen.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/system/timezone_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/gesture_navigation_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/marketing_opt_in_screen_handler.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

constexpr char kUserActionGetStarted[] = "get-started";
constexpr char kUserActionSetA11yNavigationButtonsEnabled[] =
    "set-a11y-button-enable";

void RecordShowShelfNavigationButtonsValueChange(bool enabled) {
  base::UmaHistogramBoolean(
      "Accessibility.CrosShelfNavigationButtonsInTabletModeChanged.OOBE",
      enabled);
}

// Records the opt-in and opt-out rates for Chromebook emails. Differentiates
// between users who have a default opt-in vs. a default opt-out option.
void RecordOptInAndOptOutRates(const bool user_opted_in,
                               const bool opt_in_by_default,
                               const std::string& country) {
  MarketingOptInScreen::Event event;
  if (opt_in_by_default)  // A 'checked' toggle was shown.
    event = (user_opted_in)
                ? MarketingOptInScreen::Event::kUserOptedInWhenDefaultIsOptIn
                : MarketingOptInScreen::Event::kUserOptedOutWhenDefaultIsOptIn;
  else  // An 'unchecked' toggle was shown
    event = (user_opted_in)
                ? MarketingOptInScreen::Event::kUserOptedInWhenDefaultIsOptOut
                : MarketingOptInScreen::Event::kUserOptedOutWhenDefaultIsOptOut;

  base::UmaHistogramEnumeration("OOBE.MarketingOptInScreen.Event." + country,
                                event);
  // Generic event aggregating data from all countries.
  base::UmaHistogramEnumeration("OOBE.MarketingOptInScreen.Event", event);
}

void RecordGeolocationResolve(MarketingOptInScreen::GeolocationEvent event) {
  base::UmaHistogramEnumeration("OOBE.MarketingOptInScreen.GeolocationResolve",
                                event);
}

}  // namespace

// static
std::string MarketingOptInScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::NEXT:
      return "Next";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

MarketingOptInScreen::MarketingOptInScreen(
    base::WeakPtr<MarketingOptInScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(MarketingOptInScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {
  DCHECK(view_);
}

MarketingOptInScreen::~MarketingOptInScreen() {
  if (a11y_nav_buttons_toggle_metrics_reporter_timer_.IsRunning()) {
    a11y_nav_buttons_toggle_metrics_reporter_timer_.FireNow();
  }
}

bool MarketingOptInScreen::MaybeSkip(WizardContext& context) {
  if (context.skip_post_login_screens_for_tests) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }
  Initialize();

  if (chrome_user_manager_util::IsManagedGuestSessionOrEphemeralLogin() ||
      IsCurrentUserManaged() || !context.is_branded_build) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }

  return false;
}

void MarketingOptInScreen::ShowImpl() {
  DCHECK(initialized_);

  // Show a verbose legal footer for Canada. (https://crbug.com/1124956)
  const bool legal_footer_visible =
      email_opt_in_visible_ && countries_with_legal_footer.count(country_);

  const bool cloud_gaming_enabled =
      chromeos::features::IsCloudGamingDeviceEnabled();

  if (view_) {
    view_->Show(/*opt_in_visible=*/email_opt_in_visible_,
                /*opt_in_default_state=*/IsDefaultOptInCountry(),
                /*legal_footer_visible=*/legal_footer_visible,
                /*cloud_gaming_enabled=*/cloud_gaming_enabled);
  }

  // Mark the screen as shown for this user.
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kOobeMarketingOptInScreenFinished, true);

  // Only show the link for accessibility settings if the gesture navigation
  // screen was shown.
  if (view_) {
    view_->UpdateA11ySettingsButtonVisibility(
        context()->is_gesture_navigation_screen_was_shown ||
        switches::ShouldShowAccessibilityButtonOnMarketingOptInForTesting());

    view_->UpdateA11yShelfNavigationButtonToggle(prefs->GetBoolean(
        prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled));
  }

  // Observe the a11y shelf navigation buttons pref so the setting toggle in the
  // screen can be updated if the pref value changes.
  active_user_pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  active_user_pref_change_registrar_->Init(prefs);
  active_user_pref_change_registrar_->Add(
      prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled,
      base::BindRepeating(
          &MarketingOptInScreen::OnA11yShelfNavigationButtonPrefChanged,
          base::Unretained(this)));
}

void MarketingOptInScreen::HideImpl() {
  if (is_hidden())
    return;
  active_user_pref_change_registrar_.reset();
}

void MarketingOptInScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();

  if (action_id == kUserActionGetStarted) {
    CHECK_EQ(args.size(), 2u);
    const bool chromebook_email_opt_in = args[1].GetBool();
    OnGetStarted(chromebook_email_opt_in);
    return;
  }
  if (action_id == kUserActionSetA11yNavigationButtonsEnabled) {
    CHECK_EQ(args.size(), 2u);
    const bool enabled = args[1].GetBool();
    SetA11yNavigationButtonsEnabled(enabled);
    return;
  }
  BaseScreen::OnUserAction(args);
}

void MarketingOptInScreen::OnGetStarted(bool chromebook_email_opt_in) {
  if (is_hidden())
    return;
  DCHECK(initialized_);

  // UMA Metrics & API call only when the toggle is visible
  if (email_opt_in_visible_) {
    RecordOptInAndOptOutRates(/*user_opted_in=*/chromebook_email_opt_in,
                              /*opt_in_by_default=*/IsDefaultOptInCountry(),
                              /*country=*/country_);

    // Store the user's preference regarding marketing emails
    Profile* profile = ProfileManager::GetActiveUserProfile();
    DCHECK(profile);
    profile->GetPrefs()->SetBoolean(prefs::kOobeMarketingOptInChoice,
                                    chromebook_email_opt_in);
    if (chromebook_email_opt_in) {
      // Call Chromebook Email Service API
      MarketingBackendConnector::UpdateEmailPreferences(profile, country_);
    }
  }

  exit_callback_.Run(Result::NEXT);
}

void MarketingOptInScreen::SetA11yButtonVisibilityForTest(bool shown) {
  if (view_) {
    view_->UpdateA11ySettingsButtonVisibility(shown);
  }
}

void MarketingOptInScreen::OnA11yShelfNavigationButtonPrefChanged() {
  if (!view_)
    return;
  view_->UpdateA11yShelfNavigationButtonToggle(
      ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
          prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled));
}

bool MarketingOptInScreen::IsCurrentUserManaged() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (profile->IsOffTheRecord())
    return false;
  const std::string user_type = apps::DetermineUserType(profile);
  return (user_type != apps::kUserTypeUnmanaged);
}

void MarketingOptInScreen::Initialize() {
  // Set the country to be used based on the timezone
  // and supported country list.
  SetCountryFromTimezoneIfAvailable(g_browser_process->local_state()->GetString(
      ::prefs::kSigninScreenTimezone));

  // Only show the opt in option if this is a supported region, and if the user
  // never made a choice regarding emails.
  email_opt_in_visible_ = !country_.empty() && ShouldShowOptionToSubscribe();

  initialized_ = true;
}

void MarketingOptInScreen::SetCountryFromTimezoneIfAvailable(
    const std::string& timezone) {
  auto region = system::GetCountryCodeFromTimezoneIfAvailable(timezone);
  if (!region.has_value()) {
    RecordGeolocationResolve(GeolocationEvent::kCouldNotDetermineCountry);
    return;
  }

  // Track whether we could successfully resolve.
  RecordGeolocationResolve(GeolocationEvent::kCountrySuccessfullyDetermined);

  // Set the country
  country_.clear();
  const bool is_default_country = default_countries_.count(region.value());
  const bool is_extended_country = additional_countries_.count(region.value());
  const bool is_double_optin_country =
      double_opt_in_countries_.count(region.value());

  if (is_default_country || is_extended_country || is_double_optin_country) {
    country_ = region.value();
  }
}

void MarketingOptInScreen::SetA11yNavigationButtonsEnabled(bool enabled) {
  ProfileManager::GetActiveUserProfile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled, enabled);
  a11y_nav_buttons_toggle_metrics_reporter_timer_.Start(
      FROM_HERE, base::Seconds(10),
      base::BindOnce(&RecordShowShelfNavigationButtonsValueChange, enabled));
}

bool MarketingOptInScreen::ShouldShowOptionToSubscribe() {
  // Directly access PrefServiceSyncable instead of PrefService because
  // we need to know whether the prefs have been loaded.
  sync_preferences::PrefServiceSyncable* prefs =
      PrefServiceSyncableFromProfile(ProfileManager::GetActiveUserProfile());
  const bool sync_complete =
      ignore_pref_sync_for_testing_ || prefs->AreOsPrefsSyncing();
  // Do not show if the preferences cannot be synced
  if (!sync_complete)
    return false;

  // Show if the screen was never shown before.
  if (!prefs->GetBoolean(prefs::kOobeMarketingOptInScreenFinished))
    return true;

  // Show if we haven't recorded the user's choice yet. The screen was shown
  // in the past, but the option to sign up was not (due to not being
  // available in the user's region).
  if (prefs->FindPreference(prefs::kOobeMarketingOptInChoice)->IsDefaultValue())
    return true;

  // The screen was shown before and the user has made its choice.
  return false;
}

}  // namespace ash
