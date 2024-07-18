// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_ip_protection_config_provider.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/aw_ip_protection_config_provider_factory.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "components/ip_protection/blind_sign_message_android_impl.h"
#include "components/ip_protection/ip_protection_config_provider_helper.h"
#include "components/ip_protection/ip_protection_proxy_config_fetcher.h"
#include "components/version_info/android/channel_getter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
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

namespace {
// TODO(crbug.com/40216037): Once `google_apis::GetAPIKey()` handles this
// logic we can remove this helper.
std::string GetAPIKey() {
  version_info::Channel channel = version_info::android::GetChannel();
  return channel == version_info::Channel::STABLE
             ? google_apis::GetAPIKey()
             : google_apis::GetNonStableAPIKey();
}
}  // namespace

namespace android_webview {

AwIpProtectionConfigProvider::AwIpProtectionConfigProvider(
    AwBrowserContext* aw_browser_context)
    : aw_browser_context_(aw_browser_context) {}

AwIpProtectionConfigProvider::~AwIpProtectionConfigProvider() = default;

void AwIpProtectionConfigProvider::SetUp() {
  if (!blind_sign_message_android_impl_) {
    blind_sign_message_android_impl_ =
        std::make_unique<ip_protection::BlindSignMessageAndroidImpl>();
  }

  if (!ip_protection_proxy_config_fetcher_) {
    CHECK(aw_browser_context_);
    ip_protection_proxy_config_fetcher_ =
        std::make_unique<ip_protection::IpProtectionProxyConfigFetcher>(
            aw_browser_context_->GetDefaultStoragePartition()
                ->GetURLLoaderFactoryForBrowserProcess()
                .get(),
            ip_protection::IpProtectionConfigProviderHelper::kWebViewIpBlinding,
            GetAPIKey());
  }

  if (!bsa_) {
    if (!blind_sign_auth_) {
      privacy::ppn::BlindSignAuthOptions bsa_options{};
      bsa_options.set_enable_privacy_pass(true);

      blind_sign_auth_ = std::make_unique<quiche::BlindSignAuth>(
          blind_sign_message_android_impl_.get(), std::move(bsa_options));
    }
    bsa_ = blind_sign_auth_.get();
  }
}

void AwIpProtectionConfigProvider::SetUpForTesting(
    std::unique_ptr<ip_protection::IpProtectionProxyConfigRetriever>
        ip_protection_proxy_config_retriever,
    std::unique_ptr<ip_protection::BlindSignMessageAndroidImpl>
        blind_sign_message_android_impl,
    quiche::BlindSignAuthInterface* bsa) {
  // Carefully destroy any existing values in the correct order.
  bsa_ = nullptr;
  blind_sign_auth_ = nullptr;
  blind_sign_message_android_impl_ = nullptr;
  ip_protection_proxy_config_fetcher_ = nullptr;

  ip_protection_proxy_config_fetcher_ =
      std::make_unique<ip_protection::IpProtectionProxyConfigFetcher>(
          std::move(ip_protection_proxy_config_retriever));
  blind_sign_message_android_impl_ = std::move(blind_sign_message_android_impl);
  bsa_ = bsa;
}

void AwIpProtectionConfigProvider::GetProxyList(GetProxyListCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(!is_shutting_down_);
  SetUp();

  // If IP Protection is disabled then don't attempt to get a proxy list.
  if (!IsIpProtectionEnabled()) {
    std::move(callback).Run(/*proxy_chains=*/std::nullopt,
                            /*geo_hint=*/nullptr);
    return;
  }

  ip_protection_proxy_config_fetcher_->CallGetProxyConfig(
      std::move(callback), /*oauth_token=*/std::nullopt);
}

void AwIpProtectionConfigProvider::TryGetAuthTokens(
    uint32_t batch_size,
    network::mojom::IpProtectionProxyLayer proxy_layer,
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

  // If IP Protection is disabled then don't attempt to fetch tokens.
  if (!IsIpProtectionEnabled()) {
    TryGetAuthTokensComplete(
        /*bsa_tokens=*/std::nullopt, std::move(callback),
        AwIpProtectionTryGetAuthTokensResult::kFailedDisabled);
    return;
  }

  FetchBlindSignedToken(base::checked_cast<int>(batch_size), proxy_layer,
                        std::move(callback));
}

void AwIpProtectionConfigProvider::FetchBlindSignedToken(
    int batch_size,
    network::mojom::IpProtectionProxyLayer proxy_layer,
    TryGetAuthTokensCallback callback) {
  auto bsa_get_tokens_start_time = base::TimeTicks::Now();
  auto quiche_proxy_layer =
      proxy_layer == network::mojom::IpProtectionProxyLayer::kProxyA
          ? quiche::ProxyLayer::kProxyA
          : quiche::ProxyLayer::kProxyB;
  bsa_->GetTokens(
      /*oauth_token=*/std::nullopt, batch_size, quiche_proxy_layer,
      quiche::BlindSignAuthServiceType::kWebviewIpBlinding,
      [weak_ptr = weak_ptr_factory_.GetWeakPtr(), bsa_get_tokens_start_time,
       callback = std::move(callback)](
          absl::StatusOr<absl::Span<quiche::BlindSignToken>> tokens) mutable {
        if (weak_ptr) {
          weak_ptr->OnFetchBlindSignedTokenCompleted(
              bsa_get_tokens_start_time, std::move(callback), tokens);
        }
      });
}

void AwIpProtectionConfigProvider::OnFetchBlindSignedTokenCompleted(
    base::TimeTicks bsa_get_tokens_start_time,
    TryGetAuthTokensCallback callback,
    absl::StatusOr<absl::Span<quiche::BlindSignToken>> tokens) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_shutting_down_) {
    return;
  }
  if (!tokens.ok()) {
    AwIpProtectionTryGetAuthTokensResult result;
    switch (tokens.status().code()) {
      case absl::StatusCode::kUnavailable:
        result = AwIpProtectionTryGetAuthTokensResult::kFailedBSATransient;
        break;
      case absl::StatusCode::kFailedPrecondition:
        result = AwIpProtectionTryGetAuthTokensResult::kFailedBSAPersistent;
        break;
      default:
        result = AwIpProtectionTryGetAuthTokensResult::kFailedBSAOther;
        break;
    }
    VLOG(2) << "AwIpProtectionConfigProvider::OnFetchBlindSignedTokenCompleted "
               "got an error: "
            << static_cast<int>(result);
    TryGetAuthTokensComplete(/*bsa_tokens=*/std::nullopt, std::move(callback),
                             result);
    return;
  }

  if (tokens.value().size() == 0) {
    VLOG(2) << "AwIpProtectionConfigProvider::"
               "OnFetchBlindSignedTokenCompleted called with no tokens";
    TryGetAuthTokensComplete(
        /*bsa_tokens=*/std::nullopt, std::move(callback),
        AwIpProtectionTryGetAuthTokensResult::kFailedBSAOther);
    return;
  }

  std::vector<network::mojom::BlindSignedAuthTokenPtr> bsa_tokens;
  for (const quiche::BlindSignToken& token : tokens.value()) {
    network::mojom::BlindSignedAuthTokenPtr converted_token = ip_protection::
        IpProtectionConfigProviderHelper::CreateBlindSignedAuthToken(token);
    if (converted_token.is_null() || converted_token->token.empty()) {
      VLOG(2) << "AwIpProtectionConfigProvider::"
                 "OnFetchBlindSignedTokenCompleted failed to convert "
                 "`quiche::BlindSignAuth` token to a "
                 "`network::mojom::BlindSignedAuthToken`";
      TryGetAuthTokensComplete(
          /*bsa_tokens=*/std::nullopt, std::move(callback),
          AwIpProtectionTryGetAuthTokensResult::kFailedBSAOther);
      return;
    }
    bsa_tokens.push_back(std::move(converted_token));
  }

  const base::TimeTicks current_time = base::TimeTicks::Now();
  base::UmaHistogramTimes("NetworkService.AwIpProtection.TokenBatchRequestTime",
                          current_time - bsa_get_tokens_start_time);

  TryGetAuthTokensComplete(std::move(bsa_tokens), std::move(callback),
                           AwIpProtectionTryGetAuthTokensResult::kSuccess);
}

