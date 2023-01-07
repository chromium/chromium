// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_CLIENT_TEST_UTIL_H_
#define CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_CLIENT_TEST_UTIL_H_

#include <memory>

class KeyedService;

namespace content {
class BrowserContext;
}

namespace network {
class TestURLLoaderFactory;
}

// Creates a ChromeSigninClient using the supplied
// |test_url_loader_factory| and |context|.
std::unique_ptr<KeyedService> BuildChromeSigninClientWithURLLoader(
    network::TestURLLoaderFactory* test_url_loader_factory,
    content::BrowserContext* context);

#endif  // CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_CLIENT_TEST_UTIL_H_
