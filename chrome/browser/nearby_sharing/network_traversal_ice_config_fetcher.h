// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NETWORK_TRAVERSAL_ICE_CONFIG_FETCHER_H_
#define CHROME_BROWSER_NEARBY_SHARING_NETWORK_TRAVERSAL_ICE_CONFIG_FETCHER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "chromeos/ash/services/nearby/public/mojom/webrtc.mojom.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

class NetworkTraversalIceConfigFetcher
    : public ::sharing::mojom::IceConfigFetcher {
 public:
  explicit NetworkTraversalIceConfigFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~NetworkTraversalIceConfigFetcher() override;

  NetworkTraversalIceConfigFetcher(
      const NetworkTraversalIceConfigFetcher& other) = delete;
  NetworkTraversalIceConfigFetcher& operator=(
      const NetworkTraversalIceConfigFetcher& other) = delete;

  // TODO(crbug.com/40147375) - Cache configs fetched from server.
  void GetIceServers(GetIceServersCallback callback) override;

 private:
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NETWORK_TRAVERSAL_ICE_CONFIG_FETCHER_H_
