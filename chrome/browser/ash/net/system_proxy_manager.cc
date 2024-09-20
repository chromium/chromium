// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/system_proxy_manager.h"

#include <string>

#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chrome/browser/ash/notifications/request_system_proxy_credentials_view.h"
#include "chrome/browser/ash/notifications/system_proxy_notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/system_proxy/system_proxy_client.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/proxy/proxy_config_service_impl.h"
#include "chromeos/ash/components/network/proxy/ui_proxy_config_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/host_port_pair.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/http/http_auth_scheme.h"
#include "net/http/http_util.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "ui/aura/window.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash {

namespace {

const char kSystemProxyService[] = "system-proxy-service";

// A `content::LoginDelegate` implementation that returns to the caller the
// proxy credentials set by the policy `SystemProxySettings`.
class SystemProxyLoginHandler : public content::LoginDelegate {
 public:
  SystemProxyLoginHandler() = default;
  ~SystemProxyLoginHandler() override = default;

  SystemProxyLoginHandler(const SystemProxyLoginHandler&) = delete;
  SystemProxyLoginHandler& operator=(const SystemProxyLoginHandler&) = delete;

  void AuthenticateWithCredentials(
      const std::string& username,
      const std::string& password,
      LoginAuthRequiredCallback auth_required_callback) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&SystemProxyLoginHandler::InvokeWithCredentials,
                       weak_factory_.GetWeakPtr(), username, password,
                       std::move(auth_required_callback)));
  }

 private:
  void InvokeWithCredentials(const std::string& username,
                             const std::string& password,
                             LoginAuthRequiredCallback auth_required_callback) {
    std::move(auth_required_callback)
        .Run(std::make_optional<net::AuthCredentials>(
            base::UTF8ToUTF16(username), base::UTF8ToUTF16(password)));
  }

  base::WeakPtrFactory<SystemProxyLoginHandler> weak_factory_{this};
};

// If system-proxy is enabled via policy, it can be used by both Chrome OS
// system services and the PlayStore. If enabled via flag, system-proxy can only
// be used by system services which explicitly ask to use system-proxy for HTTP
// proxy authentication. Otherwise, system-proxy is disabled.
SystemProxyManager::SystemProxyState DetermineSystemProxyState(
    bool policy_enabled) {
  if (policy_enabled)
    return SystemProxyManager::SystemProxyState::kEnabledForAll;

  if (base::FeatureList::IsEnabled(features::kSystemProxyForSystemServices)) {
    return SystemProxyManager::SystemProxyState::kEnabledForSystemServices;
  }
  return SystemProxyManager::SystemProxyState::kDisabled;
}

SystemProxyManager* g_system_proxy_manager_ = nullptr;

}  // namespace

SystemProxyManager::SystemProxyManager(PrefService* local_state) {
  // Connect to System-proxy signals.
  SystemProxyClient::Get()->SetWorkerActiveSignalCallback(base::BindRepeating(
      &SystemProxyManager::OnWorkerActive, weak_factory_.GetWeakPtr()));
  SystemProxyClient::Get()->SetAuthenticationRequiredSignalCallback(
      base::BindRepeating(&SystemProxyManager::OnAuthenticationRequired,
                          weak_factory_.GetWeakPtr()));
  SystemProxyClient::Get()->ConnectToWorkerSignals();
  local_state_ = local_state;

  // Listen to pref changes.
  local_state_pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  local_state_pref_change_registrar_->Init(local_state_);
  local_state_pref_change_registrar_->Add(
      prefs::kKerberosEnabled,
      base::BindRepeating(&SystemProxyManager::OnKerberosEnabledChanged,
                          weak_factory_.GetWeakPtr()));
  DCHECK(NetworkHandler::IsInitialized());
  network_state_handler_observer_.Observe(
      NetworkHandler::Get()->network_state_handler());

  system_proxy_state_ = DetermineSystemProxyState(/*policy_enabled=*/false);

  // Start the system-proxy worker that authenticates system services.
  if (system_proxy_state_ == SystemProxyState::kEnabledForSystemServices) {
    SendPolicyAuthenticationCredentials(/*username=*/"",
                                        /*password=*/"",
                                        /*force_send=*/true);
  }
}

SystemProxyManager::~SystemProxyManager() {
  if (IsEnabled()) {
    SendShutDownRequest(system_proxy::TrafficOrigin::ALL);
  }
  DCHECK(NetworkHandler::IsInitialized());
}

