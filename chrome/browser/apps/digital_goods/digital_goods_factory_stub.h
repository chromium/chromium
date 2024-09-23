// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_DIGITAL_GOODS_DIGITAL_GOODS_FACTORY_STUB_H_
#define CHROME_BROWSER_APPS_DIGITAL_GOODS_DIGITAL_GOODS_FACTORY_STUB_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/digital_goods/digital_goods.mojom.h"

namespace content {
class RenderFrameHost;
}

namespace apps {

// A stub implementation that returns an error trying to create a digital goods
// instance.
// TODO(crbug.com/40179639): Remove when Lacros is permanently enabled.
class DigitalGoodsFactoryStub : public payments::mojom::DigitalGoodsFactory {
 public:
  DigitalGoodsFactoryStub();
  ~DigitalGoodsFactoryStub() override;

  static void Bind(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<payments::mojom::DigitalGoodsFactory> receiver);

  // payments::mojom::DigitalGoodsFactory overrides.
  void CreateDigitalGoods(const std::string& payment_method,
                          CreateDigitalGoodsCallback callback) override;

 private:
  void HandleBind(
      mojo::PendingReceiver<payments::mojom::DigitalGoodsFactory> receiver);

  mojo::ReceiverSet<payments::mojom::DigitalGoodsFactory> receiver_set_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_DIGITAL_GOODS_DIGITAL_GOODS_FACTORY_STUB_H_
