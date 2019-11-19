// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/proxy_lookup_client_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "net/proxy_resolution/proxy_info.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace predictors {

ProxyLookupClientImpl::ProxyLookupClientImpl(
    const GURL& url,
    ProxyLookupCallback callback,
    network::mojom::NetworkContext* network_context)
    : callback_(std::move(callback)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  network_context->LookUpProxyForURL(
      url,
      receiver_.BindNewPipeAndPassRemote(base::CreateSingleThreadTaskRunner(
          {content::BrowserThread::UI,
           content::BrowserTaskType::kPreconnect})));
  receiver_.set_disconnect_handler(
      base::BindOnce(&ProxyLookupClientImpl::OnProxyLookupComplete,
                     base::Unretained(this), net::ERR_ABORTED, base::nullopt));
}

ProxyLookupClientImpl::~ProxyLookupClientImpl() = default;

void ProxyLookupClientImpl::OnProxyLookupComplete(
    int32_t net_error,
    const base::Optional<net::ProxyInfo>& proxy_info) {
  bool success = proxy_info.has_value() && !proxy_info->is_direct();
  std::move(callback_).Run(success);
}

}  // namespace predictors
