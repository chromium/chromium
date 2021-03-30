// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CART_COMMERCE_HINT_SERVICE_H_
#define CHROME_BROWSER_CART_COMMERCE_HINT_SERVICE_H_

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/cart/cart_service.h"
#include "chrome/common/cart/commerce_hints.mojom.h"
#include "components/optimization_guide/content/browser/optimization_guide_decider.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace cart {

class CommerceHintService
    : public content::WebContentsUserData<CommerceHintService> {
 public:
  ~CommerceHintService() override;
  void BindCommerceHintObserver(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<mojom::CommerceHintObserver> receiver);
  content::WebContents* WebContents();
  void OnAddToCart(const GURL& navigation_url,
                   const base::Optional<GURL>& cart_url);
  void OnRemoveCart(const GURL& url);
  void OnCartUpdated(const GURL& cart_url,
                     std::vector<mojom::ProductPtr> products);

 private:
  explicit CommerceHintService(content::WebContents* web_contents);
  friend class content::WebContentsUserData<CommerceHintService>;

  bool ShouldSkip(const GURL& url);
  void AddCartToDB(const GURL& url,
                   bool success,
                   std::vector<CartDB::KeyAndValue> proto_pairs);
  void OnOperationFinished(const std::string& operation, bool success);
  void ConstructCartProto(cart_db::ChromeCartContentProto* proto,
                          const GURL& navigation_url,
                          std::vector<mojom::ProductPtr> products);

  content::WebContents* web_contents_;
  CartService* service_;
  optimization_guide::OptimizationGuideDecider* optimization_guide_decider_ =
      nullptr;
  base::WeakPtrFactory<CommerceHintService> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace cart

#endif  // CHROME_BROWSER_CART_COMMERCE_HINT_SERVICE_H_
