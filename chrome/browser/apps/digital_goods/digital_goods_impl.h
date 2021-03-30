// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_DIGITAL_GOODS_DIGITAL_GOODS_IMPL_H_
#define CHROME_BROWSER_APPS_DIGITAL_GOODS_DIGITAL_GOODS_IMPL_H_

#include <string>
#include <vector>

#include "components/arc/pay/arc_digital_goods_bridge.h"
#include "content/public/browser/render_document_host_user_data.h"
#include "content/public/browser/render_widget_host.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/digital_goods/digital_goods.mojom.h"

namespace apps {

class DigitalGoodsImpl
    : public content::RenderDocumentHostUserData<DigitalGoodsImpl>,
      public payments::mojom::DigitalGoods {
 public:
  ~DigitalGoodsImpl() override;

  static mojo::PendingRemote<payments::mojom::DigitalGoods> CreateAndBind(
      content::RenderFrameHost* render_frame_host);

  mojo::PendingRemote<payments::mojom::DigitalGoods> BindRequest();

  // payments::mojom::DigitalGoods overrides.
  void GetDetails(const std::vector<std::string>& item_ids,
                  GetDetailsCallback callback) override;
  void Acknowledge(const std::string& purchase_token,
                   bool make_available_again,
                   AcknowledgeCallback callback) override;
  void ListPurchases(ListPurchasesCallback callback) override;

 private:
  explicit DigitalGoodsImpl(content::RenderFrameHost* render_frame_host);
  friend class content::RenderDocumentHostUserData<DigitalGoodsImpl>;
  RENDER_DOCUMENT_HOST_USER_DATA_KEY_DECL();

  arc::ArcDigitalGoodsBridge* GetArcDigitalGoodsBridge();

  content::RenderFrameHost* render_frame_host_;
  mojo::Receiver<payments::mojom::DigitalGoods> receiver_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_DIGITAL_GOODS_DIGITAL_GOODS_IMPL_H_
