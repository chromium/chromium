// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_IP_PROTECTION_AW_IP_PROTECTION_CORE_HOST_H_
#define ANDROID_WEBVIEW_BROWSER_IP_PROTECTION_AW_IP_PROTECTION_CORE_HOST_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "android_webview/browser/aw_browser_context.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "components/ip_protection/android/ip_protection_token_ipc_fetcher.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_proxy_config_direct_fetcher.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
#include "components/ip_protection/mojom/core.mojom.h"
#include "components/ip_protection/mojom/data_types.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/abseil-cpp/absl/status/status.h"

namespace quiche {
class BlindSignAuthInterface;
enum class ProxyLayer;
}  // namespace quiche

namespace android_webview {

// Fetches IP protection tokens and proxy list on demand for the network
// service.

// TODO(b/346997109): Refactor AwIpProtectionCoreHost to reduce code
// duplication once a common implementation of IpProtectionConfigGetter is
// added.
class AwIpProtectionCoreHost
    : public KeyedService,
      public ip_protection::mojom::CoreHost,
      public ip_protection::IpProtectionTokenIpcFetcher::Delegate,
      public ip_protection::IpProtectionProxyConfigDirectFetcher::Delegate {
 public:
  explicit AwIpProtectionCoreHost(AwBrowserContext* aw_browser_context);

  ~AwIpProtectionCoreHost() override;

  AwIpProtectionCoreHost(const AwIpProtectionCoreHost&) = delete;
  AwIpProtectionCoreHost& operator=(const AwIpProtectionCoreHost&) = delete;

  // IpProtectionConfigGetter:
  // Get a batch of blind-signed auth tokens.
  void TryGetAuthTokens(uint32_t batch_size,
                        ip_protection::ProxyLayer proxy_layer,
                        TryGetAuthTokensCallback callback) override;
  // Get the list of IP Protection proxies.
  void GetProxyConfig(GetProxyConfigCallback callback) override;

  // PRTs are not supported in WebView. This method is here to make
  // build work.
  void TryGetProbabilisticRevealTokens(
      TryGetProbabilisticRevealTokensCallback callback) override;

  // KeyedService:

  // We do not currently support destroying WebView's browser context. No
  // shutdown code will be executed on termination of the browser process so
  // this is not actually being tested yet. However, we would like to support
  // destroying browser context in the future so this method contains an idea of
  // how this could be done. Note that Shutdown should not be called more than
  // once.
  void Shutdown() override;

  static AwIpProtectionCoreHost* Get(AwBrowserContext* aw_browser_context);

  static bool CanIpProtectionBeEnabled();
  bool IsIpProtectionEnabled();

  // `IpProtectionTokenIpcFetcher::Delegate` implementation.
  bool IsTokenFetchEnabled() override;

  // `IpProtectionProxyConfigDirectFetcher::Delegate` implementation
  bool IsProxyConfigFetchEnabled() override;
  void AuthenticateRequest(std::unique_ptr<network::ResourceRequest>,
                           ip_protection::IpProtectionProxyConfigDirectFetcher::
                               Delegate::AuthenticateRequestCallback) override;

  // Binds Mojo interfaces to be passed to a new network service.
  void AddNetworkService(
      mojo::PendingReceiver<ip_protection::mojom::CoreHost> pending_receiver,
      mojo::PendingRemote<ip_protection::mojom::CoreControl> pending_remote);

  // Like `SetUp()`, but providing values for each of the member variables. Note
  // `bsa` is moved onto a separate sequence when initializing
  // `ip_protection_token_fetcher_`.
  void SetUpForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<quiche::BlindSignAuthInterface> bsa);

 private:
  // Set up `ip_protection_token_fetcher_`
  // and`ip_protection_proxy_config_retriever_`, if not already initialized.
  void SetUp();

  // Injected browser context.
  raw_ptr<AwBrowserContext> aw_browser_context_;

  std::unique_ptr<ip_protection::IpProtectionProxyConfigDirectFetcher>
      ip_protection_proxy_config_fetcher_;

  // An IpProtectionTokenIpcFetcher instance for fetching tokens.
  std::unique_ptr<ip_protection::IpProtectionTokenIpcFetcher>
      ip_protection_token_fetcher_;

  // Whether `Shutdown()` has been called.
  bool is_shutting_down_ = false;

  // The `mojo::Receiver` objects allowing the network service to call methods
  // on `this`.
  mojo::ReceiverSet<ip_protection::mojom::CoreHost> receivers_;

  // Similar to `receivers_`, but containing remotes for all existing
  // IpProtectionProxyDelegates.
  mojo::RemoteSet<ip_protection::mojom::CoreControl> remotes_;

  // True if this class is being tested.
  bool for_testing_ = false;

  // This must be the last member in this class.
  base::WeakPtrFactory<AwIpProtectionCoreHost> weak_ptr_factory_{this};
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_IP_PROTECTION_AW_IP_PROTECTION_CORE_HOST_H_
