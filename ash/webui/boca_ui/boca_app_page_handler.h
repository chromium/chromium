// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_BOCA_UI_BOCA_APP_PAGE_HANDLER_H_
#define ASH_WEBUI_BOCA_UI_BOCA_APP_PAGE_HANDLER_H_

#include "ash/webui/boca_ui/mojom/boca.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

class BocaUI;

class BocaAppHandler : public boca::mojom::PageHandler {
 public:
  BocaAppHandler(BocaUI* boca_ui,
                 mojo::PendingReceiver<boca::mojom::PageHandler> receiver,
                 mojo::PendingRemote<boca::mojom::Page> remote);

  BocaAppHandler(const BocaAppHandler&) = delete;
  BocaAppHandler& operator=(const BocaAppHandler&) = delete;

  ~BocaAppHandler() override;

  // boca::mojom::PageHandler:

 private:
  mojo::Receiver<boca::mojom::PageHandler> receiver_;
  mojo::Remote<boca::mojom::Page> remote_;
  raw_ptr<BocaUI> boca_ui_;  // Owns |this|.
};
}  // namespace ash

#endif  // ASH_WEBUI_BOCA_UI_BOCA_APP_PAGE_HANDLER_H_
