// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_OPTIN_ARC_OPTIN_PREFERENCE_HANDLER_H_
#define CHROME_BROWSER_ASH_ARC_OPTIN_ARC_OPTIN_PREFERENCE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace metrics {
class MetricsService;
}

namespace arc {

class ArcOptInPreferenceHandlerObserver;

// This helper encapsulates access to preferences and metrics mode, used in
// OptIn flow. It provides setters for metrics mode and preferences. It also
// observes changes there. Changes in preferences and metrics mode are passed to
// external consumer via ArcOptInPreferenceHandlerObserver. Once started it
// immediately sends current state of metrics mode and preferences.
//
// Note that the preferences and metrics mode passed by this class should only
// be used for the OptIn flow, as this class overrides some of the defaults in
// order to encourage users to consent with the settings.
class ArcOptInPreferenceHandler {
 public:
  ArcOptInPreferenceHandler(ArcOptInPreferenceHandlerObserver* observer,
                            PrefService* pref_service,
                            metrics::MetricsService* metrics_service);

  ArcOptInPreferenceHandler(const ArcOptInPreferenceHandler&) = delete;
  ArcOptInPreferenceHandler& operator=(const ArcOptInPreferenceHandler&) =
      delete;

  ~ArcOptInPreferenceHandler();

  void Start();

  // Enabling metrics happens asynchronously as it depends on device ownership.
  // Once the update has been called, |callback| will be called.
  void EnableMetrics(bool is_enabled, base::OnceClosure callback);
  void EnableBackupRestore(bool is_enabled);
  void EnableLocationService(bool is_enabled);

 private:
  void OnMetricsPreferenceChanged();
  void OnBackupAndRestorePreferenceChanged();
  void OnLocationServicePreferenceChanged();

  // Retrieves ownership status from device settings via callback to determine
  // if the user is the device owner. Returns whether the current user is
  // allowed to update the consent.
  bool IsAllowedToUpdateUserConsent(
      ash::DeviceSettingsService::OwnershipStatus ownership_status);

  // Helper functions to retrieve user metrics consent.
  bool GetUserMetrics();
  void EnableUserMetrics(bool is_enabled);

  // Helper method to update metrics asynchronously. Updating the correct
  // metrics prefs depends on knowing ownership status, which may not be ready
  // immediately.
  //
  // Ownership status will either be None or Taken, it cannot be Unknown.
  void EnableMetricsOnOwnershipKnown(
      bool metrics_enabled,
      ash::DeviceSettingsService::OwnershipStatus ownership_status);

  // Notifies user metrics consent changes to ARC related preferences.
  //
  // Ownership status will either be None or Taken, it cannot be Unknown.
  void SendMetricsMode(
      ash::DeviceSettingsService::OwnershipStatus ownership_status);

  // Utilities on preference update.
  void SendBackupAndRestoreMode();
  void SendLocationServicesMode();

  // Unowned pointers.
  const raw_ptr<ArcOptInPreferenceHandlerObserver> observer_;
  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<metrics::MetricsService> metrics_service_;

  base::OnceClosure enable_metrics_callback_;

  // Used to track metrics preference.
  PrefChangeRegistrar pref_local_change_registrar_;
  // Used to track backup&restore and location service preference.
  PrefChangeRegistrar pref_change_registrar_;
  // Metrics consent observer.
  base::CallbackListSubscription reporting_consent_subscription_;

  base::WeakPtrFactory<ArcOptInPreferenceHandler> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_OPTIN_ARC_OPTIN_PREFERENCE_HANDLER_H_
