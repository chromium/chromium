// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_SYSTEM_PROXY_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_SYSTEM_PROXY_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util.h"
#include "chromeos/dbus/system_proxy/system_proxy_service.pb.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/content_browser_client.h"
#include "net/base/auth.h"

namespace chromeos {
class NetworkState;
class RequestSystemProxyCredentialsView;
class SystemProxyNotification;
}  // namespace chromeos

namespace content {
class LoginDelegate;
}

namespace system_proxy {
class SetAuthenticationDetailsResponse;
class ShutDownResponse;
}  // namespace system_proxy

namespace views {
class Widget;
}

class PrefRegistrySimple;
class PrefService;
class PrefChangeRegistrar;
class Profile;

namespace policy {

// This class observes the device setting |SystemProxySettings|, and controls
// the availability of System-proxy service and the configuration of the web
// proxy credentials for system services connecting through System-proxy. It
// also listens for the |WorkerActive| dbus signal sent by the System-proxy
// daemon and stores connection information regarding the active worker
// processes.
// TODO(acostinas, https://crbug.com/1145174): Move the logic that tracks
// managed network changes to another class.
class SystemProxyManager : public chromeos::NetworkStateHandlerObserver {
 public:
  SystemProxyManager(chromeos::CrosSettings* cros_settings,
                     PrefService* local_state);
  SystemProxyManager(const SystemProxyManager&) = delete;

  SystemProxyManager& operator=(const SystemProxyManager&) = delete;

  ~SystemProxyManager() override;

  // If System-proxy is enabled by policy, it returns the URL of the local proxy
  // instance that authenticates system services, in PAC format, e.g.
  //     PROXY localhost:3128
  // otherwise it returns an empty string.
  std::string SystemServicesProxyPacString() const;
  void StartObservingPrimaryProfilePrefs(Profile* profile);
  void StopObservingPrimaryProfilePrefs();
  // If System-proxy is enabled, it will send a request via D-Bus to clear the
  // user's proxy credentials cached by the local proxy workers. System-proxy
  // requests proxy credentials from the browser by sending an
  // |AuthenticationRequired| D-Bus signal.
  void ClearUserCredentials();

  void SetSystemProxyEnabledForTest(bool enabled);
  void SetSystemServicesProxyUrlForTest(const std::string& local_proxy_url);
  void SetSendAuthDetailsClosureForTest(base::RepeatingClosure closure);
  chromeos::RequestSystemProxyCredentialsView* GetActiveAuthDialogForTest();
  void CloseAuthDialogForTest();

  // Registers prefs stored in user profiles.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Indicates whether the credentials set via the device policy
  // SystemProxySettings can be used for proxy authentication in Chrome. The
  // following conditions must be true:
  // - the current session must be Managed Guest Session (MGS) or Kiosk app;
  // - the proxy is set via policy;
  // - System-proxy is enabled and credentials are set via policy;
  // - `first_auth_attempt` is true;
  // - `auth_info.scheme` must be allowed by the SystemProxySettings policy.
  bool CanUsePolicyCredentials(const net::AuthChallengeInfo& auth_info,
                               bool first_auth_attempt);

  // Returns a login delegate that posts `auth_required_callback` with the
  // credentials provided by the policy SystemProxySettings. Callers must verify
  // that `CanUsePolicyCredentials` is true before calling this method.
  std::unique_ptr<content::LoginDelegate> CreateLoginDelegate(
      LoginAuthRequiredCallback auth_required_callback);

 private:
  // NetworkStateHandlerObserver implementation
  void DefaultNetworkChanged(const chromeos::NetworkState* network) override;
  // Called when the proxy configurations may have changed either by updates to
  // the kProxy policy or updates to the default network.
  void OnProxyConfigChanged();
  // Returns true if there's a policy configured proxy on the default network
  // (via device or user ONC policy, user policy or force installed extension).
  bool IsManagedProxyConfigured();
  // Returns true if the `kProxy` preference set by an extension can be changed
  // by the user.
  bool IsProxyConfiguredByUserViaExtension();

  void OnSetAuthenticationDetails(
      const system_proxy::SetAuthenticationDetailsResponse& response);
  void OnShutDownProcess(const system_proxy::ShutDownResponse& response);
  void OnClearUserCredentials(
      const system_proxy::ClearUserCredentialsResponse& response);

  void OnKerberosEnabledChanged();
  void OnKerberosAccountChanged();
  void OnArcEnabledChanged();
  // Sets the value of the pref |kSystemProxyUserTrafficHostAndPort|.
  void SetUserTrafficProxyPref(const std::string& user_traffic_address);
  bool IsArcEnabled() const;