// static
void SystemProxyManager::Initialize(PrefService* local_state) {
  g_system_proxy_manager_ = new SystemProxyManager(local_state);
}

// static
SystemProxyManager* SystemProxyManager::Get() {
  return g_system_proxy_manager_;
}

// static
void SystemProxyManager::Shutdown() {
  if (g_system_proxy_manager_) {
    delete g_system_proxy_manager_;
    g_system_proxy_manager_ = nullptr;
  }
}

std::string SystemProxyManager::SystemServicesProxyPacString(
    chromeos::SystemProxyOverride system_proxy_override) const {
  if (system_proxy_override == chromeos::SystemProxyOverride::kOptOut ||
      system_services_address_.empty()) {
    return std::string();
  }

  if (system_proxy_state_ == SystemProxyState::kEnabledForAll ||
      (system_proxy_state_ == SystemProxyState::kEnabledForSystemServices &&
       system_proxy_override == chromeos::SystemProxyOverride::kOptIn)) {
    return "PROXY " + system_services_address_;
  }

  return std::string();
}

void SystemProxyManager::StartObservingPrimaryProfilePrefs(Profile* profile) {
  primary_profile_ = profile;
  extension_prefs_util_ = std::make_unique<extensions::PrefsUtil>(profile);
  // Listen to pref changes.
  profile_pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  profile_pref_change_registrar_->Init(primary_profile_->GetPrefs());
  profile_pref_change_registrar_->Add(
      prefs::kKerberosActivePrincipalName,
      base::BindRepeating(&SystemProxyManager::OnKerberosAccountChanged,
                          base::Unretained(this)));
  profile_pref_change_registrar_->Add(
      arc::prefs::kArcEnabled,
      base::BindRepeating(&SystemProxyManager::OnArcEnabledChanged,
                          weak_factory_.GetWeakPtr()));
  profile_pref_change_registrar_->Add(
      proxy_config::prefs::kProxy,
      base::BindRepeating(&SystemProxyManager::OnProxyConfigChanged,
                          base::Unretained(this)));
  if (IsEnabled()) {
    OnProxyConfigChanged();
    OnKerberosAccountChanged();
  }

  if (system_proxy_state_ == SystemProxyState::kEnabledForAll) {
    OnArcEnabledChanged();
  }
}

void SystemProxyManager::StopObservingPrimaryProfilePrefs() {
  profile_pref_change_registrar_->RemoveAll();
  profile_pref_change_registrar_.reset();
  extension_prefs_util_.reset();
  primary_profile_ = nullptr;
}

void SystemProxyManager::ClearUserCredentials() {
  if (!IsEnabled()) {
    return;
  }

  system_proxy::ClearUserCredentialsRequest request;
  SystemProxyClient::Get()->ClearUserCredentials(
      request, base::BindOnce(&SystemProxyManager::OnClearUserCredentials,
                              weak_factory_.GetWeakPtr()));
}

void SystemProxyManager::SetPolicySettings(
    bool system_proxy_enabled,
    const std::string& system_services_username,
    const std::string& system_services_password,
    const std::vector<std::string>& auth_schemes) {
  system_services_username_ = system_services_username;
  system_services_password_ = system_services_password;
  policy_credentials_auth_schemes_ = auth_schemes;

  if (system_proxy_state_ == SystemProxyState::kDisabled &&
      !system_proxy_enabled) {
    return;  // nothing to do
  }

  system_proxy_state_ = DetermineSystemProxyState(system_proxy_enabled);

  if (system_proxy_state_ == SystemProxyState::kDisabled) {
    system_services_address_.clear();
    SetUserTrafficProxyPref(std::string());
    CloseAuthenticationUI();
    SendShutDownRequest(system_proxy::TrafficOrigin::ALL);
    return;
  }

  if (system_proxy_state_ == SystemProxyState::kEnabledForSystemServices) {
    // Start the system-proxy worker for system services and make sure the
    // system-proxy worker for ARC is shut down.
    SendPolicyAuthenticationCredentials(/*username=*/"",
                                        /*password=*/"",
                                        /*force_send=*/true);
    SetUserTrafficProxyPref(std::string());
    SendShutDownRequest(system_proxy::TrafficOrigin::USER);
  }

  if (IsManagedProxyConfigured() &&
      system_proxy_state_ == SystemProxyState::kEnabledForAll) {
    // Force send the configuration in case the credentials hand't changed, but
    // `policy_credentials_auth_schemes_` has.
    SendPolicyAuthenticationCredentials(system_services_username_,
                                        system_services_password_,
                                        /*force_send=*/true);
  } else {
    // To avoid leaking the policy set credentials, don't send them to
    // System-proxy if there's no managed proxy on the network.
    // Note: When SystemProxyManager is starting, the credentials are empty and
    // they were never sent before. We need to force send them, otherwise
    // `SendPolicyAuthenticationCredentials` will detect that no change to the
    // credentials occurred and will not trigger a D-Bus request. This means the
    // worker service for Chrome OS system services will not be started.
    SendPolicyAuthenticationCredentials(/*username=*/"",
                                        /*password=*/"",
                                        /*force_send=*/true);
  }

  // Fire once to cover the case where the SystemProxySetting policy is set
  // during a user session.
  if (IsArcEnabled() &&
      system_proxy_state_ == SystemProxyState::kEnabledForAll) {
    OnArcEnabledChanged();
  }
}

