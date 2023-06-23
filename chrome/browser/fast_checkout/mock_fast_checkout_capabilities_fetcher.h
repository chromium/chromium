// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_MOCK_FAST_CHECKOUT_CAPABILITIES_FETCHER_H_
#define CHROME_BROWSER_FAST_CHECKOUT_MOCK_FAST_CHECKOUT_CAPABILITIES_FETCHER_H_

#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher.h"

#include "components/autofill/core/common/signatures.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/origin.h"

class MockFastCheckoutCapabilitiesFetcher
    : public FastCheckoutCapabilitiesFetcher {
 public:
  MockFastCheckoutCapabilitiesFetcher();
  ~MockFastCheckoutCapabilitiesFetcher() override;

  MOCK_METHOD(void, FetchCapabilities, (), (override));
  MOCK_METHOD(bool,
              IsTriggerFormSupported,
              (const url::Origin&, autofill::FormSignature),
              (override));
  MOCK_METHOD(base::flat_set<autofill::FormSignature>,
              GetFormsToFill,
              (const url::Origin& origin),
              (override));
};

#endif  // CHROME_BROWSER_FAST_CHECKOUT_MOCK_FAST_CHECKOUT_CAPABILITIES_FETCHER_H_