  // Sends the authentication details for |protection_space| to System-proxy via
  // D-Bus.
  void SendUserAuthenticationCredentials(
      const system_proxy::ProtectionSpace& protection_space,
      const std::string& username,
      const std::string& password);

  // Sends policy set credentials to System-proxy via D-Bus. Credentials are
  // sent only if `username` and `password` are different than
  // `last_sent_username_` and `last_sent_password_` or if `force_send` is true.
  void SendPolicyAuthenticationCredentials(const std::string& username,
                                           const std::string& password,
                                           bool force_send);

  // Send the Kerberos enabled state and active principal name to System-proxy
  // via D-Bus.
  void SendKerberosAuthenticationDetails();
  // Sends empty credentials for |protection_space| to System-proxy via D-Bus.
  // This can mean that a user is not signed into Chrome OS or they didn't
  // provide proxy authentication credentials. In this case, System-proxy will
  // forward the authentication failure (HTTP 407 status code) to the Chrome OS
  // client.
  void SendEmptyCredentials(
      const system_proxy::ProtectionSpace& protection_space);

  // Once a trusted set of policies is established, this function calls
  // the System-proxy dbus client to start/shutdown the daemon and, if
  // necessary, to configure the web proxy credentials for system services.
  void OnSystemProxySettingsPolicyChanged();

  // This function is called when the |WorkerActive| dbus signal is received.
  void OnWorkerActive(const system_proxy::WorkerActiveSignalDetails& details);

  // Requests from the NetworkService the user credentials associated with the
  // protection space specified in |details|. This function is called when the
  // |AuthenticationRequired| dbus signal is received.
  void OnAuthenticationRequired(
      const system_proxy::AuthenticationRequiredDetails& details);

  // Forwards the user credentials to System-proxy. |credentials| may be empty
  // indicating the credentials for the specified |protection_space| are not
  // available.
  void LookupProxyAuthCredentialsCallback(
      const system_proxy::ProtectionSpace& protection_space,
      const base::Optional<net::AuthCredentials>& credentials);

  void ShowAuthenticationNotification(
      const system_proxy::ProtectionSpace& protection_space,
      bool show_error);

  // Shows a dialog which prompts the user to introduce proxy authentication
  // credentials for OS level traffic. If |show_error_label| is true, the
  // dialog will show a label that indicates the previous attempt to
  // authenticate has failed due to invalid credentials.
  void ShowAuthenticationDialog(
      const system_proxy::ProtectionSpace& protection_space,
      bool show_error_label);
  void OnDialogAccepted(const system_proxy::ProtectionSpace& protection_space);
  void OnDialogCanceled(const system_proxy::ProtectionSpace& protection_space);
  void OnDialogClosed(const system_proxy::ProtectionSpace& protection_space);

  // Closes the authentication notification or dialog if shown.
  void CloseAuthenticationUI();

  chromeos::CrosSettings* cros_settings_;
  base::CallbackListSubscription system_proxy_subscription_;

  bool system_proxy_enabled_ = false;
  // The authority URI in the format host:port of the local proxy worker for
  // system services.
  std::string system_services_address_;
  std::string system_services_username_;
  std::string system_services_password_;
  // List of proxy authentication schemes for which the policy set credentials
  // can be used.
  std::vector<std::string> policy_credentials_auth_schemes_;

  // The credentials which were last sent to System-proxy. They can differ from
  // `system_services_username_` and `system_services_username_` if the proxy
  // configuration is not managed; in this case `last_sent_username_` and
  // `last_sent_password_` are both empty even if credentials were specified by
  // policy.
  std::string last_sent_username_;
  std::string last_sent_password_;

  // Local state prefs, not owned.
  PrefService* local_state_ = nullptr;

  // Notification which informs the user that System-proxy requires credentials
  // for authentication to the remote proxy.
  std::unique_ptr<chromeos::SystemProxyNotification> notification_handler_;

  // Owned by |auth_widget_|.
  chromeos::RequestSystemProxyCredentialsView* active_auth_dialog_ = nullptr;
  // Owned by the UI code (NativeWidget).
  views::Widget* auth_widget_ = nullptr;

  // Primary profile, not owned.
  Profile* primary_profile_ = nullptr;
  std::unique_ptr<extensions::PrefsUtil> extension_prefs_util_;

  // Observer for Kerberos-related prefs.
  std::unique_ptr<PrefChangeRegistrar> local_state_pref_change_registrar_;
  std::unique_ptr<PrefChangeRegistrar> profile_pref_change_registrar_;

  base::RepeatingClosure send_auth_details_closure_for_test_;

  base::WeakPtrFactory<SystemProxyManager> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_SYSTEM_PROXY_MANAGER_H_