void SystemProxyManager::OnKerberosEnabledChanged() {
  SendKerberosAuthenticationDetails();
}

void SystemProxyManager::OnKerberosAccountChanged() {
  if (!local_state_->GetBoolean(prefs::kKerberosEnabled)) {
    return;
  }
  SendKerberosAuthenticationDetails();
}

void SystemProxyManager::OnArcEnabledChanged() {
  if (system_proxy_state_ != SystemProxyState::kEnabledForAll) {
    return;
  }

  if (!IsArcEnabled()) {
    system_proxy::ShutDownRequest request;
    request.set_traffic_type(system_proxy::TrafficOrigin::USER);
    SystemProxyClient::Get()->ShutDownProcess(
        request, base::BindOnce(&SystemProxyManager::OnShutDownProcess,
                                weak_factory_.GetWeakPtr()));
    return;
  }

  if (local_state_->GetBoolean(prefs::kKerberosEnabled)) {
    SendKerberosAuthenticationDetails();
    return;
  }

  system_proxy::SetAuthenticationDetailsRequest request;
  request.set_traffic_type(system_proxy::TrafficOrigin::USER);
  SystemProxyClient::Get()->SetAuthenticationDetails(
      request, base::BindOnce(&SystemProxyManager::OnSetAuthenticationDetails,
                              weak_factory_.GetWeakPtr()));
}

bool SystemProxyManager::IsArcEnabled() const {
  return primary_profile_ &&
         primary_profile_->GetPrefs()->GetBoolean(arc::prefs::kArcEnabled);
}

bool SystemProxyManager::IsEnabled() const {
  return system_proxy_state_ != SystemProxyState::kDisabled;
}

void SystemProxyManager::SendUserAuthenticationCredentials(
    const system_proxy::ProtectionSpace& protection_space,
    const std::string& username,
    const std::string& password) {
  // System-proxy is started via d-bus activation, meaning the first d-bus call
  // will start the daemon. Check that System-proxy was not disabled by policy
  // while looking for credentials so we don't accidentally restart it.
  if (!IsEnabled()) {
    return;
  }

  system_proxy::Credentials user_credentials;
  user_credentials.set_username(username);
  user_credentials.set_password(password);

  system_proxy::SetAuthenticationDetailsRequest request;
  request.set_traffic_type(
      IsArcEnabled() && system_proxy_state_ == SystemProxyState::kEnabledForAll
          ? system_proxy::TrafficOrigin::ALL
          : system_proxy::TrafficOrigin::SYSTEM);
  *request.mutable_credentials() = user_credentials;
  *request.mutable_protection_space() = protection_space;

  SystemProxyClient::Get()->SetAuthenticationDetails(
      request, base::BindOnce(&SystemProxyManager::OnSetAuthenticationDetails,
                              weak_factory_.GetWeakPtr()));
}

void SystemProxyManager::SendPolicyAuthenticationCredentials(
    const std::string& username,
    const std::string& password,
    bool force_send) {
  if (!IsEnabled())
    return;

  if (!force_send &&
      (last_sent_username_ == username && last_sent_password_ == password)) {
    // Credentials were already sent.
    return;
  }

  last_sent_username_ = username;
  last_sent_password_ = password;

  system_proxy::SetAuthenticationDetailsRequest request;
  system_proxy::Credentials credentials;
  credentials.set_username(username);
  credentials.set_password(password);
  for (const auto& auth_scheme : policy_credentials_auth_schemes_) {
    credentials.add_policy_credentials_auth_schemes(auth_scheme);
  }
  *request.mutable_credentials() = credentials;

  request.set_traffic_type(system_proxy::TrafficOrigin::SYSTEM);

  SystemProxyClient::Get()->SetAuthenticationDetails(
      request, base::BindOnce(&SystemProxyManager::OnSetAuthenticationDetails,
                              weak_factory_.GetWeakPtr()));
}

