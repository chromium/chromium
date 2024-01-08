// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DBUS_PROXY_RESOLUTION_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_DBUS_PROXY_RESOLUTION_SERVICE_PROVIDER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/services/cros_dbus_service.h"
#include "dbus/exported_object.h"
#include "net/base/network_anonymization_key.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace dbus {
class MethodCall;
}  // namespace dbus

namespace network {
namespace mojom {
class NetworkContext;
}  // namespace mojom
}  // namespace network

namespace ash {

// This class processes proxy resolution requests for Chrome OS clients.
//
// The following method is exported:
//
// Interface: org.chromium.NetworkProxyServiceInterface
//            (kNetworkProxyServiceInterface)
// Method: ResolveProxy (kNetworkProxyServiceResolveProxyMethod)
// Parameters: string:source_url
//
//   Resolves the proxy for |source_url| and returns proxy information via an
//   asynchronous response containing two values:
//
//   - string:proxy_info - proxy info for the source URL in PAC format
//                         like "PROXY cache.example.com:12345"
//   - string:error_message - error message. Empty if successful.
//
// This service can be manually tested using dbus-send:
//
//   % dbus-send --system --type=method_call --print-reply
//       --dest=org.chromium.NetworkProxyService
//       /org/chromium/NetworkProxyService
//       org.chromium.NetworkProxyServiceInterface.ResolveProxy
//       string:https://www.google.com/
//
class ProxyResolutionServiceProvider
    : public CrosDBusService::ServiceProviderInterface {
 public:
  // Callback that is invoked with the result of proxy resolution. On success
  // |error| is empty, and |pac_string| contains the result. Otherwise |error|
  // is non-empty.
  using NotifyCallback =
      base::OnceCallback<void(const std::string& error,
                              const std::string& pac_string)>;

  ProxyResolutionServiceProvider();

  ProxyResolutionServiceProvider(const ProxyResolutionServiceProvider&) =
      delete;
  ProxyResolutionServiceProvider& operator=(
      const ProxyResolutionServiceProvider&) = delete;

  ~ProxyResolutionServiceProvider() override;

  void set_network_context_for_test(
      network::mojom::NetworkContext* network_context) {
    network_context_for_test_ = network_context;
    use_network_context_for_test_ = true;
  }

  // CrosDBusService::ServiceProviderInterface:
  void Start(scoped_refptr<dbus::ExportedObject> exported_object) override;

 private:
  friend class ProxyResolutionServiceProviderTestWrapper;

  // Returns true if called on |origin_thread_|.
  bool OnOriginThread();

  // Called when ResolveProxy() is exported as a D-Bus method.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  // Callback invoked when Chrome OS clients send network proxy resolution
  // requests to the service. Called on UI thread.
  void DbusResolveProxy(dbus::MethodCall* method_call,
                        dbus::ExportedObject::ResponseSender response_sender);

  void ResolveProxyInternal(
      const std::string& source_url,
      NotifyCallback callback,
      chromeos::SystemProxyOverride system_proxy_override);

  // Called on UI thread from OnResolutionComplete() to pass the resolved proxy
  // information to the client over D-Bus.
  void NotifyProxyResolved(std::unique_ptr<dbus::Response> response,
                           dbus::ExportedObject::ResponseSender response_sender,
                           const std::string& error,
                           const std::string& pac_string);

  // Returns the NetworkContext to use for resolving the proxy. This may be
  // mocked by testing using set_network_context_for_test(), otherwise it picks
  // the primary user profile's default.
  network::mojom::NetworkContext* GetNetworkContext();

  scoped_refptr<dbus::ExportedObject> exported_object_;
  scoped_refptr<base::SingleThreadTaskRunner> origin_thread_;
  raw_ptr<network::mojom::NetworkContext> network_context_for_test_ = nullptr;
  bool use_network_context_for_test_ = false;

  // A transient NetworkAnonymizationKey used for all requests. This prevents
  // what hostnames have been resolved using any PAC scripts to websites, while
  // allowing cached host resolutions between DBus calls. Since only Chrome OS
  // system daemons have access to DBus, don't have to worry about information
  // leaks between them. In the case no PAC script is in use, the
  // NetworkAnonymizationKey has no effect.
  const net::NetworkAnonymizationKey network_anonymization_key_;

  base::WeakPtrFactory<ProxyResolutionServiceProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DBUS_PROXY_RESOLUTION_SERVICE_PROVIDER_H_
