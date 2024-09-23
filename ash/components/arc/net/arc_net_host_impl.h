// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_NET_ARC_NET_HOST_IMPL_H_
#define ASH_COMPONENTS_ARC_NET_ARC_NET_HOST_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/components/arc/mojom/net.mojom.h"
#include "ash/components/arc/net/arc_app_metadata_provider.h"
#include "ash/components/arc/net/cert_manager.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/patchpanel/patchpanel_client.h"
#include "chromeos/ash/components/network/network_connection_observer.h"
#include "chromeos/ash/components/network/network_profile_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/aura/window.h"

namespace content {
class BrowserContext;
}  // namespace content

class PrefService;

namespace arc {

class ArcBridgeService;

// Private implementation of ArcNetHost.
class ArcNetHostImpl : public KeyedService,
                       public ConnectionObserver<mojom::NetInstance>,
                       public ash::NetworkConnectionObserver,
                       public ash::NetworkStateHandlerObserver,
                       public ash::PatchPanelClient::Observer,
                       public mojom::NetHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcNetHostImpl* GetForBrowserContext(content::BrowserContext* context);
  static ArcNetHostImpl* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  // The constructor will register an Observer with ArcBridgeService.
  ArcNetHostImpl(content::BrowserContext* context,
                 ArcBridgeService* arc_bridge_service);

  ArcNetHostImpl(const ArcNetHostImpl&) = delete;
  ArcNetHostImpl& operator=(const ArcNetHostImpl&) = delete;

  ~ArcNetHostImpl() override;

  void SetPrefService(PrefService* pref_service);
  void SetArcAppMetadataProvider(ArcAppMetadataProvider* app_metadata_provider);
  void SetCertManager(std::unique_ptr<CertManager> cert_manager);

  // Overridden from mojom::NetHost.

  // TODO(b/329552433): Delete get visible networks part in this method after
  // pi-arc is removed.
  // Deprecated for getting visible networks. ArcWifiHostImpl::GetScanResults()
  // should be used.
  void GetNetworks(mojom::GetNetworksRequestType type,
                   GetNetworksCallback callback) override;
  // TODO(b/329552433): Delete this method after pi-arc is removed.
  // Deprecated. ArcWifiHostImpl::GetWifiEnabledState() should be used.
  void GetWifiEnabledState(GetWifiEnabledStateCallback callback) override;
  // TODO(b/329552433): Delete this method after pi-arc is removed.
  // Deprecated. ArcWifiHostImpl::SetWifiEnabledState() should be used.
  void SetWifiEnabledState(bool is_enabled,
                           SetWifiEnabledStateCallback callback) override;
  // TODO(b/329552433): Delete this method after pi-arc is removed.
  // Deprecated. ArcWifiHostImpl::StartScan() should be used.
  void StartScan() override;
  void CreateNetwork(mojom::WifiConfigurationPtr cfg,
                     CreateNetworkCallback callback) override;
  void ForgetNetwork(const std::string& guid,
                     ForgetNetworkCallback callback) override;
  void UpdateWifiNetwork(const std::string& guid,
                         mojom::WifiConfigurationPtr cfg,
                         UpdateWifiNetworkCallback callback) override;
  void StartConnect(const std::string& guid,
                    StartConnectCallback callback) override;
  void StartDisconnect(const std::string& guid,
                       StartDisconnectCallback callback) override;
  void AndroidVpnConnected(mojom::AndroidVpnConfigurationPtr cfg) override;
  void AndroidVpnUpdated(mojom::AndroidVpnConfigurationPtr cfg) override;
  void DEPRECATED_AndroidVpnStateChanged(
      mojom::ConnectionStateType state) override;
  void AndroidVpnDisconnected() override;
  void AddPasspointCredentials(
      mojom::PasspointCredentialsPtr credentials) override;
  void RemovePasspointCredentials(
      mojom::PasspointRemovalPropertiesPtr properties) override;
  void SetAlwaysOnVpn(const std::string& vpnPackage, bool lockdown) override;
  base::Value::Dict TranslateVpnConfigurationToOnc(
      const mojom::AndroidVpnConfiguration& cfg);
  void DisconnectHostVpn() override;
  void StartLohs(mojom::LohsConfigPtr config,
                 StartLohsCallback callback) override;
  void StopLohs() override;
  void RequestPasspointAppApproval(
      mojom::PasspointApprovalRequestPtr request,
      RequestPasspointAppApprovalCallback callback) override;
  void NotifyAndroidWifiMulticastLockChange(bool is_held) override;
  void NotifySocketConnectionEvent(
      mojom::SocketConnectionEventPtr msg) override;
  void NotifyARCVPNSocketConnectionEvent(
      mojom::SocketConnectionEventPtr msg) override;

