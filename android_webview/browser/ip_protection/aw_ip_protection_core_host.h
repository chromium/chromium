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
#include "components/ip_protection/common/ip_protection_core_host_helper.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_proxy_config_direct_fetcher.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
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
struct BlindSignToken;
}  // namespace quiche

namespace android_webview {

// Fetches IP protection tokens and proxy list on demand for the network
// service.

// TODO(b/346997109): Refactor AwIpProtectionCoreHost to reduce code
// duplication once a common implementation of IpProtectionConfigGetter is
// added.
class AwIpProtectionCoreHost
    : public KeyedService,
      public network::mojom::IpProtectionConfigGetter {
 public:
  explicit AwIpProtectionCoreHost(AwBrowserContext* aw_browser_context);

  ~AwIpProtectionCoreHost() override;

  AwIpProtectionCoreHost(const AwIpProtectionCoreHost&) = delete;
  AwIpProtectionCoreHost& operator=(const AwIpProtectionCoreHost&) =
      delete;

  // IpProtectionConfigGetter:
  // Get a batch of blind-signed auth tokens.
  void TryGetAuthTokens(uint32_t batch_size,
                        network::mojom::IpProtectionProxyLayer proxy_layer,
                        TryGetAuthTokensCallback callback) override;
  // Get the list of IP Protection proxies.
  void GetProxyList(GetProxyListCallback callback) override;

  // KeyedService:

  // We do not currently support destroying WebView's browser context. No
  // shutdown code will be executed on termination of the browser process so
  // this is not actually being tested yet. However, we would like to support
  // destroying browser context in the future so this method contains an idea of
  // how this could be done. Note that Shutdown should not be called more than
  // once.
  void Shutdown() override;

  static AwIpProtectionCoreHost* Get(
      AwBrowserContext* aw_browser_context);

  static bool CanIpProtectionBeEnabled();

  // Checks if IP Protection is disabled.
  bool IsIpProtectionEnabled();

  // Binds Mojo interfaces to be passed to a new network service.
  void AddNetworkService(
      mojo::PendingReceiver<network::mojom::IpProtectionConfigGetter>
          pending_receiver,
      mojo::PendingRemote<network::mojom::IpProtectionControl> pending_remote);

  // Like `SetUp()`, but providing values for each of the member variables. Note
  // `bsa` is moved onto a separate sequence when initializing
  // `ip_protection_token_ipc_fetcher_`.
  void SetUpForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<quiche::BlindSignAuthInterface> bsa);

 private:
  // Set up `ip_protection_token_ipc_fetcher_`
  // and`ip_protection_proxy_config_retriever_`, if not already initialized.
  void SetUp();

  // `FetchBlindSignedToken()` uses the `ip_protection_token_ipc_fetcher_`
  // to make an async call on the bound sequence into the
  // `quiche::BlindSignAuth` library to request a blind-signed auth token for
  // use at the IP Protection proxies.
  void FetchBlindSignedToken(int batch_size,
                             quiche::ProxyLayer quiche_proxy_layer,
                             TryGetAuthTokensCallback callback);
  void OnFetchBlindSignedTokenCompleted(
      base::TimeTicks bsa_get_tokens_start_time,
      TryGetAuthTokensCallback callback,
      absl::StatusOr<std::vector<quiche::BlindSignToken>> tokens);

  // Finish a call to `TryGetAuthTokens()` by recording the result and invoking
  // its callback.
  void TryGetAuthTokensComplete(
      std::optional<std::vector<ip_protection::BlindSignedAuthToken>>
          bsa_tokens,
      TryGetAuthTokensCallback callback,
      ip_protection::TryGetAuthTokensAndroidResult result,
      std::optional<base::TimeDelta> duration = std::nullopt);

  // Calculates the backoff time for the given result, based on
  // `last_try_get_auth_tokens_..` fields, and updates those fields.
  std::optional<base::TimeDelta> CalculateBackoff(
      ip_protection::TryGetAuthTokensAndroidResult result);

  void AuthenticateCallback(
      std::unique_ptr<network::ResourceRequest>,
      ip_protection::IpProtectionProxyConfigDirectFetcher::
          AuthenticateDoneCallback);

  // Injected browser context.
  raw_ptr<AwBrowserContext> aw_browser_context_;

  std::unique_ptr<ip_protection::IpProtectionProxyConfigDirectFetcher>
      ip_protection_proxy_config_fetcher_;

  // The thread pool task runner on which async calls are made to
  // `ip_protection_token_ipc_fetcher` to fetch blind signed tokens. This
  // is needed to move some of the expensive token generation work off the UI
  // thread.
  scoped_refptr<base::SequencedTaskRunner> token_fetcher_task_runner_;

  // An IpProtectionTokenIpcFetcher instance that is bound to the given
  // sequenced `token_fetcher_task_runner_` on which all calls to the
  // `quiche::BlindSignAuth` library will happen on.
  base::SequenceBound<ip_protection::IpProtectionTokenIpcFetcher>
      ip_protection_token_ipc_fetcher_;

  // Whether `Shutdown()` has been called.
  bool is_shutting_down_ = false;

  // The result of the last call to `TryGetAuthTokens()`, and the
  // backoff applied to `try_again_after`. `last_try_get_auth_tokens_backoff_`
  // will be set to `base::TimeDelta::Max()` if no further attempts to get
  // tokens should be made. These will be updated by calls from any receiver.
  ip_protection::TryGetAuthTokensAndroidResult
      last_try_get_auth_tokens_result_ =
          ip_protection::TryGetAuthTokensAndroidResult::kSuccess;
  std::optional<base::TimeDelta> last_try_get_auth_tokens_backoff_;

  // The `mojo::Receiver` objects allowing the network service to call methods
  // on `this`.
  mojo::ReceiverSet<network::mojom::IpProtectionConfigGetter> receivers_;

  // Similar to `receivers_`, but containing remotes for all existing
  // IpProtectionProxyDelegates.
  mojo::RemoteSet<network::mojom::IpProtectionControl> remotes_;

  // True if this class is being tested.
  bool for_testing_ = false;

  // This must be the last member in this class.
  base::WeakPtrFactory<AwIpProtectionCoreHost> weak_ptr_factory_{this};
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_IP_PROTECTION_AW_IP_PROTECTION_CORE_HOST_H_
