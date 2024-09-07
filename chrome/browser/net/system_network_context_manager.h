// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SYSTEM_NETWORK_CONTEXT_MANAGER_H_
#define CHROME_BROWSER_NET_SYSTEM_NETWORK_CONTEXT_MANAGER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/net/cert_verifier_service_time_updater.h"
#include "chrome/browser/net/cookie_encryption_provider_impl.h"
#include "chrome/browser/net/proxy_config_monitor.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/browser/ssl/ssl_config_service_manager.h"
#include "chrome/common/buildflags.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom-forward.h"
#include "services/network/public/mojom/host_resolver.mojom-forward.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/ssl_config.mojom-forward.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

class NetworkAnnotationMonitor;
class PrefRegistrySimple;
class PrefService;

namespace network {
namespace mojom {
class URLLoaderFactory;
}
class SharedURLLoaderFactory;
}  // namespace network

namespace net_log {
class NetExportFileWriter;
class NetLogProxySource;
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
  SystemNetworkContextManager(const SystemNetworkContextManager&) = delete;
  SystemNetworkContextManager& operator=(const SystemNetworkContextManager&) =
      delete;

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

  static StubResolverConfigReader* GetStubResolverConfigReader();

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
  // NetworkContextParam.
  mojo::PendingReceiver<network::mojom::SSLConfigClient>
  GetSSLConfigClientReceiver();

  // Adds a CookieEncryptionManager mojo remote to the specified
  // `network_context_params`.
  void AddCookieEncryptionManagerToNetworkContextParams(
      network::mojom::NetworkContextParams* network_context_params);

  // Populates |initial_ssl_config| and |ssl_config_client_receiver| members of
  // |network_context_params|. As long as the SystemNetworkContextManager
  // exists, any NetworkContext created with the params will continue to get
  // SSL configuration updates.
  void AddSSLConfigToNetworkContextParams(
      network::mojom::NetworkContextParams* network_context_params);

  // Configures default set of parameters for configuring the network context.
  void ConfigureDefaultNetworkContextParams(
      network::mojom::NetworkContextParams* network_context_params);

  // Performs the same function as ConfigureDefaultNetworkContextParams(), and
  // then returns a newly allocated network::mojom::NetworkContextParams with
  // some modifications: if the CertVerifierService is enabled, the new
  // NetworkContextParams will contain a CertVerifierServiceRemoteParams.
  // Otherwise the newly configured CertVerifierCreationParams is placed
  // directly into the NetworkContextParams.
  network::mojom::NetworkContextParamsPtr CreateDefaultNetworkContextParams();

  // Returns a shared global NetExportFileWriter instance, used by net-export.
  // It lives here so it can outlive chrome://net-export/ if the tab is closed
  // or destroyed, and so that it's destroyed before Mojo is shut down.
  net_log::NetExportFileWriter* GetNetExportFileWriter();

  // Returns whether the network sandbox is enabled. This depends on policy but
  // also feature status from sandbox. Called before there is an instance of
  // SystemNetworkContextManager.
  static bool IsNetworkSandboxEnabled();

  // Flushes all pending SSL configuration changes.
  void FlushSSLConfigManagerForTesting();

  // Flushes all pending proxy configuration changes.
  void FlushProxyConfigMonitorForTesting();

  // Call |FlushForTesting()| on Network Service related interfaces. For test
  // use only.
  void FlushNetworkInterfaceForTesting();

#if BUILDFLAG(IS_CHROMEOS)
  // Call |FlushForTesting()| on NetworkAnnotationMonitor. For test use only.
  void FlushNetworkAnnotationMonitorForTesting();
#endif  // BUILDFLAG(IS_CHROMEOS)

  static network::mojom::HttpAuthStaticParamsPtr
  GetHttpAuthStaticParamsForTesting();
  static network::mojom::HttpAuthDynamicParamsPtr
  GetHttpAuthDynamicParamsForTesting();

  // Enables Certificate Transparency and enforcing the Chrome Certificate
  // Transparency Policy. For test use only. Use std::nullopt_t to reset to
  // the default state.
  static void SetEnableCertificateTransparencyForTesting(
      std::optional<bool> enabled);

  // Reloads the static CT log lists but overriding the log list update time
  // with the current time. For test use only.
  void SetCTLogListTimelyForTesting();

  static bool IsCertificateTransparencyEnabled();

  static void set_stub_resolver_config_reader_for_testing(
      StubResolverConfigReader* reader) {
    stub_resolver_config_reader_for_testing_ = reader;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(
      SystemNetworkContextServiceCertVerifierBuiltinPermissionsPolicyTest,
      Test);

  class URLLoaderFactoryForSystem;
  class NetworkProcessLaunchWatcher;

#if BUILDFLAG(IS_LINUX)
  class GssapiLibraryLoadObserver
      : public network::mojom::GssapiLibraryLoadObserver {
   public:
    explicit GssapiLibraryLoadObserver(SystemNetworkContextManager* owner);
    GssapiLibraryLoadObserver(const GssapiLibraryLoadObserver&) = delete;
    GssapiLibraryLoadObserver& operator=(const GssapiLibraryLoadObserver&) =
        delete;
    ~GssapiLibraryLoadObserver() override;

    void Install(network::mojom::NetworkService* network_service);

    // network::mojom::GssapiLibraryLoadObserver implementation:
    void OnBeforeGssapiLibraryLoad() override;

   private:
    mojo::Receiver<network::mojom::GssapiLibraryLoadObserver>
        gssapi_library_loader_observer_receiver_{this};
    raw_ptr<SystemNetworkContextManager> owner_;
  };
#endif

  // Constructor. |pref_service| must out live this object.
  explicit SystemNetworkContextManager(PrefService* pref_service);

  void UpdateReferrersEnabled();

  // Creates parameters for the NetworkContext. May only be called once, since
  // it initializes some class members.
  network::mojom::NetworkContextParamsPtr CreateNetworkContextParams();

  // Send the current value of the net.explicitly_allowed_network_ports pref to
  // the network process.
  void UpdateExplicitlyAllowedNetworkPorts();

  void UpdateIPv6ReachabilityOverrideEnabled();

  // The PrefService to retrieve all the pref values.
  raw_ptr<PrefService> local_state_;

  // This is an instance of the default SSLConfigServiceManager for the current
  // platform and it gets SSL preferences from the BrowserProcess's local_state
  // object. It's shared with other NetworkContexts.
  SSLConfigServiceManager ssl_config_service_manager_;

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

  // Copies NetLog events from the browser process to the Network Service, if
  // the network service is running in a separate process. It will be destroyed
  // and re-created on Network Service crash.
  std::unique_ptr<net_log::NetLogProxySource> net_log_proxy_source_;

  // Initialized on first access.
  std::unique_ptr<net_log::NetExportFileWriter> net_export_file_writer_;

  std::unique_ptr<NetworkProcessLaunchWatcher> network_process_launch_watcher_;

  StubResolverConfigReader stub_resolver_config_reader_;
  static StubResolverConfigReader* stub_resolver_config_reader_for_testing_;

  static std::optional<bool> certificate_transparency_enabled_for_testing_;

#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<NetworkAnnotationMonitor> network_annotation_monitor_;
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_LINUX)
  GssapiLibraryLoadObserver gssapi_library_loader_observer_{this};
#endif  // BUILDFLAG(IS_LINUX)

  CookieEncryptionProviderImpl cookie_encryption_provider_;

  std::unique_ptr<CertVerifierServiceTimeUpdater> cert_verifier_time_updater_;
};

#endif  // CHROME_BROWSER_NET_SYSTEM_NETWORK_CONTEXT_MANAGER_H_