void SystemProxyManager::SendKerberosAuthenticationDetails() {
  if (!IsEnabled()) {
    return;
  }

  system_proxy::SetAuthenticationDetailsRequest request;
  request.set_traffic_type(
      IsArcEnabled() && system_proxy_state_ == SystemProxyState::kEnabledForAll
          ? system_proxy::TrafficOrigin::ALL
          : system_proxy::TrafficOrigin::SYSTEM);
  request.set_kerberos_enabled(
      local_state_->GetBoolean(prefs::kKerberosEnabled));
  if (primary_profile_) {
    request.set_active_principal_name(
        primary_profile_->GetPrefs()
            ->GetValue(prefs::kKerberosActivePrincipalName)
            // TODO (https://crbug.com/1344857) Maybe call GetString directly.
            .GetString());
  }
  SystemProxyClient::Get()->SetAuthenticationDetails(
      request, base::BindOnce(&SystemProxyManager::OnSetAuthenticationDetails,
                              weak_factory_.GetWeakPtr()));
}

void SystemProxyManager::SendEmptyCredentials(
    const system_proxy::ProtectionSpace& protection_space) {
  SendUserAuthenticationCredentials(protection_space,
                                    /*username=*/std::string(),
                                    /*password=*/std::string());
}

void SystemProxyManager::SendShutDownRequest(
    system_proxy::TrafficOrigin traffic) {
  system_proxy::ShutDownRequest request;
  request.set_traffic_type(traffic);
  SystemProxyClient::Get()->ShutDownProcess(
      request, base::BindOnce(&SystemProxyManager::OnShutDownProcess,
                              weak_factory_.GetWeakPtr()));
}

void SystemProxyManager::SetSystemProxyEnabledForTest(bool enabled) {
  system_proxy_state_ =
      enabled ? SystemProxyState::kEnabledForAll : SystemProxyState::kDisabled;
}

void SystemProxyManager::SetSystemServicesProxyUrlForTest(
    const std::string& local_proxy_url) {
  system_services_address_ = local_proxy_url;
}

void SystemProxyManager::SetSendAuthDetailsClosureForTest(
    base::RepeatingClosure closure) {
  send_auth_details_closure_for_test_ = closure;
}

RequestSystemProxyCredentialsView*
SystemProxyManager::GetActiveAuthDialogForTest() {
  return active_auth_dialog_;
}

void SystemProxyManager::CloseAuthDialogForTest() {
  DCHECK(auth_widget_);
  auth_widget_->CloseNow();
}

// static
void SystemProxyManager::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kSystemProxyUserTrafficHostAndPort,
                               /*default_value=*/std::string());
}

bool SystemProxyManager::CanUsePolicyCredentials(
    const net::AuthChallengeInfo& auth_info,
    bool first_auth_attempt) {
  if (!auth_info.is_proxy || !first_auth_attempt) {
    return false;
  }
  if (!LoginState::IsInitialized() ||
      (!LoginState::Get()->IsManagedGuestSessionUser() &&
       !LoginState::Get()->IsKioskSession())) {
    VLOG(1) << "Only kiosk app and MGS can reuse the policy provided proxy "
               "credentials for authentication";
    return false;
  }

  if (system_proxy_state_ != SystemProxyState::kEnabledForAll ||
      system_services_username_.empty() || system_services_password_.empty()) {
    return false;
  }

  if (!IsManagedProxyConfigured())
    return false;

  if (!policy_credentials_auth_schemes_.empty()) {
    if (!base::Contains(policy_credentials_auth_schemes_, auth_info.scheme)) {
      VLOG(1) << "Auth scheme not allowed by policy";
      return false;
    }
  }
  return true;
}

std::unique_ptr<content::LoginDelegate> SystemProxyManager::CreateLoginDelegate(
    LoginAuthRequiredCallback auth_required_callback) {
  auto login_delegate = std::make_unique<SystemProxyLoginHandler>();
  login_delegate->AuthenticateWithCredentials(
      system_services_username_, system_services_password_,
      std::move(auth_required_callback));
  return std::move(login_delegate);
}

