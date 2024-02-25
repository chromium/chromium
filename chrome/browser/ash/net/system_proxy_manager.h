// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_SYSTEM_PROXY_MANAGER_H_
#define CHROME_BROWSER_ASH_NET_SYSTEM_PROXY_MANAGER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util.h"
#include "chromeos/ash/components/dbus/system_proxy/system_proxy_service.pb.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "content/public/browser/content_browser_client.h"
#include "net/base/auth.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace content {
class LoginDelegate;
}  // namespace content

namespace system_proxy {
class SetAuthenticationDetailsResponse;
class ShutDownResponse;
}  // namespace system_proxy

namespace views {
class Widget;
}  // namespace views

class PrefRegistrySimple;
class PrefService;
class PrefChangeRegistrar;
class Profile;

namespace ash {

class NetworkStateHandler;
class RequestSystemProxyCredentialsView;
class SystemProxyNotification;

// Starts and stops the system-proxy service and handles the authentication
// requests coming from system-proxy. Authentication requests are resolved by
// requesting proxy credentials from the NetworkService or, if the
// NetworkService doesn't have credentials for the specified proxy, it will
// prompt a dialog asking the user for credentials.
// It also listens for the `WorkerActive` dbus signal sent by the System-proxy
// daemon and stores connection information regarding the active worker
// processes.
// TODO(acostinas, https://crbug.com/1145174): Move the logic that tracks
// managed network changes to another class.
class SystemProxyManager : public NetworkStateHandlerObserver {
 public:
  enum class SystemProxyState {
    // System-proxy is not enabled by feature nor policy.
    kDisabled = 0,
    // System proxy is enabled via feature flag; only available to system
    // services which explicitly opt to use system-proxy.
    kEnabledForSystemServices,
    // System proxy is enabled via policy for all system services and the
    // PlayStore.
    kEnabledForAll
  };

  explicit SystemProxyManager(PrefService* local_state);
  SystemProxyManager(const SystemProxyManager&) = delete;

  SystemProxyManager& operator=(const SystemProxyManager&) = delete;

  ~SystemProxyManager() override;

  // Called by `ChromeBrowserMainPartsAsh` in order to bootstrap the
  // SystemProxyManager instance after the required global data is
  // available (local state, and CrosSettings).
  static void Initialize(PrefService* local_state);

  // Returns the instance of the SystemProxyManager singleton.  May return
  // nullptr during browser startup and shutdown.  When calling Get(), either
  // make sure that your code executes after browser startup and before shutdown
  // or be careful to call Get() every time (instead of holding a pointer) and
  // check for nullptr to handle cases where you might access
  // SystemProxyManager during startup or shutdown.
  static SystemProxyManager* Get();

  // Called by `ChromeBrowserMainPartsAsh` in order to shutdown the
  // SystemProxyManager instance before the required global data is destroyed
  // (local state and CrosSettings).
  static void Shutdown();

  // If System-proxy is enabled, it returns the URL of the local proxy instance
  // that authenticates system services, in PAC format, e.g.
  //     PROXY localhost:3128
  // otherwise it returns an empty string.
  std::string SystemServicesProxyPacString(
      chromeos::SystemProxyOverride system_proxy_override) const;

  void StartObservingPrimaryProfilePrefs(Profile* profile);
  void StopObservingPrimaryProfilePrefs();
  // If System-proxy is enabled, it will send a request via D-Bus to clear the
  // user's proxy credentials cached by the local proxy workers. System-proxy
  // requests proxy credentials from the browser by sending an
  // |AuthenticationRequired| D-Bus signal.
  void ClearUserCredentials();

  // Enables/disables system-proxy and sets credentials to be used by ChromeOS
  // system services when connecting to a remote web proxy via system-proxy. The
  // credentials are only forwarded to system-proxy if the network proxy
  // configuration is managed via policy. `auth_schemes` allows restricting the
  // credentials to certain HTTP auth schemes.
  void SetPolicySettings(bool system_proxy_enabled,
                         const std::string& system_services_username,
                         const std::string& system_services_password,
                         const std::vector<std::string>& auth_schemes);

  void SetSystemProxyEnabledForTest(bool enabled);
  void SetSystemServicesProxyUrlForTest(const std::string& local_proxy_url);
  void SetSendAuthDetailsClosureForTest(base::RepeatingClosure closure);
  RequestSystemProxyCredentialsView* GetActiveAuthDialogForTest();
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
  void DefaultNetworkChanged(const NetworkState* network) override;
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
  // Returns true if system-proxy is enabled by policy or flag.
  bool IsEnabled() const;

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

  // Sends a shut-down command to the system-proxy daemon. Since system-proxy is
  // started via dbus activation, if the daemon is inactive, this command will
  // start the daemon and tell it to exit.
  // TODO(crbug.com/1055245,acostinas): Do not send shut-down command if
  // System-proxy is inactive.
  void SendShutDownRequest(system_proxy::TrafficOrigin traffic);

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
      const std::optional<net::AuthCredentials>& credentials);

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

  SystemProxyState system_proxy_state_ = SystemProxyState::kDisabled;

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
  raw_ptr<PrefService> local_state_ = nullptr;

  // Notification which informs the user that System-proxy requires credentials
  // for authentication to the remote proxy.
  std::unique_ptr<SystemProxyNotification> notification_handler_;

  // Owned by |auth_widget_|.
  raw_ptr<RequestSystemProxyCredentialsView> active_auth_dialog_ = nullptr;
  // Owned by the UI code (NativeWidget).
  raw_ptr<views::Widget> auth_widget_ = nullptr;

  // Primary profile, not owned.
  raw_ptr<Profile> primary_profile_ = nullptr;
  std::unique_ptr<extensions::PrefsUtil> extension_prefs_util_;

  // Observer for Kerberos-related prefs.
  std::unique_ptr<PrefChangeRegistrar> local_state_pref_change_registrar_;
  std::unique_ptr<PrefChangeRegistrar> profile_pref_change_registrar_;

  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};

  base::RepeatingClosure send_auth_details_closure_for_test_;

  base::WeakPtrFactory<SystemProxyManager> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_SYSTEM_PROXY_MANAGER_H_
