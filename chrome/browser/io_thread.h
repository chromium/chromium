// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IO_THREAD_H_
#define CHROME_BROWSER_IO_THREAD_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/common/buildflags.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browser_thread_delegate.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/network_change_notifier.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/url_request_context_owner.h"

class PrefRegistrySimple;
class PrefService;
class SystemNetworkContextManager;

namespace chrome_browser_net {
class DnsProbeService;
}

namespace data_use_measurement {
class ChromeDataUseAscriber;
}

namespace extensions {
class EventRouterForwarder;
}

namespace net {
class CertVerifier;
class HostResolver;
class NetworkQualityEstimator;
class URLRequestContext;
class URLRequestContextGetter;
}  // namespace net

namespace net_log {
class ChromeNetLog;
}

namespace network {
class URLRequestContextBuilderMojo;
}

namespace policy {
class PolicyService;
}  // namespace policy

// Contains state associated with, initialized and cleaned up on, and
// primarily used on, the IO thread.
//
// If you are looking to interact with the IO thread (e.g. post tasks
// to it or check if it is the current thread), see
// content::BrowserThread.
class IOThread : public content::BrowserThreadDelegate {
 public:
  struct Globals {
    class SystemRequestContextLeakChecker {
     public:
      explicit SystemRequestContextLeakChecker(Globals* globals);
      ~SystemRequestContextLeakChecker();

     private:
      Globals* const globals_;
    };

    Globals();
    ~Globals();

    bool quic_disabled = false;

    // Ascribes all data use in Chrome to a source, such as page loads.
    std::unique_ptr<data_use_measurement::ChromeDataUseAscriber>
        data_use_ascriber;

    // NetworkQualityEstimator only for use in dummy in-process
    // URLRequestContext when network service is enabled.
    // TODO(mmenke): Remove this, once all consumers only access the
    // NetworkQualityEstimator through network service APIs. Then will no longer
    // need to create an in-process one.
    std::unique_ptr<net::NetworkQualityEstimator>
        deprecated_network_quality_estimator;

    // HostResolver only for use in dummy in-process
    // URLRequestContext when network service is enabled.
    std::unique_ptr<net::HostResolver> deprecated_host_resolver;

    // When the network service is enabled, this holds on to a
    // content::NetworkContext class that owns |system_request_context|.
    std::unique_ptr<network::mojom::NetworkContext> system_network_context;
    // When the network service is disabled, this owns |system_request_context|.
    network::URLRequestContextOwner system_request_context_owner;
    net::URLRequestContext* system_request_context;
#if BUILDFLAG(ENABLE_EXTENSIONS)
    scoped_refptr<extensions::EventRouterForwarder>
        extension_event_router_forwarder;
#endif
    // NetErrorTabHelper uses |dns_probe_service| to send DNS probes when a
    // main frame load fails with a DNS error in order to provide more useful
    // information to the renderer so it can show a more specific error page.
    std::unique_ptr<chrome_browser_net::DnsProbeService> dns_probe_service;
  };

  // |net_log| must either outlive the IOThread or be NULL.
  IOThread(PrefService* local_state,
           policy::PolicyService* policy_service,
           net_log::ChromeNetLog* net_log,
           extensions::EventRouterForwarder* extension_event_router_forwarder,
           SystemNetworkContextManager* system_network_context_manager);

  ~IOThread() override;

  static void RegisterPrefs(PrefRegistrySimple* registry);
  static void SetCertVerifierForTesting(net::CertVerifier* cert_verifier);

  // Can only be called on the IO thread.
  Globals* globals();

  net_log::ChromeNetLog* net_log();

  // Returns a getter for the URLRequestContext.  Only called on the UI thread.
  net::URLRequestContextGetter* system_url_request_context_getter();

  // Dynamically disables QUIC for all NetworkContexts using the IOThread's
  // NetworkService. Re-enabling Quic dynamically is not supported for
  // simplicity and requires a browser restart. May only be called on the IO
  // thread.
  void DisableQuic();

  // Configures |builder|'s ProxyResolutionService based on prefs and policies.
  void SetUpProxyService(network::URLRequestContextBuilderMojo* builder) const;

 private:
  // BrowserThreadDelegate implementation, runs on the IO thread.
  // This handles initialization and destruction of state that must
  // live on the IO thread.
  void Init() override;
  void CleanUp() override;

  extensions::EventRouterForwarder* extension_event_router_forwarder() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    return extension_event_router_forwarder_;
#else
    return NULL;
#endif
  }
  void ConstructSystemRequestContext();

  // The NetLog is owned by the browser process, to allow logging from other
  // threads during shutdown, but is used most frequently on the IOThread.
  net_log::ChromeNetLog* net_log_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // The extensions::EventRouterForwarder allows for sending events to
  // extensions from the IOThread.
  extensions::EventRouterForwarder* extension_event_router_forwarder_;
#endif

  // These member variables are basically global, but their lifetimes are tied
  // to the IOThread.  IOThread owns them all, despite not using scoped_ptr.
  // This is because the destructor of IOThread runs on the wrong thread.  All
  // member variables should be deleted in CleanUp().

  // These member variables are initialized in Init() and do not change for the
  // lifetime of the IO thread.

  Globals* globals_;

  // These are set on the UI thread, and then consumed during initialization on
  // the IO thread.
  network::mojom::NetworkContextRequest network_context_request_;
  network::mojom::NetworkContextParamsPtr network_context_params_;

  scoped_refptr<net::URLRequestContextGetter>
      system_url_request_context_getter_;

  bool stub_resolver_enabled_ = false;
  base::Optional<std::vector<network::mojom::DnsOverHttpsServerPtr>>
      dns_over_https_servers_;

  // Initial HTTP auth configuration used when setting up the NetworkService on
  // the IO Thread. Future updates are sent using the NetworkService mojo
  // interface, but initial state needs to be set non-racily.
  network::mojom::HttpAuthStaticParamsPtr http_auth_static_params_;
  network::mojom::HttpAuthDynamicParamsPtr http_auth_dynamic_params_;

  // True if QUIC is initially enabled.
  bool is_quic_allowed_on_init_;

  base::WeakPtrFactory<IOThread> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(IOThread);
};

#endif  // CHROME_BROWSER_IO_THREAD_H_