void AwIpProtectionConfigProvider::TryGetAuthTokensComplete(
    std::optional<std::vector<network::mojom::BlindSignedAuthTokenPtr>>
        bsa_tokens,
    TryGetAuthTokensCallback callback,
    AwIpProtectionTryGetAuthTokensResult result) {
  if (result == AwIpProtectionTryGetAuthTokensResult::kSuccess) {
    CHECK(bsa_tokens.has_value() && !bsa_tokens->empty());
  }

  base::UmaHistogramEnumeration(
      "NetworkService.AwIpProtection.TryGetAuthTokensResult", result);

  std::optional<base::TimeDelta> backoff = CalculateBackoff(result);
  std::optional<base::Time> try_again_after;
  if (backoff) {
    if (*backoff == base::TimeDelta::Max()) {
      try_again_after = base::Time::Max();
    } else {
      try_again_after = base::Time::Now() + *backoff;
    }
  }
  DCHECK(bsa_tokens.has_value() || try_again_after.has_value());
  std::move(callback).Run(std::move(bsa_tokens), try_again_after);
}

std::optional<base::TimeDelta> AwIpProtectionConfigProvider::CalculateBackoff(
    AwIpProtectionTryGetAuthTokensResult result) {
  std::optional<base::TimeDelta> backoff;
  switch (result) {
    case AwIpProtectionTryGetAuthTokensResult::kSuccess:
      break;
    case AwIpProtectionTryGetAuthTokensResult::kFailedBSAPersistent:
    case AwIpProtectionTryGetAuthTokensResult::kFailedDisabled:
      backoff = base::TimeDelta::Max();
      break;
    case AwIpProtectionTryGetAuthTokensResult::kFailedBSATransient:
    case AwIpProtectionTryGetAuthTokensResult::kFailedBSAOther:
      backoff =
          ip_protection::IpProtectionConfigProviderHelper::kTransientBackoff;
      // Note that we calculate the backoff assuming that we've waited for
      // `last_try_get_auth_tokens_backoff_` time already, but this may not be
      // the case when:
      //  - Concurrent calls to `TryGetAuthTokens` from two network contexts are
      //  made and both fail in the same way
      //
      //  - The network service restarts (the new network context(s) won't know
      //  to backoff until after the first request(s))
      //
      // We can't do much about the first case, but for the others we could
      // track the backoff time here and not request tokens again until
      // afterward.
      if (last_try_get_auth_tokens_backoff_ &&
          last_try_get_auth_tokens_result_ == result) {
        backoff = *last_try_get_auth_tokens_backoff_ * 2;
      }
      break;
  }
  last_try_get_auth_tokens_result_ = result;
  last_try_get_auth_tokens_backoff_ = backoff;
  return backoff;
}

