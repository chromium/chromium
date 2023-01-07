// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_offer_notification_infobar_delegate_mobile.h"

#include <memory>

#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/infobars/core/infobar_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {

TEST(AutofillOfferNotificationInfoBarDelegateMobileTest,
     CreditCardIdentifierString) {
  GURL offer_details_url = GURL("http://pay.google.com");
  CreditCard card = test::GetCreditCard();
  std::unique_ptr<AutofillOfferNotificationInfoBarDelegateMobile> delegate =
      std::make_unique<AutofillOfferNotificationInfoBarDelegateMobile>(
          offer_details_url, card);

  EXPECT_EQ(delegate->credit_card_identifier_string(),
            card.CardIdentifierStringForAutofillDisplay());
  EXPECT_EQ(offer_details_url, delegate->deep_link_url());
}

}  // namespace autofill
