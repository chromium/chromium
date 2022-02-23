// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEB_AUTHENTICATION_PROXY_WEB_AUTHENTICATION_PROXY_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEB_AUTHENTICATION_PROXY_WEB_AUTHENTICATION_PROXY_API_H_

#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace extensions {

class WebAuthenticationProxyAttachFunction : public ExtensionFunction {
 public:
  WebAuthenticationProxyAttachFunction();

 protected:
  ~WebAuthenticationProxyAttachFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("webAuthenticationProxy.attach",
                             WEB_AUTHENTICATION_PROXY_ATTACH)
};

class WebAuthenticationProxyDetachFunction : public ExtensionFunction {
 public:
  WebAuthenticationProxyDetachFunction();

 protected:
  ~WebAuthenticationProxyDetachFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("webAuthenticationProxy.detach",
                             WEB_AUTHENTICATION_PROXY_DETACH)
};

class WebAuthenticationProxyCompleteCreateRequestFunction
    : public ExtensionFunction {
 public:
  WebAuthenticationProxyCompleteCreateRequestFunction();

 protected:
  ~WebAuthenticationProxyCompleteCreateRequestFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("webAuthenticationProxy.completeCreateRequest",
                             WEB_AUTHENTICATION_PROXY_COMPLETE_CREATE_REQUEST)
};

class WebAuthenticationProxyCompleteGetRequestFunction
    : public ExtensionFunction {
 public:
  WebAuthenticationProxyCompleteGetRequestFunction();

 protected:
  ~WebAuthenticationProxyCompleteGetRequestFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
  DECLARE_EXTENSION_FUNCTION("webAuthenticationProxy.completeGetRequest",
                             WEB_AUTHENTICATION_PROXY_COMPLETE_GET_REQUEST)
};

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
