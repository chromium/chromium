// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_DIGITAL_GOODS_DIGITAL_GOODS_LACROS_H_
#define CHROME_BROWSER_APPS_DIGITAL_GOODS_DIGITAL_GOODS_LACROS_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/digital_goods.mojom.h"
#include "content/public/browser/document_user_data.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/digital_goods/digital_goods.mojom.h"

namespace content {
class RenderFrameHost;
}

namespace apps {

// Created in lacros-chrome browser. Receives Digital Goods API calls from the
// renderer and forwards them to ash-chrome.
class DigitalGoodsLacros : public content::DocumentUserData<DigitalGoodsLacros>,
                           public payments::mojom::DigitalGoods {
 public:
  ~DigitalGoodsLacros() override;

  mojo::PendingRemote<payments::mojom::DigitalGoods> BindRequest();

  // payments::mojom::DigitalGoods overrides:
  void GetDetails(const std::vector<std::string>& item_ids,
                  GetDetailsCallback callback) override;
  void ListPurchases(ListPurchasesCallback callback) override;
  void ListPurchaseHistory(ListPurchaseHistoryCallback callback) override;
  void Consume(const std::string& purchase_token,
               ConsumeCallback callback) override;

 private:
  DigitalGoodsLacros(content::RenderFrameHost* render_frame_host,
                     mojo::PendingRemote<crosapi::mojom::DigitalGoods> remote);
  friend class content::DocumentUserData<DigitalGoodsLacros>;
  DOCUMENT_USER_DATA_KEY_DECL();

  mojo::ReceiverSet<payments::mojom::DigitalGoods> receiver_set_;
  mojo::Remote<crosapi::mojom::DigitalGoods> digital_goods_;
};

class DigitalGoodsFactoryLacros
    : public content::DocumentUserData<DigitalGoodsFactoryLacros>,
      public payments::mojom::DigitalGoodsFactory {
 public:
  ~DigitalGoodsFactoryLacros() override;

  static void Bind(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<payments::mojom::DigitalGoodsFactory> receiver);

  // payments::mojom::DigitalGoodsFactory overrides:
  void CreateDigitalGoods(const std::string& payment_method,
                          CreateDigitalGoodsCallback callback) override;

 private:
  explicit DigitalGoodsFactoryLacros(
      content::RenderFrameHost* render_frame_host);
  friend class content::DocumentUserData<DigitalGoodsFactoryLacros>;
  DOCUMENT_USER_DATA_KEY_DECL();

  void BindRequest(
      mojo::PendingReceiver<payments::mojom::DigitalGoodsFactory> receiver);

  void OnCreateDigitalGoods(
      payments::mojom::CreateDigitalGoodsResponseCode code,
      mojo::PendingRemote<crosapi::mojom::DigitalGoods> digital_goods);

  // A list of callbacks waiting for a single crosapi call response.
  // There may be multiple calls to CreateDigitalGoods from the renderer at the
  // same time, which need to be bound to a single browser-side object. It could
  // take time between user document data being checked for an existing Digital
  // Goods object and being assigned one from a crosapi callback, resulting in
  // multiple crosapi calls attempting to assign user document data multiple
  // times.
  std::vector<CreateDigitalGoodsCallback> pending_callbacks_;

  mojo::Receiver<payments::mojom::DigitalGoodsFactory> receiver_{this};
  base::WeakPtrFactory<DigitalGoodsFactoryLacros> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_DIGITAL_GOODS_DIGITAL_GOODS_LACROS_H_
