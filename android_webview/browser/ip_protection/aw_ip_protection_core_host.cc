// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/ip_protection/aw_ip_protection_core_host.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/ip_protection/aw_ip_protection_core_host_factory.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "components/ip_protection/android/ip_protection_token_ipc_fetcher.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_proxy_config_direct_fetcher.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
#include "components/version_info/android/channel_getter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/common/api_key_request_util.h"
#include "google_apis/google_api_keys.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/features.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/blind_sign_auth_options.pb.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/spend_token_data.pb.h"
#include "third_party/abseil-cpp/absl/status/status.h"

namespace android_webview {

using ::ip_protection::TryGetAuthTokensAndroidResult;

AwIpProtectionCoreHost::AwIpProtectionCoreHost(
    AwBrowserContext* aw_browser_context)
    : aw_browser_context_(aw_browser_context) {}

AwIpProtectionCoreHost::~AwIpProtectionCoreHost() = default;

void AwIpProtectionCoreHost::SetUp() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!ip_protection_token_fetcher_) {
    ip_protection_token_fetcher_ =
        std::make_unique<ip_protection::IpProtectionTokenIpcFetcher>(this);
  }

  if (!ip_protection_proxy_config_fetcher_) {
    CHECK(aw_browser_context_);
    ip_protection_proxy_config_fetcher_ =
        std::make_unique<ip_protection::IpProtectionProxyConfigDirectFetcher>(
            aw_browser_context_->GetDefaultStoragePartition()
                ->GetURLLoaderFactoryForBrowserProcess()
                .get(),
            ip_protection::IpProtectionTokenFetcherHelper::kWebViewIpBlinding,
            this);
  }
}

void AwIpProtectionCoreHost::SetUpForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<quiche::BlindSignAuthInterface> bsa) {
  for_testing_ = true;

  // Carefully destroy any existing values in the correct order.
  ip_protection_token_fetcher_ = nullptr;
  ip_protection_proxy_config_fetcher_ = nullptr;

  ip_protection_token_fetcher_ =
      std::make_unique<ip_protection::IpProtectionTokenIpcFetcher>(
          this, std::move(bsa));
  ip_protection_proxy_config_fetcher_ =
      std::make_unique<ip_protection::IpProtectionProxyConfigDirectFetcher>(
          std::move(url_loader_factory),
          ip_protection::IpProtectionTokenFetcherHelper::kWebViewIpBlinding,
          this);
}

void AwIpProtectionCoreHost::GetProxyConfig(GetProxyConfigCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(!is_shutting_down_);
  SetUp();

  // If IP Protection is disabled then don't attempt to get a proxy list.
  if (!IsIpProtectionEnabled()) {
    std::move(callback).Run(/*proxy_chains=*/std::nullopt,
                            /*geo_hint=*/std::nullopt);
    return;
  }

  ip_protection_proxy_config_fetcher_->GetProxyConfig(base::BindOnce(
      // Convert the mojo style callback, which takes `const
      // std::optional<..>&`, to the preferred style, passing `std::optional` by
      // value.
      [](GetProxyConfigCallback callback,
         std::optional<std::vector<::net::ProxyChain>> proxy_chain,
         std::optional<ip_protection::GeoHint> geo_hint) {
        std::move(callback).Run(proxy_chain, geo_hint);
      },
      std::move(callback)));
}

void AwIpProtectionCoreHost::AuthenticateRequest(
    std::unique_ptr<network::ResourceRequest> resource_request,
    ip_protection::IpProtectionProxyConfigDirectFetcher::Delegate::
        AuthenticateRequestCallback callback) {
  google_apis::AddAPIKeyToRequest(
      *resource_request,
      google_apis::GetAPIKey(version_info::android::GetChannel()));
  std::move(callback).Run(true, std::move(resource_request));
}

void AwIpProtectionCoreHost::TryGetAuthTokens(
    uint32_t batch_size,
    ip_protection::ProxyLayer proxy_layer,
    TryGetAuthTokensCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(!is_shutting_down_);
  SetUp();

  // The `batch_size` is cast to an `int` for use by BlindSignAuth, so check
  // for overflow here.
  if (!base::IsValueInRangeForNumericType<int>(batch_size)) {
    receivers_.ReportBadMessage("Invalid batch_size");
    return;
  }

  // The mojo callback requires `std::optional<..>&`, while the fetcher callback
  // provides arguments by value. This seemingly-redundant lambda converts the
  // two.
  auto callback_with_refs = base::BindOnce(
      [](TryGetAuthTokensCallback callback,
         std::optional<std::vector<ip_protection::BlindSignedAuthToken>> tokens,
         std::optional<::base::Time> try_again_after) {
        std::move(callback).Run(tokens, try_again_after);
      },
      std::move(callback));

  ip_protection_token_fetcher_->TryGetAuthTokens(batch_size, proxy_layer,
                                                 std::move(callback_with_refs));
}

void AwIpProtectionCoreHost::TryGetProbabilisticRevealTokens(
    TryGetProbabilisticRevealTokensCallback callback) {
  // PRTs are not supported in WebView.
  NOTREACHED();
}

void AwIpProtectionCoreHost::Shutdown() {
  if (is_shutting_down_) {
    return;
  }
  is_shutting_down_ = true;
  receivers_.Clear();

  ip_protection_token_fetcher_ = nullptr;
  aw_browser_context_ = nullptr;
  ip_protection_proxy_config_fetcher_ = nullptr;
}

// static
AwIpProtectionCoreHost* AwIpProtectionCoreHost::Get(
    AwBrowserContext* aw_browser_context) {
  return AwIpProtectionCoreHostFactory::GetForAwBrowserContext(
      aw_browser_context);
}

void AwIpProtectionCoreHost::AddNetworkService(
    mojo::PendingReceiver<ip_protection::mojom::CoreHost> pending_receiver,
    mojo::PendingRemote<ip_protection::mojom::CoreControl> pending_remote) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(!is_shutting_down_);
  receivers_.Add(this, std::move(pending_receiver));
  remotes_.Add(std::move(pending_remote));
}

// static
bool AwIpProtectionCoreHost::CanIpProtectionBeEnabled() {
  return base::FeatureList::IsEnabled(net::features::kEnableIpProtectionProxy);
}

// TODO(b/335420700): Update to return feature flag.
bool AwIpProtectionCoreHost::IsIpProtectionEnabled() {
  if (is_shutting_down_) {
    return false;
  }
  return CanIpProtectionBeEnabled();
}

bool AwIpProtectionCoreHost::IsProxyConfigFetchEnabled() {
  return IsIpProtectionEnabled();
}

bool AwIpProtectionCoreHost::IsTokenFetchEnabled() {
  return IsIpProtectionEnabled();
}

}  // namespace android_webview
