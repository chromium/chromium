// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_DIGITAL_GOODS_DIGITAL_GOODS_FACTORY_IMPL_H_
#define CHROME_BROWSER_APPS_DIGITAL_GOODS_DIGITAL_GOODS_FACTORY_IMPL_H_

#include <string>

#include "content/public/browser/document_user_data.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/digital_goods/digital_goods.mojom.h"

namespace apps {

class DigitalGoodsFactoryImpl
    : public content::DocumentUserData<DigitalGoodsFactoryImpl>,
      public payments::mojom::DigitalGoodsFactory {
 public:
  ~DigitalGoodsFactoryImpl() override;

  static void BindDigitalGoodsFactory(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<payments::mojom::DigitalGoodsFactory> receiver);

  // payments::mojom::DigitalGoodsFactory overrides.
  void CreateDigitalGoods(const std::string& payment_method,
                          CreateDigitalGoodsCallback callback) override;

 private:
  explicit DigitalGoodsFactoryImpl(content::RenderFrameHost* render_frame_host);
  friend class content::DocumentUserData<DigitalGoodsFactoryImpl>;
  DOCUMENT_USER_DATA_KEY_DECL();

  void BindRequest(
      mojo::PendingReceiver<payments::mojom::DigitalGoodsFactory> receiver);

  mojo::Receiver<payments::mojom::DigitalGoodsFactory> receiver_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_DIGITAL_GOODS_DIGITAL_GOODS_FACTORY_IMPL_H_
