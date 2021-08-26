// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEB_AUTHENTICATION_PROXY_WEB_AUTHENTICATION_PROXY_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEB_AUTHENTICATION_PROXY_WEB_AUTHENTICATION_PROXY_API_H_

#include "extensions/browser/extension_function.h"

namespace extensions {

// WebAuthenticationProxyCompleteIsUvpaaRequestFunction implements
// the chrome.webAuthenticationProxy.completeIsUvpaaRequest() API.
class WebAuthenticationProxyCompleteIsUvpaaRequestFunction
    : public ExtensionFunction {
 public:
  WebAuthenticationProxyCompleteIsUvpaaRequestFunction();

 protected:
  ~WebAuthenticationProxyCompleteIsUvpaaRequestFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("webAuthenticationProxy.completeIsUvpaaRequest",
                             WEB_AUTHENTICATION_PROXY_COMPLETE_ISUVPAA_REQUEST)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEB_AUTHENTICATION_PROXY_WEB_AUTHENTICATION_PROXY_API_H_