  // Overridden from ash::NetworkStateHandlerObserver.

  // TODO(b/329552433): Delete this method after pi-arc is removed.
  // Deprecated. ArcWifiHostImpl::ScanCompleted() should be used.
  void ScanCompleted(const ash::DeviceState* /*unused*/) override;
  void OnShuttingDown() override;
  void NetworkConnectionStateChanged(const ash::NetworkState* network) override;
  void NetworkListChanged() override;
  // TODO(b/329552433): Delete this method after pi-arc is removed.
  // Deprecated. ArcWifiHostImpl::DeviceListChanged() should be used.
  void DeviceListChanged() override;
  void NetworkPropertiesUpdated(const ash::NetworkState* network) override;

  // Overridden from ash::NetworkConnectionObserver.
  void DisconnectRequested(const std::string& service_path) override;

  // Overridden from ConnectionObserver<mojom::NetInstance>:
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  static void EnsureFactoryBuilt();

 private:
  const ash::NetworkState* GetDefaultNetworkFromChrome();
  void UpdateHostNetworks(
      const std::vector<patchpanel::NetworkDevice>& devices);

  // Due to a race in Chrome, GetNetworkStateFromGuid() might not know about
  // newly created networks, as that function relies on the completion of a
  // separate GetProperties shill call that completes asynchronously.  So this
  // class keeps a local cache of the path->guid mapping as a fallback.
  // This is sufficient to pass CTS but it might not handle multiple
  // successive Create operations (crbug.com/631646).
  bool GetNetworkPathFromGuid(const std::string& guid, std::string* path);

  // Get active layer 3 network connections for ARC. This function will run
  // a callback that listed current active networks for ARC.
  void GetActiveNetworks(GetNetworksCallback callback,
                         const std::vector<patchpanel::NetworkDevice>& devices);

  // Look through the list of known networks for an ARC VPN service.
  // If found, return the Shill service path.  Otherwise return
  // an empty string.  It is assumed that there is at most one ARC VPN
  // service in the list, as the same service will be reused for every
  // ARC VPN connection.
  std::string LookupArcVpnServicePath();

  // Convert a vector of strings, |string_list|, to a base::Value::List that can
  // be added to an ONC dictionary.  This is used for fields like NameServers,
  // SearchDomains, etc.
  base::Value::List TranslateStringListToValue(
      const std::vector<std::string>& string_list);

  // Convert a vector of uint64_t, |long_list|, to a base::Value::List that can
  // be passed to shill. This is because 64-bit integer values are not supported
  // for base::Value.
  // The translated values will be a list of decimal string and not a single
  // string.
  base::Value::List TranslateLongListToStringValue(
      const std::vector<uint64_t>& long_list);

  // Ask Shill to connect to the Android VPN with name |service_path|.
  // |service_path| and |guid| are stored locally for future reference.
  // This is used as the callback from a CreateConfiguration() or
  // SetProperties() call, depending on whether an ARCVPN service already
  // exists.
  void ConnectArcVpn(const std::string& service_path, const std::string& guid);

  // Ask Android to disconnect any VPN app that is currently connected.
  void DisconnectArcVpn();

  // Translate EAP credentials to base::Value::Dict and run |callback|.
  // If it is necessary to import certificates this method will asynchronously
  // import them and run |callback| afterwards.
  void TranslateEapCredentialsToDict(
      mojom::EapCredentialsPtr cred,
      base::OnceCallback<void(base::Value::Dict)> callback);

  // Synchronously translate EAP credentials to shill constants mapped
  // base::Value dictionary with with empty or imported certificate and slot
  // ID. |callback| is then run with the translated values. Could be used to
  // translate passpoint EAP credentials.
  void TranslateEapCredentialsToShillDictWithCertID(
      mojom::EapCredentialsPtr cred,
      base::OnceCallback<void(base::Value::Dict)> callback,
      const std::optional<std::string>& cert_id,
      const std::optional<int>& slot_id);