void SystemProxyManager::OnSetAuthenticationDetails(
    const system_proxy::SetAuthenticationDetailsResponse& response) {
  if (response.has_error_message()) {
    NET_LOG(ERROR)
        << "Failed to set system traffic credentials for system proxy: "
        << kSystemProxyService << ", Error: " << response.error_message();
  }
  if (send_auth_details_closure_for_test_)
    send_auth_details_closure_for_test_.Run();
}

// This function is called when the default network changes or when any of its
// properties change.
void SystemProxyManager::DefaultNetworkChanged(const NetworkState* network) {
  if (!network)
    return;
  OnProxyConfigChanged();
}

void SystemProxyManager::OnProxyConfigChanged() {
  if (!IsManagedProxyConfigured()) {
    SendPolicyAuthenticationCredentials(/*username=*/"", /*password=*/"",
                                        /*force_send=*/false);
    return;
  }
  SendPolicyAuthenticationCredentials(system_services_username_,
                                      system_services_password_,
                                      /*force_send=*/false);
}

bool SystemProxyManager::IsManagedProxyConfigured() {
  DCHECK(NetworkHandler::IsInitialized());
  NetworkHandler* network_handler = NetworkHandler::Get();
  base::Value::Dict proxy_settings;

  // |ui_proxy_config_service| may be missing in tests. If the device is offline
  // (no network connected) the |DefaultNetwork| is null.
  if (NetworkHandler::HasUiProxyConfigService() &&
      network_handler->network_state_handler()->DefaultNetwork()) {
    // Check if proxy is enforced by user policy, force installed extension or
    // ONC policies. This will only read managed settings.
    NetworkHandler::GetUiProxyConfigService()->MergeEnforcedProxyConfig(
        network_handler->network_state_handler()->DefaultNetwork()->guid(),
        &proxy_settings);
  }
  if (proxy_settings.empty())
    return false;  // no managed proxy set

  if (IsProxyConfiguredByUserViaExtension())
    return false;
  // Proxy was configured by the admin
  return true;
}

bool SystemProxyManager::IsProxyConfiguredByUserViaExtension() {
  if (!extension_prefs_util_)
    return false;

  std::optional<extensions::api::settings_private::PrefObject> pref =
      extension_prefs_util_->GetPref(proxy_config::prefs::kProxy);
  return pref && pref->extension_can_be_disabled &&
         *pref->extension_can_be_disabled;
}

void SystemProxyManager::OnShutDownProcess(
    const system_proxy::ShutDownResponse& response) {
  if (response.has_error_message() && !response.error_message().empty()) {
    NET_LOG(ERROR) << "Failed to shutdown system proxy process: "
                   << kSystemProxyService
                   << ", error: " << response.error_message();
  }
}

void SystemProxyManager::OnClearUserCredentials(
    const system_proxy::ClearUserCredentialsResponse& response) {
  if (response.has_error_message() && !response.error_message().empty()) {
    NET_LOG(ERROR) << "Failed to clear user credentials: "
                   << kSystemProxyService
                   << ", error: " << response.error_message();
  }
}

void SystemProxyManager::OnWorkerActive(
    const system_proxy::WorkerActiveSignalDetails& details) {
  if (details.traffic_origin() == system_proxy::TrafficOrigin::SYSTEM) {
    system_services_address_ = details.local_proxy_url();
    return;
  }
  if (system_proxy_state_ != SystemProxyState::kEnabledForAll)
    return;

  SetUserTrafficProxyPref(details.local_proxy_url());
}

void SystemProxyManager::SetUserTrafficProxyPref(
    const std::string& user_traffic_address) {
  if (!primary_profile_) {
    return;
  }
  primary_profile_->GetPrefs()->SetString(
      prefs::kSystemProxyUserTrafficHostAndPort, user_traffic_address);
}

