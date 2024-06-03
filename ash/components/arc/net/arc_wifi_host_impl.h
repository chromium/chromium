// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_NET_ARC_WIFI_HOST_IMPL_H_
#define ASH_COMPONENTS_ARC_NET_ARC_WIFI_HOST_IMPL_H_

#include "ash/components/arc/mojom/arc_wifi.mojom.h"
#include "ash/components/arc/mojom/net.mojom.h"
#include "ash/components/arc/net/arc_net_host_impl.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/files/scoped_file.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;
class CertManager;

// Private implementation of ArcWifiHost.
class ArcWifiHostImpl : public KeyedService,
                        public ConnectionObserver<mojom::ArcWifiInstance>,
                        public mojom::ArcWifiHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcWifiHostImpl* GetForBrowserContext(
      content::BrowserContext* context);
  static ArcWifiHostImpl* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  // The constructor will register an Observer with ArcBridgeService.
  ArcWifiHostImpl(content::BrowserContext* context,
                  ArcBridgeService* arc_bridge_service);

  ArcWifiHostImpl(const ArcWifiHostImpl&) = delete;
  ArcWifiHostImpl& operator=(const ArcWifiHostImpl&) = delete;

  ~ArcWifiHostImpl() override;

  void SetCertManager(std::unique_ptr<CertManager> cert_manager);

  // Overridden from mojom::ArcWifiHost.
  void GetWifiEnabledState(GetWifiEnabledStateCallback callback) override;
  void SetWifiEnabledState(bool is_enabled,
                           SetWifiEnabledStateCallback callback) override;
  void StartScan() override;
  void GetScanResults(GetScanResultsCallback callback) override;
  void CreateNetwork(mojom::WifiConfigurationPtr cfg,
                     CreateNetworkCallback callback) override;
  void ForgetNetwork(const std::string& guid,
                     ForgetNetworkCallback callback) override;
  void UpdateWifiNetwork(const std::string& guid,
                         mojom::WifiConfigurationPtr cfg,
                         UpdateWifiNetworkCallback callback) override;
  void GetConfiguredWifiServices(
      GetConfiguredWifiServicesCallback callback) override;

  static void EnsureFactoryBuilt();

 private:
  // Synchronously translate WiFi Configuration to shill configuration
  // and create network in shill.
  void CreateNetworkWithEapTranslated(
      mojom::WifiConfigurationPtr cfg,
      ArcNetHostImpl::CreateNetworkCallback callback,
      base::Value::Dict eap_dict);

  // Translate EAP credentials to base::Value::Dict and run |callback|.
  // If it is necessary to import certificates this method will asynchronously
  // import them and run |callback| afterwards.
  void TranslateEapCredentialsToDict(
      mojom::EapCredentialsPtr cred,
      base::OnceCallback<void(base::Value::Dict)> callback);

  // Translate EAP credentials to base::Value dictionary. If it is
  // necessary to import certificates this method will asynchronously
  // import them and run |callback| afterwards.. |is_onc| flag is used
  // to indicate whether EAP credentials will be translated directly to
  // shill properties or to ONC properties.
  void TranslateEapCredentialsToDict(
      mojom::EapCredentialsPtr cred,
      bool is_onc,
      base::OnceCallback<void(base::Value::Dict)> callback);

  // Synchronously translate EAP credentials to base::Value dictionary in ONC
  // with empty or imported certificate and slot ID. |callback| is then run
  // with the translated values. Could be used to translate WiFi EAP
  // credentials.
  void TranslateEapCredentialsToOncDictWithCertID(
      const mojom::EapCredentialsPtr& eap,
      base::OnceCallback<void(base::Value::Dict)> callback,
      const std::optional<std::string>& cert_id,
      const std::optional<int>& slot_id);

  // Synchronously translate EAP credentials to shill constants mapped
  // base::Value dictionary with with empty or imported certificate and slot
  // ID. |callback| is then run with the translated values. Could be used to
  // translate passpoint EAP credentials.
  void TranslateEapCredentialsToShillDictWithCertID(
      const mojom::EapCredentialsPtr& cred,
      base::OnceCallback<void(base::Value::Dict)> callback,
      const std::optional<std::string>& cert_id,
      const std::optional<int>& slot_id);

  void CreateNetworkSuccessCallback(
      base::OnceCallback<void(const std::string&)> callback,
      const std::string& service_path,
      const std::string& guid);

  void CreateNetworkFailureCallback(
      base::OnceCallback<void(const std::string&)> callback,
      const std::string& error_name);

  // Due to a race in Chrome, GetNetworkStateFromGuid() might not know about
  // newly created networks, as that function relies on the completion of a
  // separate GetProperties shill call that completes asynchronously.  So this
  // class keeps a local cache of the path->guid mapping as a fallback.
  // This is sufficient to pass CTS but it might not handle multiple
  // successive Create operations (crbug.com/631646).
  std::optional<std::string_view> GetNetworkPathFromGuid(
      const std::string& guid);

  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.

  // This stores a local cache of a path->guid mapping as a fallback when unable
  // to get newly create networks due to race condition. This is required for
  // passing CTS. See comment of GetNetworkStateFromGuid() for more details.
  std::string cached_guid_;
  std::string cached_service_path_;

  std::unique_ptr<CertManager> cert_manager_;

  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<ArcWifiHostImpl> weak_factory_{this};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_NET_ARC_WIFI_HOST_IMPL_H_