void AwIpProtectionConfigProvider::Shutdown() {
  if (is_shutting_down_) {
    return;
  }
  is_shutting_down_ = true;
  receivers_.Clear();

  aw_browser_context_ = nullptr;
  bsa_ = nullptr;
  blind_sign_auth_ = nullptr;
}

// static
AwIpProtectionConfigProvider* AwIpProtectionConfigProvider::Get(
    AwBrowserContext* aw_browser_context) {
  return AwIpProtectionConfigProviderFactory::GetForAwBrowserContext(
      aw_browser_context);
}

void AwIpProtectionConfigProvider::AddNetworkService(
    mojo::PendingReceiver<network::mojom::IpProtectionConfigGetter>
        pending_receiver,
    mojo::PendingRemote<network::mojom::IpProtectionProxyDelegate>
        pending_remote) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(!is_shutting_down_);
  receivers_.Add(this, std::move(pending_receiver));
  remotes_.Add(std::move(pending_remote));
}

// static
bool AwIpProtectionConfigProvider::CanIpProtectionBeEnabled() {
  return base::FeatureList::IsEnabled(net::features::kEnableIpProtectionProxy);
}

// TODO(b/335420700): Update to return feature flag.
bool AwIpProtectionConfigProvider::IsIpProtectionEnabled() {
  if (is_shutting_down_) {
    return false;
  }
  return CanIpProtectionBeEnabled();
}

}  // namespace android_webview
