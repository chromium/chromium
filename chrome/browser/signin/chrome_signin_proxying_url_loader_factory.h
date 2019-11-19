// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_PROXYING_URL_LOADER_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_PROXYING_URL_LOADER_FACTORY_H_

#include "base/callback.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

#include <memory>
#include <set>

namespace content {
class RenderFrameHost;
}

namespace signin {

class HeaderModificationDelegate;

// This class is used to modify sub-resource requests made by the renderer
// that is displaying the GAIA signin realm, to the GAIA signin realm. When
// such a request is made a proxy is inserted between the renderer and the
// Network Service to modify request and response headers.
class ProxyingURLLoaderFactory : public network::mojom::URLLoaderFactory {
 public:
  using DisconnectCallback =
      base::OnceCallback<void(ProxyingURLLoaderFactory*)>;

  // Constructor public for testing purposes. New instances should be created
  // by calling MaybeProxyRequest().
  ProxyingURLLoaderFactory(
      std::unique_ptr<HeaderModificationDelegate> delegate,
      content::WebContents::Getter web_contents_getter,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory,
      DisconnectCallback on_disconnect);
  ~ProxyingURLLoaderFactory() override;

  // Called when a renderer needs a URLLoaderFactory to give this module the
  // opportunity to install a proxy. This is only done when
  // https://accounts.google.com is loaded in non-incognito mode. Returns true
  // when |factory_request| has been proxied.
  static bool MaybeProxyRequest(
      content::RenderFrameHost* render_frame_host,
      bool is_navigation,
      const url::Origin& request_initiator,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>*
          factory_receiver);

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory>
                 loader_receiver) override;

 private:
  friend class base::DeleteHelper<ProxyingURLLoaderFactory>;
  friend class base::RefCountedDeleteOnSequence<ProxyingURLLoaderFactory>;

  class InProgressRequest;
  class ProxyRequestAdapter;
  class ProxyResponseAdapter;

  void OnTargetFactoryError();
  void OnProxyBindingError();
  void RemoveRequest(InProgressRequest* request);
  void MaybeDestroySelf();

  std::unique_ptr<HeaderModificationDelegate> delegate_;
  content::WebContents::Getter web_contents_getter_;

  mojo::ReceiverSet<network::mojom::URLLoaderFactory> proxy_receivers_;
  std::set<std::unique_ptr<InProgressRequest>, base::UniquePtrComparator>
      requests_;
  mojo::Remote<network::mojom::URLLoaderFactory> target_factory_;
  DisconnectCallback on_disconnect_;

  DISALLOW_COPY_AND_ASSIGN(ProxyingURLLoaderFactory);
};

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_PROXYING_URL_LOADER_FACTORY_H_
