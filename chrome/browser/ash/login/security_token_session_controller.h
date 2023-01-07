// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SECURITY_TOKEN_SESSION_CONTROLLER_H_
#define CHROME_BROWSER_ASH_LOGIN_SECURITY_TOKEN_SESSION_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/certificate_provider/certificate_provider_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/user.h"
#include "extensions/common/extension_id.h"

namespace views {
class Widget;
}

namespace chromeos {
class CertificateProvider;
}

namespace ash::login {

// A controller that implements the combined behavior of the
// SecurityTokenSessionBehavior and SecurityTokenSessionNotificationSeconds
// preferences. When a user is authenticating via a security token (e.g., with a
// smart card), SecurityTokenSessionBehavior dictates what should happen if the
// certificate ceases to be present while the user is logged in.
// SecurityTokenSessionNotificationSeconds determines if and how long the user
// is getting informed what is going to happen when the certificate vanishes.
class SecurityTokenSessionController
    : public KeyedService,
      public chromeos::CertificateProviderService::Observer,
      public session_manager::SessionManagerObserver {
 public:
  enum class Behavior { kIgnore, kLogout, kLock };
  // A key in the known_user database that stores the boolean flag: whether the
  // notification has been shown or not.
  static const char* const kNotificationDisplayedKnownUserKey;

  SecurityTokenSessionController(
      bool is_user_profile,
      PrefService* local_state,
      const user_manager::User* primary_user,
      chromeos::CertificateProviderService* certificate_provider_service);
  SecurityTokenSessionController(const SecurityTokenSessionController& other) =
      delete;
  SecurityTokenSessionController& operator=(
      const SecurityTokenSessionController& other) = delete;
  ~SecurityTokenSessionController() override;

  // KeyedService
  void Shutdown() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // If this controller logged the user out just before, display a notification
  // explaining why this happened. This is only done the first time this
  // happens for a user on a device.
  static void MaybeDisplayLoginScreenNotification();

  // Informs the controller that there are new challenge response keys stored
  // in known_user. This will not immediately check that all required
  // certificates are present, since this happens during login when extensions
  // providing certificates are not yet initialized.
  void OnChallengeResponseKeysUpdated();

  // CertificateProviderService::Observer:
  void OnCertificatesUpdated(
      const std::string& extension_id,
      const std::vector<chromeos::certificate_provider::CertificateInfo>&
          certificate_infos) override;

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

 private:
  bool ShouldApplyPolicyInCurrentSessionState() const;
  Behavior GetBehaviorFromPrefAndSessionState() const;
  void UpdateBehavior();
  void UpdateKeepAlive();
  void UpdateNotificationPref();

  void ExtensionProvidesAllRequiredCertificates(
      const extensions::ExtensionId& extension_id);
  void ExtensionStopsProvidingCertificate(
      const extensions::ExtensionId& extension_id);
  void TriggerAction();
  void AddLockNotification();
  void ScheduleLogoutNotification();
  void Reset();

  bool GetNotificationDisplayedKnownUserFlag() const;
  void SetNotificationDisplayedKnownUserFlag();

  const bool is_user_profile_;
  PrefService* const local_state_;
  const user_manager::User* const primary_user_;
  chromeos::CertificateProviderService* certificate_provider_service_ = nullptr;
  session_manager::SessionManager* const session_manager_;
  std::unique_ptr<crosapi::BrowserManager::ScopedKeepAlive> keep_alive_;
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_manager_observation_{this};
  PrefChangeRegistrar pref_change_registrar_;
  Behavior behavior_ = Behavior::kIgnore;
  base::TimeDelta notification_seconds_;
  base::flat_set<extensions::ExtensionId> observed_extensions_;
  base::flat_map<extensions::ExtensionId, std::vector<std::string>>
      extension_to_spkis_;
  base::flat_set<extensions::ExtensionId>
      extensions_missing_required_certificates_;
  views::Widget* fullscreen_notification_ = nullptr;
  base::OneShotTimer action_timer_;
  std::unique_ptr<chromeos::CertificateProvider> certificate_provider_;
  // Whether all of the user's certificates have been provided at least once by
  // the extensions. This field is reset every time the session state changes.
  bool all_required_certificates_were_observed_ = false;
  // Whether the session state has transitioned into the `LOCKED` session state
  // at least once.
  bool had_lock_screen_transition_ = false;

  base::WeakPtrFactory<SecurityTokenSessionController> weak_ptr_factory_{this};
};

}  // namespace ash::login

#endif  // CHROME_BROWSER_ASH_LOGIN_SECURITY_TOKEN_SESSION_CONTROLLER_H_
