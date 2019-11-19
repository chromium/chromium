// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_NETWORK_HINTS_HANDLER_IMPL_H_
#define CHROME_BROWSER_PREDICTORS_NETWORK_HINTS_HANDLER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "components/network_hints/common/network_hints.mojom.h"

namespace predictors {
class PreconnectManager;

class NetworkHintsHandlerImpl
    : public network_hints::mojom::NetworkHintsHandler {
 public:
  ~NetworkHintsHandlerImpl() override;

  static void Create(
      int32_t render_process_id,
      mojo::PendingReceiver<network_hints::mojom::NetworkHintsHandler>
          receiver);

  // network_hints::mojom::NetworkHintsHandler methods:
  void PrefetchDNS(const std::vector<std::string>& names) override;
  void Preconnect(int32_t render_frame_id,
                  const GURL& url,
                  bool allow_credentials) override;

 private:
  explicit NetworkHintsHandlerImpl(int32_t render_process_id);

  int32_t render_process_id_;
  base::WeakPtr<PreconnectManager> preconnect_manager_;

  DISALLOW_COPY_AND_ASSIGN(NetworkHintsHandlerImpl);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_NETWORK_HINTS_HANDLER_IMPL_H_
