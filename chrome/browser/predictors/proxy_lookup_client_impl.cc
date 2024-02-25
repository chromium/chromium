// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/proxy_lookup_client_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/common/task_annotator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/proxy_resolution/proxy_info.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace predictors {

ProxyLookupClientImpl::ProxyLookupClientImpl(
    const GURL& url,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    ProxyLookupCallback callback,
    network::mojom::NetworkContext* network_context)
    : callback_(std::move(callback)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  proxy_lookup_start_time_ = base::TimeTicks::Now();
  network_context->LookUpProxyForURL(url, network_anonymization_key,
                                     receiver_.BindNewPipeAndPassRemote());
  receiver_.set_disconnect_handler(
      base::BindOnce(&ProxyLookupClientImpl::OnProxyLookupComplete,
                     base::Unretained(this), net::ERR_ABORTED, std::nullopt));
}

ProxyLookupClientImpl::~ProxyLookupClientImpl() = default;

void ProxyLookupClientImpl::OnProxyLookupComplete(
    int32_t net_error,
    const std::optional<net::ProxyInfo>& proxy_info) {
  UMA_HISTOGRAM_TIMES("Navigation.Preconnect.ProxyLookupLatency",
                      base::TimeTicks::Now() - proxy_lookup_start_time_);

  // As this method is executed as a callback from a Mojo call, it should be
  // executed via RunTask() and thus have a non-delayed PendingTask associated
  // with it.
  auto* task = base::TaskAnnotator::CurrentTaskForThread();
  DCHECK(task);
  DCHECK(task->delayed_run_time.is_null());
  // The task will have a null |queue_time| if run synchronously (this happens
  // in unit tests, for example).
  base::TimeTicks queue_time =
      !task->queue_time.is_null() ? task->queue_time : base::TimeTicks::Now();
  UMA_HISTOGRAM_TIMES("Navigation.Preconnect.ProxyLookupCallbackQueueingTime",
                      base::TimeTicks::Now() - queue_time);

  bool success = proxy_info.has_value() && !proxy_info->is_direct();
  std::move(callback_).Run(success);
}

}  // namespace predictors
