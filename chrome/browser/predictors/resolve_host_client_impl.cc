// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resolve_host_client_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/base/request_priority.h"
#include "net/dns/public/resolve_error_info.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace predictors {

ResolveHostClientImpl::ResolveHostClientImpl(
    const GURL& url,
    const net::NetworkIsolationKey& network_isolation_key,
    ResolveHostCallback callback,
    network::mojom::NetworkContext* network_context)
    : callback_(std::move(callback)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  network::mojom::ResolveHostParametersPtr parameters =
      network::mojom::ResolveHostParameters::New();
  parameters->initial_priority = net::RequestPriority::IDLE;
  parameters->is_speculative = true;
  parameters->purpose =
      network::mojom::ResolveHostParameters::Purpose::kPreconnect;
  resolve_host_start_time_ = base::TimeTicks::Now();
  network_context->ResolveHost(
      net::HostPortPair::FromURL(url), network_isolation_key,
      std::move(parameters),
      receiver_.BindNewPipeAndPassRemote(content::GetUIThreadTaskRunner(
          {content::BrowserTaskType::kPreconnect})));
  receiver_.set_disconnect_handler(base::BindOnce(
      &ResolveHostClientImpl::OnConnectionError, base::Unretained(this)));
}

ResolveHostClientImpl::~ResolveHostClientImpl() = default;

void ResolveHostClientImpl::OnComplete(
    int result,
    const net::ResolveErrorInfo& resolve_error_info,
    const absl::optional<net::AddressList>& resolved_addresses) {
  UMA_HISTOGRAM_TIMES("Navigation.Preconnect.ResolveHostLatency",
                      base::TimeTicks::Now() - resolve_host_start_time_);
  std::move(callback_).Run(result == net::OK);
}

void ResolveHostClientImpl::OnConnectionError() {
  std::move(callback_).Run(false);
}

}  // namespace predictors
