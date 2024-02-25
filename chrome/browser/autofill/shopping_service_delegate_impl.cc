// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/shopping_service_delegate_impl.h"

#include "components/commerce/core/shopping_service.h"

namespace autofill {
ShoppingServiceDelegateImpl::ShoppingServiceDelegateImpl(
    commerce::ShoppingService* shopping_service)
    : shopping_service_(shopping_service) {}

ShoppingServiceDelegateImpl::~ShoppingServiceDelegateImpl() = default;

bool ShoppingServiceDelegateImpl::IsDiscountEligibleToShowOnNavigation() {
  return shopping_service_ &&
         shopping_service_->IsDiscountEligibleToShowOnNavigation();
}

void ShoppingServiceDelegateImpl::GetDiscountInfoForUrls(
    const std::vector<GURL>& urls,
    commerce::DiscountInfoCallback callback) {
  if (!shopping_service_) {
    return;
  }
  shopping_service_->GetDiscountInfoForUrls(urls, std::move(callback));
}

}  // namespace autofill
