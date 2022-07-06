// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_UTIL_H_
#define CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_UTIL_H_

#include "components/autofill_assistant/browser/public/external_action.pb.h"

namespace autofill {
class AutofillProfile;
class CreditCard;
}  // namespace autofill

namespace fast_checkout {

// Creates `ProfileProto` from an `AutofillProfile`. Maps any `ServerFieldType`
// set on the `autofill_profile`.
autofill_assistant::external::ProfileProto CreateProfileProto(
    const autofill::AutofillProfile& autofill_profile);

// Creates `CreditCardProto` from a `CreditCard`. Maps any `ServerFieldType`
// set on the `credit_Card`. Also maps `record_type`, `instrument_id` and, if
// set, `network` and `server_id`.
autofill_assistant::external::CreditCardProto CreateCreditCardProto(
    const autofill::CreditCard& credit_card);

}  // namespace fast_checkout

#endif
