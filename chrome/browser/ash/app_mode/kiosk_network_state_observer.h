// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_NETWORK_STATE_OBSERVER_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_NETWORK_STATE_OBSERVER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace ash {

// Max number of attempts to expose active WiFi.
inline constexpr size_t kMaxWifiExposureAttempts = 3;

// When `KioskActiveWiFiCredentialsScopeChangeEnabled` policy is enabled,
// expose the first active WiFi configuration to the device level.
class KioskNetworkStateObserver : public NetworkStateHandlerObserver {
 public:
  explicit KioskNetworkStateObserver(PrefService* pref_service);

  KioskNetworkStateObserver(const KioskNetworkStateObserver&) = delete;
  KioskNetworkStateObserver& operator=(const KioskNetworkStateObserver&) =
      delete;
  ~KioskNetworkStateObserver() override;

  bool IsPolicyEnabled() const;

  // This callback is called after an attempt to expose active WiFi.
  // `is_successful_attempt` indicates where the attempt was successful or not.
  using WifiExposureAttemptCallback =
      base::RepeatingCallback<void(bool is_successful_attempt)>;

  void SetWifiExposureAttemptCallbackForTesting(
      WifiExposureAttemptCallback callback);

 private:
  // NetworkStateHandlerObserver overrides:
  void ActiveNetworksChanged(
      const std::vector<const NetworkState*>& active_networks) override;

  void StartActiveWifiExposureProcess();
  void StopActiveWifiExposureProcess();

  void ExposeActiveWifiConfiguration();

  void OnGetWifiPassphraseResult(const std::string& service_path,
                                 const std::string& passphrase);
  void OnGetWifiPassphraseError(const std::string& error_name,
                                const std::string& error_message);
  void ReceiveProperties(const std::string& passphrase,
                         const std::string& service_path,
                         std::optional<base::Value::Dict> shill_properties);

  void OnCreatedShillConfigSuccess(const std::string&, const std::string&);
  void OnCreatedShillConfigFailure(const std::string& error);

  // This function is called once
  // `prefs::kKioskActiveWiFiCredentialsScopeChangeEnabled` preference is
  // updated.
  void PolicyChanged();

  // If the attempt to expose WiFi fails, call this function to update
  // `active_wifi_exposed_` and `wifi_exposure_attempts_`.
  void FailCurrentAttempt();

  // Does nothing if `wifi_exposure_attempt_callback_for_testing_` is not set.
  // Propagates `is_successful_attempt` to
  // `wifi_exposure_attempt_callback_for_testing_`.
  void MaybeRunWifiExposureAttemptCallback(bool is_successful_attempt);

  // Copy only one active WiFi. This helps to avoid a situation when
  // `ActiveNetworksChanged` is called second time before we unsubscribe on
  // the success WiFi exposure.
  bool active_wifi_exposed_ = false;

  // To avoid a failure loop, stop trying to expose the active WiFi after
  // `kMaxWifiExposureAttempts`
  size_t wifi_exposure_attempts_ = 0;

  const raw_ptr<PrefService> pref_service_;
  // Register `prefs::kKioskActiveWiFiCredentialsScopeChangeEnabled`
  // preference to support dynamic refresh.
  PrefChangeRegistrar pref_change_registrar_;

  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observation_{this};

  // This callback should be called after each successful or failed attempt. The
  // callback should be set only for testing via
  // `SetWifiExposureAttemptCallbackForTesting`.
  WifiExposureAttemptCallback wifi_exposure_attempt_callback_;

  base::WeakPtrFactory<KioskNetworkStateObserver> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_NETWORK_STATE_OBSERVER_H_
