// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resolve_host_client_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/common/task_annotator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/base/request_priority.h"
#include "net/dns/public/resolve_error_info.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace predictors {

ResolveHostClientImpl::ResolveHostClientImpl(
    const GURL& url,
    const net::NetworkAnonymizationKey& network_anonymization_key,
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
  // Intentionally using a SchemeHostPort. Resolving http:// scheme host will
  // fail when a HTTPS resource record exists due to DNS-based scheme upgrade
  // functionality.
  network_context->ResolveHost(
      network::mojom::HostResolverHost::NewSchemeHostPort(
          url::SchemeHostPort(url)),
      network_anonymization_key, std::move(parameters),
      receiver_.BindNewPipeAndPassRemote());
  receiver_.set_disconnect_handler(base::BindOnce(
      &ResolveHostClientImpl::OnConnectionError, base::Unretained(this)));
}

ResolveHostClientImpl::~ResolveHostClientImpl() = default;

void ResolveHostClientImpl::OnComplete(
    int result,
    const net::ResolveErrorInfo& resolve_error_info,
    const std::optional<net::AddressList>& resolved_addresses,
    const std::optional<net::HostResolverEndpointResults>&
        endpoint_results_with_metadata) {
  UMA_HISTOGRAM_TIMES("Navigation.Preconnect.ResolveHostLatency",
                      base::TimeTicks::Now() - resolve_host_start_time_);

  auto* task = base::TaskAnnotator::CurrentTaskForThread();
  // As this method is executed as a callback from a Mojo call, it should be
  // executed via RunTask() and thus have a non-delayed PendingTask associated
  // with it.
  DCHECK(task);
  DCHECK(task->delayed_run_time.is_null());

  // The task will have a null |queue_time| if run synchronously (this happens
  // in unit tests, for example).
  base::TimeTicks queue_time =
      !task->queue_time.is_null() ? task->queue_time : base::TimeTicks::Now();
  UMA_HISTOGRAM_TIMES("Navigation.Preconnect.ResolveHostCallbackQueueingTime",
                      base::TimeTicks::Now() - queue_time);

  std::move(callback_).Run(result == net::OK);
}

void ResolveHostClientImpl::OnConnectionError() {
  std::move(callback_).Run(false);
}

}  // namespace predictors
