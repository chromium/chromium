// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CAPABILITIES_FETCHER_H_
#define CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CAPABILITIES_FETCHER_H_

#include "base/containers/flat_set.h"
#include "components/keyed_service/core/keyed_service.h"

namespace autofill {
class FormSignature;
}  // namespace autofill

namespace url {
class Origin;
}  // namespace url

// A service that provides information about whether a form on a given origin
// is supported for FastCheckout flows. The service is used as one of several
// inputs that determine whether to offer a FastCheckout flow to a user
// interacting with an input form field.
class FastCheckoutCapabilitiesFetcher : public KeyedService {
 public:
  // Downloads Fast Checkout capabilities for all domains.
  virtual void FetchCapabilities() = 0;
  // Checks whether a form with `form_signature` on `origin` is supported
  // for Fast Checkout. Requires `FetchCapabilities()` to have been completed
  // or will return `false` otherwise.
  virtual bool IsTriggerFormSupported(
      const url::Origin& origin,
      autofill::FormSignature form_signature) = 0;
  // Returns form signatures cached for `origin` if present. Otherwise returns
  // an empty set.
  virtual base::flat_set<autofill::FormSignature> GetFormsToFill(
      const url::Origin& origin) = 0;
};

#endif  // CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CAPABILITIES_FETCHER_H_
