// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_DIGITAL_GOODS_DIGITAL_GOODS_IMPL_H_
#define CHROME_BROWSER_APPS_DIGITAL_GOODS_DIGITAL_GOODS_IMPL_H_

#include <string>
#include <vector>

#include "ash/components/arc/pay/arc_digital_goods_bridge.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_widget_host.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/digital_goods/digital_goods.mojom.h"

namespace apps {

// TODO(crbug.com/40179639): Remove when Lacros is permanently enabled.
class DigitalGoodsImpl : public content::DocumentUserData<DigitalGoodsImpl>,
                         public payments::mojom::DigitalGoods {
 public:
  ~DigitalGoodsImpl() override;

  static mojo::PendingRemote<payments::mojom::DigitalGoods> CreateAndBind(
      content::RenderFrameHost* render_frame_host);

  mojo::PendingRemote<payments::mojom::DigitalGoods> BindRequest();

  // payments::mojom::DigitalGoods overrides.
  void GetDetails(const std::vector<std::string>& item_ids,
                  GetDetailsCallback callback) override;
  void ListPurchases(ListPurchasesCallback callback) override;
  void ListPurchaseHistory(ListPurchaseHistoryCallback callback) override;
  void Consume(const std::string& purchase_token,
               ConsumeCallback callback) override;

 private:
  explicit DigitalGoodsImpl(content::RenderFrameHost* render_frame_host);
  friend class content::DocumentUserData<DigitalGoodsImpl>;
  DOCUMENT_USER_DATA_KEY_DECL();

  arc::ArcDigitalGoodsBridge* GetArcDigitalGoodsBridge();

  mojo::ReceiverSet<payments::mojom::DigitalGoods> receiver_set_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_DIGITAL_GOODS_DIGITAL_GOODS_IMPL_H_
