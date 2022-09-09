// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_TEST_SIGNIN_CLIENT_BUILDER_H_
#define CHROME_BROWSER_SIGNIN_TEST_SIGNIN_CLIENT_BUILDER_H_

#include <memory>

class KeyedService;

namespace content {
class BrowserContext;
}

namespace signin {

// Method to be used by the |ChromeSigninClientFactory| to create a test version
// of the SigninClient
std::unique_ptr<KeyedService> BuildTestSigninClient(
    content::BrowserContext* context);

}  // namespace signin


#endif  // CHROME_BROWSER_SIGNIN_TEST_SIGNIN_CLIENT_BUILDER_H_
