// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_MOCK_FAST_CHECKOUT_CLIENT_H_
#define CHROME_BROWSER_FAST_CHECKOUT_MOCK_FAST_CHECKOUT_CLIENT_H_

#include "chrome/browser/fast_checkout/fast_checkout_client_impl.h"

#include "testing/gmock/include/gmock/gmock.h"

class MockFastCheckoutClient : public FastCheckoutClientImpl {
 public:
  static MockFastCheckoutClient* CreateForWebContents(
      content::WebContents* web_contents);

  explicit MockFastCheckoutClient(content::WebContents* web_contents);
  ~MockFastCheckoutClient() override;

  MOCK_METHOD(bool,
              TryToStart,
              (const GURL& url,
               const autofill::FormData& form,
               const autofill::FormFieldData& field,
               base::WeakPtr<autofill::AutofillManager> autofill_manager),
              (override));
  MOCK_METHOD(void, Stop, (bool allow_further_runs), (override));
  MOCK_METHOD(bool, IsRunning, (), (const override));
  MOCK_METHOD(bool, IsShowing, (), (const override));
  MOCK_METHOD(void,
              OnNavigation,
              (const GURL& url, bool is_cart_or_checkout_url),
              (override));
};

#endif
