// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SYSTEM_NETWORK_CONTEXT_MANAGER_H_
#define CHROME_BROWSER_NET_SYSTEM_NETWORK_CONTEXT_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "chrome/browser/net/proxy_config_monitor.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/host_resolver.mojom-forward.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom-forward.h"
#include "services/network/public/mojom/ssl_config.mojom-forward.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"

class PrefRegistrySimple;
class PrefService;
class SSLConfigServiceManager;

namespace network {
namespace mojom {
class URLLoaderFactory;
}
class SharedURLLoaderFactory;
}  // namespace network

namespace net_log {
class NetExportFileWriter;
}

// Responsible for creating and managing access to the system NetworkContext.
// Lives on the UI thread. The NetworkContext this owns is intended for requests
// not associated with a profile. It stores no data on disk, and has no HTTP
// cache, but it does have ephemeral cookie and channel ID stores. It also does
// not have access to HTTP proxy auth information the user has entered or that
// comes from extensions, and similarly, has no extension-provided per-profile
// proxy configuration information.
//
// This class is also responsible for configuring global NetworkService state.
//
// The "system" NetworkContext will either share a URLRequestContext with
// IOThread's SystemURLRequestContext and be part of IOThread's NetworkService
// (If the network service is disabled) or be an independent NetworkContext
// using the actual network service.
//
// This class is intended to eventually replace IOThread. Handling the two cases
// differently allows this to be used in production without breaking anything or
// requiring two separate paths, while IOThread consumers slowly transition over
// to being compatible with the network service.
class SystemNetworkContextManager {
 public:
  ~SystemNetworkContextManager();

  // Creates the global instance of SystemNetworkContextManager. If an
  // instance already exists, this will cause a DCHECK failure.
  static SystemNetworkContextManager* CreateInstance(PrefService* pref_service);

  // Checks if the global SystemNetworkContextManager has been created.
  static bool HasInstance();

  // Gets the global SystemNetworkContextManager instance. If it has not been
  // created yet, NetworkService is called, which will cause the
  // SystemNetworkContextManager to be created.
  static SystemNetworkContextManager* GetInstance();

  // Destroys the global SystemNetworkContextManager instance.
  static void DeleteInstance();

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Returns the System NetworkContext. May only be called after SetUp(). Does
  // any initialization of the NetworkService that may be needed when first
  // called.
  network::mojom::NetworkContext* GetContext();

  // Returns a URLLoaderFactory owned by the SystemNetworkContextManager that is
  // backed by the SystemNetworkContext. Allows sharing of the URLLoaderFactory.
  // Prefer this to creating a new one.  Call Clone() on the value returned by
  // this method to get a URLLoaderFactory that can be used on other threads.
  network::mojom::URLLoaderFactory* GetURLLoaderFactory();

  // Returns a SharedURLLoaderFactory owned by the SystemNetworkContextManager
  // that is backed by the SystemNetworkContext.
  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory();

  // Called when content creates a NetworkService. Creates the
  // SystemNetworkContext, if the network service is enabled.
  void OnNetworkServiceCreated(network::mojom::NetworkService* network_service);

  // Permanently disables QUIC, both for NetworkContexts using the IOThread's
  // NetworkService, and for those using the network service (if enabled).
  void DisableQuic();

  // Returns an mojo::PendingReceiver<SSLConfigClient> that can be passed as a
  // NetorkContextParam.
  mojo::PendingReceiver<network::mojom::SSLConfigClient>
  GetSSLConfigClientReceiver();

  // Populates |initial_ssl_config| and |ssl_config_client_receiver| members of
  // |network_context_params|. As long as the SystemNetworkContextManager
  // exists, any NetworkContext created with the params will continue to get
  // SSL configuration updates.
  void AddSSLConfigToNetworkContextParams(
      network::mojom::NetworkContextParams* network_context_params);

  // Returns default set of parameters for configuring the network service.
  network::mojom::NetworkContextParamsPtr CreateDefaultNetworkContextParams();

  // Returns a shared global NetExportFileWriter instance, used by net-export.
  // It lives here so it can outlive chrome://net-export/ if the tab is closed
  // or destroyed, and so that it's destroyed before Mojo is shut down.
  net_log::NetExportFileWriter* GetNetExportFileWriter();

  // Flushes all pending SSL configuration changes.
  void FlushSSLConfigManagerForTesting();

  // Flushes all pending proxy configuration changes.
  void FlushProxyConfigMonitorForTesting();

  // Call |FlushForTesting()| on Network Service related interfaces. For test
  // use only.
  void FlushNetworkInterfaceForTesting();

  // Returns configuration that would be sent to the stub DNS resolver.
  static void GetStubResolverConfigForTesting(
      bool* insecure_stub_resolver_enabled,
      net::DnsConfig::SecureDnsMode* secure_dns_mode,
      base::Optional<std::vector<network::mojom::DnsOverHttpsServerPtr>>*
          dns_over_https_servers);

  static network::mojom::HttpAuthStaticParamsPtr
  GetHttpAuthStaticParamsForTesting();
  static network::mojom::HttpAuthDynamicParamsPtr
  GetHttpAuthDynamicParamsForTesting();

  // Enables Certificate Transparency and enforcing the Chrome Certificate
  // Transparency Policy. For test use only. Use base::nullopt_t to reset to
  // the default state.
  static void SetEnableCertificateTransparencyForTesting(
      base::Optional<bool> enabled);

 private:
  class URLLoaderFactoryForSystem;

  // Constructor. |pref_service| must out live this object.
  explicit SystemNetworkContextManager(PrefService* pref_service);

  void UpdateReferrersEnabled();

  // Creates parameters for the NetworkContext. May only be called once, since
  // it initializes some class members.
  network::mojom::NetworkContextParamsPtr CreateNetworkContextParams();

  // The PrefService to retrieve all the pref values.
  PrefService* local_state_;

  // This is an instance of the default SSLConfigServiceManager for the current
  // platform and it gets SSL preferences from the BrowserProcess's local_state
  // object. It's shared with other NetworkContexts.
  std::unique_ptr<SSLConfigServiceManager> ssl_config_service_manager_;

  ProxyConfigMonitor proxy_config_monitor_;

  // NetworkContext using the network service, if the network service is
  // enabled. mojo::NullRemote(), otherwise.
  mojo::Remote<network::mojom::NetworkContext> network_service_network_context_;

  // URLLoaderFactory backed by the NetworkContext returned by GetContext(), so
  // consumers don't all need to create their own factory.
  scoped_refptr<URLLoaderFactoryForSystem> shared_url_loader_factory_;
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;

  bool is_quic_allowed_ = true;

  PrefChangeRegistrar pref_change_registrar_;

  BooleanPrefMember enable_referrers_;

  // Initialized on first access.
  std::unique_ptr<net_log::NetExportFileWriter> net_export_file_writer_;

  DISALLOW_COPY_AND_ASSIGN(SystemNetworkContextManager);
};

#endif  // CHROME_BROWSER_NET_SYSTEM_NETWORK_CONTEXT_MANAGER_H_
