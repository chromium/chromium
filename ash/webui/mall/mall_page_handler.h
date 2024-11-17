// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_MALL_MALL_PAGE_HANDLER_H_
#define ASH_WEBUI_MALL_MALL_PAGE_HANDLER_H_

#include "ash/webui/mall/mall_ui.mojom.h"
#include "base/memory/raw_ref.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {

class MallUIDelegate;

class MallPageHandler : public mall::mojom::PageHandler {
 public:
  explicit MallPageHandler(
      mojo::PendingReceiver<mall::mojom::PageHandler> receiver,
      MallUIDelegate& delegate);
  MallPageHandler(const MallPageHandler&) = delete;
  MallPageHandler& operator=(const MallPageHandler&) = delete;
  ~MallPageHandler() override;

  void GetMallEmbedUrl(const std::string& path,
                       GetMallEmbedUrlCallback callback) override;

 private:
  mojo::Receiver<mall::mojom::PageHandler> receiver_;
  raw_ref<MallUIDelegate> delegate_;  // Owned by `MallUI`, which owns `this`.
};

}  // namespace ash

#endif  // ASH_WEBUI_MALL_MALL_PAGE_HANDLER_H_
