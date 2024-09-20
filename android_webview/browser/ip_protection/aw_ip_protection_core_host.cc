// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/ip_protection/aw_ip_protection_core_host.h"

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
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "components/ip_protection/android/ip_protection_token_ipc_fetcher.h"
#include "components/ip_protection/common/ip_protection_core_host_helper.h"
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
    : aw_browser_context_(aw_browser_context),
      token_fetcher_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}

AwIpProtectionCoreHost::~AwIpProtectionCoreHost() = default;

void AwIpProtectionCoreHost::SetUp() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!ip_protection_token_ipc_fetcher_) {
    ip_protection_token_ipc_fetcher_ =
        base::SequenceBound<ip_protection::IpProtectionTokenIpcFetcher>(
            token_fetcher_task_runner_);
  }

  if (!ip_protection_proxy_config_fetcher_) {
    CHECK(aw_browser_context_);
    ip_protection_proxy_config_fetcher_ =
        std::make_unique<ip_protection::IpProtectionProxyConfigDirectFetcher>(
            aw_browser_context_->GetDefaultStoragePartition()
                ->GetURLLoaderFactoryForBrowserProcess()
                .get(),
            ip_protection::IpProtectionCoreHostHelper::kWebViewIpBlinding,
            base::BindRepeating(&AwIpProtectionCoreHost::AuthenticateCallback,
                                weak_ptr_factory_.GetWeakPtr()));
  }
}

void AwIpProtectionCoreHost::SetUpForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<quiche::BlindSignAuthInterface> bsa) {
  for_testing_ = true;

  // Carefully destroy any existing values in the correct order.
  ip_protection_token_ipc_fetcher_.Reset();
  ip_protection_proxy_config_fetcher_ = nullptr;

  ip_protection_token_ipc_fetcher_ =
      base::SequenceBound<ip_protection::IpProtectionTokenIpcFetcher>(
          token_fetcher_task_runner_, std::move(bsa));
  ip_protection_proxy_config_fetcher_ =
      std::make_unique<ip_protection::IpProtectionProxyConfigDirectFetcher>(
          std::move(url_loader_factory),
          ip_protection::IpProtectionCoreHostHelper::kWebViewIpBlinding,
          base::BindRepeating(&AwIpProtectionCoreHost::AuthenticateCallback,
                              weak_ptr_factory_.GetWeakPtr()));
}

void AwIpProtectionCoreHost::GetProxyList(GetProxyListCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(!is_shutting_down_);
  SetUp();

  // If IP Protection is disabled then don't attempt to get a proxy list.
  if (!IsIpProtectionEnabled()) {
    std::move(callback).Run(/*proxy_chains=*/std::nullopt,
                            /*geo_hint=*/std::nullopt);
    return;
  }

  ip_protection_proxy_config_fetcher_->GetProxyConfig(std::move(callback));
}

void AwIpProtectionCoreHost::AuthenticateCallback(
    std::unique_ptr<network::ResourceRequest> resource_request,
    ip_protection::IpProtectionProxyConfigDirectFetcher::
        AuthenticateDoneCallback callback) {
  google_apis::AddAPIKeyToRequest(
      *resource_request,
      google_apis::GetAPIKey(version_info::android::GetChannel()));
  std::move(callback).Run(true, std::move(resource_request));
}

void AwIpProtectionCoreHost::TryGetAuthTokens(
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
        TryGetAuthTokensAndroidResult::kFailedDisabled);
    return;
  }

  auto quiche_proxy_layer =
      proxy_layer == network::mojom::IpProtectionProxyLayer::kProxyA
          ? quiche::ProxyLayer::kProxyA
          : quiche::ProxyLayer::kProxyB;
  FetchBlindSignedToken(base::checked_cast<int>(batch_size), quiche_proxy_layer,
                        std::move(callback));
}

void AwIpProtectionCoreHost::FetchBlindSignedToken(
    int batch_size,
    quiche::ProxyLayer quiche_proxy_layer,
    TryGetAuthTokensCallback callback) {
  auto bsa_get_tokens_start_time = base::TimeTicks::Now();
  ip_protection_token_ipc_fetcher_
      .AsyncCall(
          &ip_protection::IpProtectionTokenIpcFetcher::FetchBlindSignedToken)
      .WithArgs(
          /*access_token=*/std::nullopt, batch_size, quiche_proxy_layer,
          base::BindPostTaskToCurrentDefault(base::BindOnce(
              &AwIpProtectionCoreHost::OnFetchBlindSignedTokenCompleted,
              weak_ptr_factory_.GetWeakPtr(), bsa_get_tokens_start_time,
              std::move(callback))));
}

