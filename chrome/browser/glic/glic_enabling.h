// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_ENABLING_H_
#define CHROME_BROWSER_GLIC_GLIC_ENABLING_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "base/types/expected.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class Profile;

namespace glic {

// This class provides a central location for checking if GLIC is enabled. It
// allows for future expansion to include other ways the feature may be disabled
// such as based on user preferences or system settings.
//
// There are multiple notions of "enabled". The highest level is
// IsEnabledByFlags which controls whether any global-Glic infrastructure is
// created. If flags are off, nothing Glic-related should be created.
//
// If flags are enabled, various global objects are created as well as a
// GlicKeyedService for each "eligible" profile. Eligible profiles exclude
// incognito, guest mode, system profile, etc. i.e. include only real user
// profiles. An eligible profile will create various Glic decorations and views
// for the profile's browser windows, regardless of whether Glic is actually
// "enabled" for the given profile. If disabled, those decorations should remain
// inert.  The GlicKeyedService is created for all eligible profiles so it can
// listen for changes to prefs which control the per-profile Glic-Enabled state.
//
// Finally, an eligible profile may be Glic-Enabled. In this state, Glic UI is
// visible and usable by the user. This state can change at runtime so Glic
// entry points should depend on this state.
class GlicEnabling : public signin::IdentityManager::Observer {
 public:
  // Returns whether the global Glic feature is enabled for Chrome. This status
  // will not change at runtime.
  static bool IsEnabledByFlags();

  // Some profiles - such as incognito, guest, system profile, etc. - are never
  // eligible to use Glic. This function returns true if a profile is eligible
  // for Glic, that is, it can potentially be enabled, regardless of whether it
  // is currently enabled or not. Always returns false if IsEnabledByFlags is
  // off. This will never change for a given profile.
  static bool IsProfileEligible(const Profile* profile);

  // This is a convenience method for code outside of //chrome/browser/glic.
  // Code inside should use instance method IsEnabled() instead.
  static bool IsEnabledForProfile(Profile* profile);

  // Returns true if the given profile has Glic enabled and has completed the
  // FRE. True implies that IsEnabledByFlags(), IsProfileEligible(profile), and
  // IsEnabledForProfile(profile) are also true. This value can change at
  // runtime.
  static bool IsEnabledAndConsentForProfile(Profile* profile);

  // Whether or not the profile is currently ready for Glic. This means no
  // additional steps must be taken before opening Glic.
  static bool IsReadyForProfile(Profile* profile);

  // The settings page is shown when:
  // * Flags are enabled
  // * The profile is eligible (regular, non-incognito, non-guest, etc.)
  // * The profile has model execution privileges
  // * The profile has completed the first run experience
  static bool ShouldShowSettingsPage(Profile* profile);

  explicit GlicEnabling(Profile* profile);
  ~GlicEnabling() override;

  // TODO(crbug.com/390487066): This method is misnamed. It would be more
  // accurate to call it `IsAllowed()`.
  // Returns true if the given profile is allowed to use glic. This means that
  // IsProfileEligible() returns true and:
  //   * the profile is signed in
  //   * can_use_model_execution is true
  //   * glic is allowed by enterprise policy.
  // This value can change at runtime.
  //
  // Once a profile is allowed to run glic, there are several more checks that
  // are required to use glic although many callsites may not care about all of
  // these:
  //   * FRE has been passed. There is no way to permanently decline FRE, as
  //     it's only invoked on user interaction with glic entry points.
  //   * Entry point specific flags (e.g. kGlicPinnedToTabstrip).
  //   * Profile is not paused.
  // If all entry-points have been disabled, then glic is functionally disabled.
  bool IsEnabled();

  // This is called anytime IsEnabled() might return a different value.
  using EnableChangedCallback = base::RepeatingClosure;
  base::CallbackListSubscription RegisterEnableChanged(
      EnableChangedCallback callback);

 private:
  void OnGlicSettingsPolicyChanged();

  // IdentityManagerObserver:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;

  // Detects changes to capabilities.
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;

  void OnRefreshTokensLoaded() override;

  // Detects paused state.
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;

  raw_ptr<Profile> profile_;
  using EnableChangedCallbackList = base::RepeatingCallbackList<void()>;
  EnableChangedCallbackList enable_changed_callback_list_;
  PrefChangeRegistrar pref_registrar_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_ENABLING_H_
