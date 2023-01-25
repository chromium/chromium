// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_NETWORK_HINTS_HANDLER_IMPL_H_
#define CHROME_BROWSER_PREDICTORS_NETWORK_HINTS_HANDLER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "components/network_hints/common/network_hints.mojom.h"

namespace content {
class RenderFrameHost;
}

namespace predictors {
class PreconnectManager;

class NetworkHintsHandlerImpl
    : public network_hints::mojom::NetworkHintsHandler {
 public:
  NetworkHintsHandlerImpl(const NetworkHintsHandlerImpl&) = delete;
  NetworkHintsHandlerImpl& operator=(const NetworkHintsHandlerImpl&) = delete;

  ~NetworkHintsHandlerImpl() override;

  static void Create(
      content::RenderFrameHost* frame_host,
      mojo::PendingReceiver<network_hints::mojom::NetworkHintsHandler>
          receiver);

  // network_hints::mojom::NetworkHintsHandler methods:
  void PrefetchDNS(const std::vector<url::SchemeHostPort>& urls) override;
  void Preconnect(const url::SchemeHostPort& url,
                  bool allow_credentials) override;

 private:
  explicit NetworkHintsHandlerImpl(content::RenderFrameHost* frame_host);

  const int32_t render_process_id_;
  const int32_t render_frame_id_;
  base::WeakPtr<PreconnectManager> preconnect_manager_;
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_NETWORK_HINTS_HANDLER_IMPL_H_