void AwIpProtectionCoreHost::OnFetchBlindSignedTokenCompleted(
    base::TimeTicks bsa_get_tokens_start_time,
    TryGetAuthTokensCallback callback,
    absl::StatusOr<std::vector<quiche::BlindSignToken>> tokens) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_shutting_down_) {
    return;
  }
  if (!tokens.ok()) {
    TryGetAuthTokensAndroidResult result;
    switch (tokens.status().code()) {
      case absl::StatusCode::kUnavailable:
        result = TryGetAuthTokensAndroidResult::kFailedBSATransient;
        break;
      case absl::StatusCode::kFailedPrecondition:
        result = TryGetAuthTokensAndroidResult::kFailedBSAPersistent;
        break;
      default:
        result = TryGetAuthTokensAndroidResult::kFailedBSAOther;
        break;
    }
    VLOG(2) << "AwIpProtectionCoreHost::OnFetchBlindSignedTokenCompleted "
               "got an error: "
            << static_cast<int>(result);
    TryGetAuthTokensComplete(/*bsa_tokens=*/std::nullopt, std::move(callback),
                             result);
    return;
  }

  if (tokens.value().size() == 0) {
    VLOG(2) << "AwIpProtectionCoreHost::"
               "OnFetchBlindSignedTokenCompleted called with no tokens";
    TryGetAuthTokensComplete(
        /*bsa_tokens=*/std::nullopt, std::move(callback),
        TryGetAuthTokensAndroidResult::kFailedBSAOther);
    return;
  }

  std::vector<ip_protection::BlindSignedAuthToken> bsa_tokens;
  for (const quiche::BlindSignToken& token : tokens.value()) {
    std::optional<ip_protection::BlindSignedAuthToken> converted_token =
        ip_protection::IpProtectionCoreHostHelper::
            CreateBlindSignedAuthToken(token);
    if (!converted_token.has_value() || converted_token->token.empty()) {
      VLOG(2) << "AwIpProtectionCoreHost::"
                 "OnFetchBlindSignedTokenCompleted failed to convert "
                 "`quiche::BlindSignAuth` token to a "
                 "`network::mojom::BlindSignedAuthToken`";
      TryGetAuthTokensComplete(
          /*bsa_tokens=*/std::nullopt, std::move(callback),
          TryGetAuthTokensAndroidResult::kFailedBSAOther);
      return;
    } else {
      bsa_tokens.push_back(std::move(converted_token).value());
    }
  }

  const base::TimeTicks current_time = base::TimeTicks::Now();
  TryGetAuthTokensComplete(std::move(bsa_tokens), std::move(callback),
                           TryGetAuthTokensAndroidResult::kSuccess,
                           current_time - bsa_get_tokens_start_time);
}

void AwIpProtectionCoreHost::TryGetAuthTokensComplete(
    std::optional<std::vector<ip_protection::BlindSignedAuthToken>> bsa_tokens,
    TryGetAuthTokensCallback callback,
    ip_protection::TryGetAuthTokensAndroidResult result,
    std::optional<base::TimeDelta> duration) {
  if (result == TryGetAuthTokensAndroidResult::kSuccess) {
    CHECK(bsa_tokens.has_value() && !bsa_tokens->empty());
  }

  ip_protection::Telemetry().AndroidTokenBatchFetchComplete(result, duration);

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

std::optional<base::TimeDelta> AwIpProtectionCoreHost::CalculateBackoff(
    TryGetAuthTokensAndroidResult result) {
  std::optional<base::TimeDelta> backoff;
  switch (result) {
    case TryGetAuthTokensAndroidResult::kSuccess:
      break;
    case TryGetAuthTokensAndroidResult::kFailedBSAPersistent:
    case TryGetAuthTokensAndroidResult::kFailedDisabled:
      backoff = base::TimeDelta::Max();
      break;
    case TryGetAuthTokensAndroidResult::kFailedBSATransient:
    case TryGetAuthTokensAndroidResult::kFailedBSAOther:
      backoff =
          ip_protection::IpProtectionCoreHostHelper::kTransientBackoff;
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

void AwIpProtectionCoreHost::Shutdown() {
  if (is_shutting_down_) {
    return;
  }
  is_shutting_down_ = true;
  receivers_.Clear();

  ip_protection_token_ipc_fetcher_.Reset();
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
    mojo::PendingReceiver<network::mojom::IpProtectionConfigGetter>
        pending_receiver,
    mojo::PendingRemote<network::mojom::IpProtectionControl> pending_remote) {
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

}  // namespace android_webview
