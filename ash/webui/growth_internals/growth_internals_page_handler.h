// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_GROWTH_INTERNALS_GROWTH_INTERNALS_PAGE_HANDLER_H_
#define ASH_WEBUI_GROWTH_INTERNALS_GROWTH_INTERNALS_PAGE_HANDLER_H_

#include "ash/webui/growth_internals/growth_internals.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {

class GrowthInternalsPageHandler : public growth::mojom::PageHandler {
 public:
  explicit GrowthInternalsPageHandler(
      mojo::PendingReceiver<growth::mojom::PageHandler> pending_page_handler);
  ~GrowthInternalsPageHandler() override;

  // mojom::PageHandler:
  void GetCampaignsLogs(GetCampaignsLogsCallback callback) override;

 private:
  mojo::Receiver<growth::mojom::PageHandler> page_handler_;
};

}  // namespace ash

#endif  // ASH_WEBUI_GROWTH_INTERNALS_GROWTH_INTERNALS_PAGE_HANDLER_H_