  // Synchronously translate EAP credentials to base::Value dictionary in ONC
  // with empty or imported certificate and slot ID. |callback| is then run
  // with the translated values. Could be used to translate WiFi EAP
  // credentials.
  void TranslateEapCredentialsToOncDictWithCertID(
      const mojom::EapCredentialsPtr& eap,
      base::OnceCallback<void(base::Value::Dict)> callback,
      const std::optional<std::string>& cert_id,
      const std::optional<int>& slot_id);

  // Translate EAP credentials to base::Value dictionary. If it is
  // necessary to import certificates this method will asynchronously
  // import them and run |callback| afterwards.. |is_onc| flag is used
  // to indicate whether EAP credentials will be translated directly to
  // shill properties or to ONC properties.
  void TranslateEapCredentialsToDict(
      mojom::EapCredentialsPtr cred,
      bool is_onc,
      base::OnceCallback<void(base::Value::Dict)> callback);

  // Translate passpoint credentials to base::Value::Dict and run
  // |callback|. If it is necessary to import certificates this method will
  // asynchronously import them and run |callback| afterwards.
  void TranslatePasspointCredentialsToDict(
      mojom::PasspointCredentialsPtr cred,
      base::OnceCallback<void(base::Value::Dict)> callback);

  // Synchronously translate passpoint credentials to base::Value::Dict
  // with EAP fields translated inside |dict|. |callback| is then run with
  // the translated values.
  void TranslatePasspointCredentialsToDictWithEapTranslated(
      mojom::PasspointCredentialsPtr cred,
      base::OnceCallback<void(base::Value::Dict)> callback,
      base::Value::Dict dict);

  base::Value::Dict TranslateProxyConfiguration(
      const mojom::ArcProxyInfoPtr& http_proxy);

  // Synchronously calls Chrome OS to add passpoint credentials from ARC with
  // the properties values translated taken from mojo.
  void AddPasspointCredentialsWithProperties(base::Value::Dict properties);

  // Get the app window with |package_name|. This is necessary to start the
  // user approval Passpoint dialog above the app. The app window is fetched by
  // doing BFS over the device's root windows and its children.
  aura::Window* GetAppWindow(const std::string& package_name);

  // Pass any Chrome flags into ARC. This function may be empty depending on the
  // current state of flags, i.e. if all Chrome->ARC flags have been launched
  // and cleaned up, this method may not do anything. But we keep this around to
  // keep the mojo file stable and decrease churn.
  void SetUpFlags();

  void CreateNetworkSuccessCallback(
      base::OnceCallback<void(const std::string&)> callback,
      const std::string& service_path,
      const std::string& guid);

  void CreateNetworkFailureCallback(
      base::OnceCallback<void(const std::string&)> callback,
      const std::string& error_name);

  // Callback for ash::NetworkHandler::GetShillProperties
  void ReceiveShillProperties(
      const std::string& service_path,
      std::optional<base::Value::Dict> shill_properties);

  // PatchPanelClient::Observer implementation:
  void NetworkConfigurationChanged() override;

  // Synchronously translate WiFi Configuration to shill configuration
  // and create network in shill.
  void CreateNetworkWithEapTranslated(mojom::WifiConfigurationPtr cfg,
                                      CreateNetworkCallback callback,
                                      base::Value::Dict eap_dict);

  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.

  // True if the chrome::NetworkStateHandler is currently being observed for
  // state changes.
  bool observing_network_state_ = false;
  // Cached shill properties for all active networks, keyed by Service path.
  std::map<std::string, base::Value::Dict> shill_network_properties_;

  std::string cached_service_path_;
  std::string cached_guid_;
  std::string arc_vpn_service_path_;
  // Owned by the user profile whose context was used to initialize |this|.
  raw_ptr<PrefService> pref_service_ = nullptr;
  raw_ptr<ArcAppMetadataProvider, DanglingUntriaged> app_metadata_provider_ =
      nullptr;

  std::unique_ptr<CertManager> cert_manager_;

  // Cached NetworkConfigurations that were last sent to ARC. This is an
  // already-filtered list of networks, e.g. non-ARC networks aren't included
  // since we don't send them to ARC.
  std::vector<arc::mojom::NetworkConfigurationPtr> cached_arc_networks_;

  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<ArcNetHostImpl> weak_factory_{this};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_NET_ARC_NET_HOST_IMPL_H_
