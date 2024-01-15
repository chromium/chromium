// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CART_COMMERCE_HINT_SERVICE_H_
#define CHROME_BROWSER_CART_COMMERCE_HINT_SERVICE_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/version.h"
#include "chrome/common/cart/commerce_hints.mojom.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/cart/cart_service.h"
#endif

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
                   const std::optional<GURL>& cart_url,
                   const std::string& product_id = std::string());
  void OnRemoveCart(const GURL& url);
  void OnFormSubmit(const GURL& navigation_url, bool is_purchase);
  void OnWillSendRequest(const GURL& navigation_url, bool is_addtocart);
  void OnCartUpdated(const GURL& cart_url,
                     std::vector<mojom::ProductPtr> products);
  bool ShouldSkip(const GURL& url);

  // Testing-only. Used to initialize commerce heuristics data in browser
  // process for testing.
  static bool InitializeCommerceHeuristicsForTesting(
      base::Version version,
      const std::string& hint_json_data,
      const std::string& global_json_data,
      const std::string& product_id_json_data,
      const std::string& cart_extraction_script);

 private:
  explicit CommerceHintService(content::WebContents* web_contents);
  friend class content::WebContentsUserData<CommerceHintService>;

  void OnOperationFinished(const std::string& operation, bool success);

#if !BUILDFLAG(IS_ANDROID)
  void AddCartToDB(const GURL& url,
                   bool success,
                   std::vector<CartDB::KeyAndValue> proto_pairs);

  raw_ptr<CartService> service_;
#endif
  raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_ = nullptr;
  base::WeakPtrFactory<CommerceHintService> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace cart

#endif  // CHROME_BROWSER_CART_COMMERCE_HINT_SERVICE_H_
