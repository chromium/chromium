// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_WEBRTC_ICE_CONFIG_FETCHER_H_
#define CHROME_BROWSER_SHARING_WEBRTC_ICE_CONFIG_FETCHER_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chromeos/services/nearby/public/mojom/webrtc.mojom.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

class IceConfigFetcher : public sharing::mojom::IceConfigFetcher {
 public:
  explicit IceConfigFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~IceConfigFetcher() override;

  IceConfigFetcher(const IceConfigFetcher& other) = delete;
  IceConfigFetcher& operator=(const IceConfigFetcher& other) = delete;

  // TODO(crbug.com/1124392) - Cache configs fetched from server.
  void GetIceServers(GetIceServersCallback callback) override;

 private:
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

#endif  // CHROME_BROWSER_SHARING_WEBRTC_ICE_CONFIG_FETCHER_H_
