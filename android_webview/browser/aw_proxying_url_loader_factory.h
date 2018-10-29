// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_PROXYING_URL_LOADER_FACTORY_H_
#define ANDROID_WEBVIEW_BROWSER_AW_PROXYING_URL_LOADER_FACTORY_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "net/base/completion_callback.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace android_webview {

class AwInterceptedRequestHandler {};

// URL Loader Factory for android webview, for supporting request/response
// interception, processing and callback invocation. Currently contains basic
// pass-through implementation.
class AwProxyingURLLoaderFactory : public network::mojom::URLLoaderFactory {
 public:
  AwProxyingURLLoaderFactory(
      int process_id,
      network::mojom::URLLoaderFactoryRequest loader_request,
      network::mojom::URLLoaderFactoryPtrInfo target_factory_info,
      std::unique_ptr<AwInterceptedRequestHandler> request_handler);

  ~AwProxyingURLLoaderFactory() override;

  // static
  static void CreateProxy(
      int process_id,
      network::mojom::URLLoaderFactoryRequest loader,
      network::mojom::URLLoaderFactoryPtrInfo target_factory_info,
      std::unique_ptr<AwInterceptedRequestHandler> request_handler);

  void CreateLoaderAndStart(network::mojom::URLLoaderRequest loader,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;

  void Clone(network::mojom::URLLoaderFactoryRequest loader_request) override;

 private:
  void OnTargetFactoryError();
  void OnProxyBindingError();

  const int process_id_;
  mojo::BindingSet<network::mojom::URLLoaderFactory> proxy_bindings_;
  network::mojom::URLLoaderFactoryPtr target_factory_;

  // TODO(timvolodine): consider functionality to have multiple interception
  // handlers operating in sequence.
  std::unique_ptr<AwInterceptedRequestHandler> request_handler_;
  base::WeakPtrFactory<AwProxyingURLLoaderFactory> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AwProxyingURLLoaderFactory);
};

}  // namespace android_webview

#endif
