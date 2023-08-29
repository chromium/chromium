// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_SHOPPING_SERVICE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_AUTOFILL_SHOPPING_SERVICE_DELEGATE_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/commerce/core/shopping_service.h"
namespace autofill {

// This class is used when constructing the AutofillOfferManager. The
// ShoppingService can't be directly used because of crbug.com/1155712. We
// should remove this and use the ShoppingService directly once this bug is
// fixed.
class ShoppingServiceDelegateImpl : public ShoppingServiceDelegate {
 public:
  explicit ShoppingServiceDelegateImpl(
      commerce::ShoppingService* shopping_service);
  ~ShoppingServiceDelegateImpl() override;

  bool IsDiscountEligibleToShowOnNavigation() override;
  void GetDiscountInfoForUrls(const std::vector<GURL>& urls,
                              commerce::DiscountInfoCallback callback) override;

 private:
  raw_ptr<commerce::ShoppingService> shopping_service_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_SHOPPING_SERVICE_DELEGATE_IMPL_H_
