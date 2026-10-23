// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_URL_LOADER_THROTTLE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_URL_LOADER_THROTTLE_H_

#include <memory>

#include "base/functional/callback.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/self_deleting_url_loader_factory.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

class GURL;
class Profile;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace net {
class IsolationInfo;
}  // namespace net

namespace network {
class URLLoaderFactoryBuilder;
}  // namespace network

namespace contextual_tasks {

// Throttle that injects the Chrome-Search-Capabilities-Version custom HTTP
// header on requests to Google-associated domains when the Contextual Tasks
// rearchitecture is enabled.
class ContextualTasksURLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  static std::unique_ptr<ContextualTasksURLLoaderThrottle> MaybeCreate(
      Profile* profile,
      const base::RepeatingCallback<content::WebContents*()>& wc_getter = {});

  ContextualTasksURLLoaderThrottle();
  ~ContextualTasksURLLoaderThrottle() override;

  ContextualTasksURLLoaderThrottle(const ContextualTasksURLLoaderThrottle&) =
      delete;
  ContextualTasksURLLoaderThrottle& operator=(
      const ContextualTasksURLLoaderThrottle&) = delete;

  // blink::URLLoaderThrottle:
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      network::HttpRequestHeadersUpdateParams* headers_update_params) override;

  static bool ShouldAppendHeader(const GURL& url);
};

// Proxying `URLLoaderFactory` that injects the
// Chrome-Search-Capabilities-Version custom HTTP header on subresource
// requests (fetch, XHR, etc.) to Google-associated domains when the Contextual
// Tasks rearchitecture is enabled and the top-level frame also has the header.
class ContextualTasksSubresourceProxyingURLLoaderFactory
    : public network::SelfDeletingURLLoaderFactory {
 public:
  ContextualTasksSubresourceProxyingURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          target_factory_remote,
      base::SelfDeletingPassKey pass_key);

  ContextualTasksSubresourceProxyingURLLoaderFactory(
      const ContextualTasksSubresourceProxyingURLLoaderFactory&) = delete;
  ContextualTasksSubresourceProxyingURLLoaderFactory& operator=(
      const ContextualTasksSubresourceProxyingURLLoaderFactory&) = delete;

  // Conditionally appends a proxying factory to `factory_builder` if
  // `browser_context` is non-null, eligible, and `isolation_info`'s top frame
  // origin matches an eligible Google URL. If `browser_context` is null or
  // ineligible, or the top frame origin is ineligible, `factory_builder` is
  // left unmodified.
  static void MaybeProxyRequest(
      content::BrowserContext* browser_context,
      const net::IsolationInfo& isolation_info,
      network::URLLoaderFactoryBuilder& factory_builder);

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;

 private:
  ~ContextualTasksSubresourceProxyingURLLoaderFactory() override;
  void OnTargetFactoryError();

  mojo::Remote<network::mojom::URLLoaderFactory> target_factory_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_URL_LOADER_THROTTLE_H_
