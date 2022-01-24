// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_authentication_proxy/web_authentication_proxy_api.h"

#include "chrome/browser/extensions/api/web_authentication_proxy/web_authentication_proxy_service.h"
#include "chrome/common/extensions/api/web_authentication_proxy.h"

namespace extensions {

WebAuthenticationProxyAttachFunction::WebAuthenticationProxyAttachFunction() =
    default;
WebAuthenticationProxyAttachFunction::~WebAuthenticationProxyAttachFunction() =
    default;

ExtensionFunction::ResponseAction WebAuthenticationProxyAttachFunction::Run() {
  DCHECK(extension());

  WebAuthenticationProxyService* proxy_service =
      WebAuthenticationProxyServiceFactory::GetForBrowserContext(
          browser_context());

  const Extension* active_proxy = proxy_service->GetActiveRequestProxy();
  if (active_proxy) {
    return RespondNow(Error("Another extension is already attached"));
  }

  proxy_service->SetActiveRequestProxy(extension());
  return RespondNow(NoArguments());
}

WebAuthenticationProxyDetachFunction::WebAuthenticationProxyDetachFunction() =
    default;
WebAuthenticationProxyDetachFunction::~WebAuthenticationProxyDetachFunction() =
    default;

ExtensionFunction::ResponseAction WebAuthenticationProxyDetachFunction::Run() {
  DCHECK(extension());

  WebAuthenticationProxyService* proxy_service =
      WebAuthenticationProxyServiceFactory::GetForBrowserContext(
          browser_context());

  if (proxy_service->GetActiveRequestProxy() != extension()) {
    return RespondNow(Error("This extension is not currently attached"));
  }

  proxy_service->ClearActiveRequestProxy();
  return RespondNow(NoArguments());
}

WebAuthenticationProxyCompleteIsUvpaaRequestFunction::
    WebAuthenticationProxyCompleteIsUvpaaRequestFunction() = default;
WebAuthenticationProxyCompleteIsUvpaaRequestFunction::
    ~WebAuthenticationProxyCompleteIsUvpaaRequestFunction() = default;

ExtensionFunction::ResponseAction
WebAuthenticationProxyCompleteIsUvpaaRequestFunction::Run() {
  DCHECK(extension());
  auto params =
      api::web_authentication_proxy::CompleteIsUvpaaRequest::Params::Create(
          args());
  EXTENSION_FUNCTION_VALIDATE(params.get());
  WebAuthenticationProxyService* proxy_service =
      WebAuthenticationProxyServiceFactory::GetForBrowserContext(
          browser_context());
  if (proxy_service->GetActiveRequestProxy() != extension()) {
    return RespondNow(Error("Invalid sender"));
  }
  if (!proxy_service->CompleteIsUvpaaRequest(params->details.request_id,
                                             params->details.is_uvpaa)) {
    return RespondNow(Error("Invalid request id"));
  }
  return RespondNow(NoArguments());
}

}  // namespace extensions