void SystemProxyManager::OnAuthenticationRequired(
    const system_proxy::AuthenticationRequiredDetails& details) {
  system_proxy::ProtectionSpace protection_space =
      details.proxy_protection_space();

  if (!primary_profile_) {
    SendEmptyCredentials(protection_space);
    return;
  }

  // The previous authentication attempt failed.
  if (details.has_bad_cached_credentials() &&
      details.bad_cached_credentials()) {
    ShowAuthenticationNotification(protection_space,
                                   details.bad_cached_credentials());
    return;
  }

  // TODO(acostinas,chromium:1104818) |protection_space.origin()| is in a
  // URI-like format which may be incompatible between Chrome and libcurl, which
  // is used on the Chrome OS side. We should change |origin()| to be a PAC
  // string (a more "standard" way of representing proxies) and call
  // |FromPacString()| to create |proxy_server|.
  net::ProxyServer proxy_server = net::ProxyUriToProxyServer(
      protection_space.origin(), net::ProxyServer::Scheme::SCHEME_HTTP);

  if (!proxy_server.is_valid()) {
    SendEmptyCredentials(protection_space);
    return;
  }
  primary_profile_->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->LookupProxyAuthCredentials(
          proxy_server, protection_space.scheme(),
          net::HttpUtil::Unquote(protection_space.realm()),
          base::BindOnce(
              &SystemProxyManager::LookupProxyAuthCredentialsCallback,
              weak_factory_.GetWeakPtr(), protection_space));
}

void SystemProxyManager::LookupProxyAuthCredentialsCallback(
    const system_proxy::ProtectionSpace& protection_space,
    const std::optional<net::AuthCredentials>& credentials) {
  if (!credentials) {
    // Ask the user for credentials
    ShowAuthenticationNotification(protection_space, /*show_error=*/false);
    return;
  }

  std::string username;
  std::string password;
  if (credentials) {
    username = base::UTF16ToUTF8(credentials->username());
    password = base::UTF16ToUTF8(credentials->password());

    // If there's a dialog requesting credentials for this proxy, close it.
    if (notification_handler_ ||
        (active_auth_dialog_ &&
         active_auth_dialog_->GetProxyServer() == protection_space.origin())) {
      CloseAuthenticationUI();
    }
  }
  SendUserAuthenticationCredentials(protection_space, username, password);
}

void SystemProxyManager::ShowAuthenticationNotification(
    const system_proxy::ProtectionSpace& protection_space,
    bool show_error) {
  if (active_auth_dialog_)
    return;
  notification_handler_ = std::make_unique<SystemProxyNotification>(
      protection_space, show_error,
      base::BindOnce(&SystemProxyManager::ShowAuthenticationDialog,
                     weak_factory_.GetWeakPtr()));
  notification_handler_->Show();
}

void SystemProxyManager::ShowAuthenticationDialog(
    const system_proxy::ProtectionSpace& protection_space,
    bool show_error_label) {
  if (active_auth_dialog_)
    return;

  if (notification_handler_)
    notification_handler_->Close();

  active_auth_dialog_ = new RequestSystemProxyCredentialsView(
      protection_space.origin(), show_error_label,
      base::BindOnce(&SystemProxyManager::OnDialogClosed,
                     weak_factory_.GetWeakPtr(), protection_space));

  active_auth_dialog_->SetAcceptCallback(
      base::BindRepeating(&SystemProxyManager::OnDialogAccepted,
                          weak_factory_.GetWeakPtr(), protection_space));
  active_auth_dialog_->SetCancelCallback(
      base::BindRepeating(&SystemProxyManager::OnDialogCanceled,
                          weak_factory_.GetWeakPtr(), protection_space));

  auth_widget_ = views::DialogDelegate::CreateDialogWidget(
      active_auth_dialog_, /*context=*/nullptr, /*parent=*/nullptr);
  auth_widget_->Show();
}

void SystemProxyManager::OnDialogAccepted(
    const system_proxy::ProtectionSpace& protection_space) {
  SendUserAuthenticationCredentials(
      protection_space, base::UTF16ToUTF8(active_auth_dialog_->GetUsername()),
      base::UTF16ToUTF8(active_auth_dialog_->GetPassword()));
}

void SystemProxyManager::OnDialogCanceled(
    const system_proxy::ProtectionSpace& protection_space) {
  SendEmptyCredentials(protection_space);
}

void SystemProxyManager::OnDialogClosed(
    const system_proxy::ProtectionSpace& protection_space) {
  active_auth_dialog_ = nullptr;
  auth_widget_ = nullptr;
}

void SystemProxyManager::CloseAuthenticationUI() {
  // Closes the notification if shown.
  if (notification_handler_) {
    notification_handler_->Close();
    notification_handler_.reset();
  }
  if (!auth_widget_)
    return;
  // Also deletes the |auth_widget_| instance.
  auth_widget_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
}

}  // namespace ash
