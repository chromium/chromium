// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_CLIENT_TEST_UTIL_H_
#define CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_CLIENT_TEST_UTIL_H_

#include <memory>

#include "base/callback_list.h"
#include "services/network/test/test_url_loader_factory.h"

class KeyedService;

namespace content {
class BrowserContext;
}

// Creates a ChromeSigninClient using the supplied
// |test_url_loader_factory| and |context|.
std::unique_ptr<KeyedService> BuildChromeSigninClientWithURLLoader(
    network::TestURLLoaderFactory* test_url_loader_factory,
    content::BrowserContext* context);

// Helps setting up test fixtures to have a `ChromeSigninClient` configured with
// a `TestURLLoaderFactory`. Just call
// `ChromeSigninClientWithURLLoaderHelper::SetUp()` during the test's
// `SetUpInProcessBrowserTestFixture()`.
class ChromeSigninClientWithURLLoaderHelper {
 public:
  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  void SetUp();

 protected:
  virtual void OnWillCreateBrowserContextServices(
      content::BrowserContext* context);

 private:
  base::CallbackListSubscription create_services_subscription_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

#endif  // CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_CLIENT_TEST_UTIL_H_
