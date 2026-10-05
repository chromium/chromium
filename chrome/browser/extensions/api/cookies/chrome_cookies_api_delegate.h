// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_COOKIES_CHROME_COOKIES_API_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_COOKIES_CHROME_COOKIES_API_DELEGATE_H_

#include "extensions/browser/api/cookies/cookies_api_delegate.h"

namespace extensions {

// Chrome's implementation of the chrome.cookies API's embedder hooks. Keeps
// all knowledge of Chrome's Profile/incognito-Profile relationship out of the
// //extensions-agnostic cookies API implementation.
class ChromeCookiesApiDelegate : public CookiesApiDelegate {
 public:
  ChromeCookiesApiDelegate();
  ChromeCookiesApiDelegate(const ChromeCookiesApiDelegate&) = delete;
  ChromeCookiesApiDelegate& operator=(const ChromeCookiesApiDelegate&) = delete;
  ~ChromeCookiesApiDelegate() override;

  // CookiesApiDelegate:
  std::vector<CookieStoreContext> GetCookieStoreContexts(
      content::BrowserContext& context,
      bool include_incognito) override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_COOKIES_CHROME_COOKIES_API_DELEGATE_H_
