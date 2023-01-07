// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/digital_goods/digital_goods_factory_stub.h"

#include <utility>

#include "base/no_destructor.h"

namespace apps {

DigitalGoodsFactoryStub::DigitalGoodsFactoryStub() = default;
DigitalGoodsFactoryStub::~DigitalGoodsFactoryStub() = default;

// static
void DigitalGoodsFactoryStub::Bind(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<payments::mojom::DigitalGoodsFactory> receiver) {
  static base::NoDestructor<DigitalGoodsFactoryStub> instance;
  instance->HandleBind(std::move(receiver));
}

void DigitalGoodsFactoryStub::CreateDigitalGoods(
    const std::string& payment_method,
    CreateDigitalGoodsCallback callback) {
  std::move(callback).Run(
      payments::mojom::CreateDigitalGoodsResponseCode::kError,
      /*digital_goods=*/mojo::NullRemote());
}

void DigitalGoodsFactoryStub::HandleBind(
    mojo::PendingReceiver<payments::mojom::DigitalGoodsFactory> receiver) {
  receiver_set_.Add(this, std::move(receiver));
}

}  // namespace apps
