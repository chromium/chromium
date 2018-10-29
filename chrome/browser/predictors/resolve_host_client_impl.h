// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_RESOLVE_HOST_CLIENT_IMPL_H_
#define CHROME_BROWSER_PREDICTORS_RESOLVE_HOST_CLIENT_IMPL_H_

#include "base/bind.h"
#include "base/macros.h"
#include "base/optional.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "services/network/public/mojom/host_resolver.mojom.h"

class GURL;

namespace network {
namespace mojom {
class NetworkContext;
}
}  // namespace network

namespace predictors {

using ResolveHostCallback = base::OnceCallback<void(bool success)>;

// This class helps perform the host resolution using the NetworkContext.
// An instance of this class must be deleted after the callback is invoked.
class ResolveHostClientImpl : public network::mojom::ResolveHostClient {
 public:
  // Starts the host resolution for |url|. |callback| is called when the host is
  // resolved or when an error occurs.
  ResolveHostClientImpl(const GURL& url,
                        ResolveHostCallback callback,
                        network::mojom::NetworkContext* network_context);
  // Cancels the request if it hasn't been completed yet.
  ~ResolveHostClientImpl() override;

  void Cancel();

  // network::mojom::ResolveHostClient:
  void OnComplete(
      int result,
      const base::Optional<net::AddressList>& resolved_addresses) override;

  void OnConnectionError();

 private:
  mojo::Binding<network::mojom::ResolveHostClient> binding_;
  network::mojom::ResolveHostHandlePtr control_handle_;
  ResolveHostCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(ResolveHostClientImpl);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_RESOLVE_HOST_CLIENT_IMPL_H_
