// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_DIGITAL_GOODS_DIGITAL_GOODS_ASH_H_
#define CHROME_BROWSER_APPS_DIGITAL_GOODS_DIGITAL_GOODS_ASH_H_

#include <string>

#include "chromeos/crosapi/mojom/digital_goods.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

class GURL;

namespace apps {

// Created in ash-chrome. Receives Digital Goods API calls from lacros-chrome
// and forwards them to ARC.
class DigitalGoodsAsh : public crosapi::mojom::DigitalGoods {
 public:
  DigitalGoodsAsh();
  ~DigitalGoodsAsh() override;

  mojo::PendingRemote<crosapi::mojom::DigitalGoods> BindRequest();

  // crosapi::mojom::DigitalGoods overrides.
  void GetDetails(const std::string& web_app_id,
                  const GURL& scope,
                  const std::vector<std::string>& item_ids,
                  GetDetailsCallback callback) override;
  void ListPurchases(const std::string& web_app_id,
                     const GURL& scope,
                     ListPurchasesCallback callback) override;
  void ListPurchaseHistory(const std::string& web_app_id,
                           const GURL& scope,
                           ListPurchaseHistoryCallback callback) override;
  void Consume(const std::string& web_app_id,
               const GURL& scope,
               const std::string& purchase_token,
               ConsumeCallback callback) override;

 private:
  mojo::ReceiverSet<crosapi::mojom::DigitalGoods> receiver_set_;
};

class DigitalGoodsFactoryAsh : public crosapi::mojom::DigitalGoodsFactory {
 public:
  DigitalGoodsFactoryAsh();
  ~DigitalGoodsFactoryAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::DigitalGoodsFactory> receiver);

  // crosapi::mojom::DigitalGoodsFactory overrides.
  void CreateDigitalGoods(const std::string& payment_method,
                          const std::string& web_app_id,
                          CreateDigitalGoodsCallback callback) override;

 private:
  DigitalGoodsAsh digital_goods_;
  mojo::ReceiverSet<crosapi::mojom::DigitalGoodsFactory> receiver_set_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_DIGITAL_GOODS_DIGITAL_GOODS_ASH_H_
