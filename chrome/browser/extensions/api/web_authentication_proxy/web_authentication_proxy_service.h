// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEB_AUTHENTICATION_PROXY_WEB_AUTHENTICATION_PROXY_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEB_AUTHENTICATION_PROXY_WEB_AUTHENTICATION_PROXY_SERVICE_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/web_authentication_request_proxy.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

class EventRouter;

// WebAuthenticationProxyService is an implementation of the
// content::WebAuthenticationRequestProxy interface that integrates Chrome's Web
// Authentication API with the webAuthenticationProxy extension API.
class WebAuthenticationProxyService
    : public content::WebAuthenticationRequestProxy,
      public KeyedService {
 public:
  using EventId = int32_t;

  // CompleteIsUvpaaRequest injects the result for the
  // `events::WEB_AUTHENTICATION_PROXY_ON_ISUVPAA_REQUEST` event with
  // `event_id`. `is_uvpaa` is the result to be returned to the original caller
  // of the PublicKeyCredential.IsUserPlatformAuthenticatorAvailable().
  //
  // Returns whether an event matching `event_id` was found.
  bool CompleteIsUvpaaRequest(EventId event_id, bool is_uvpaa);

 private:
  friend class WebAuthenticationProxyServiceFactory;

  explicit WebAuthenticationProxyService(
      content::BrowserContext* browser_context);
  ~WebAuthenticationProxyService() override;

  // content::WebAuthnRequestProxy:
  bool IsActive() override;
  void SignalIsUvpaaRequest(IsUvpaaCallback callback) override;

  EventRouter* event_router_ = nullptr;
  std::map<EventId, IsUvpaaCallback> pending_is_uvpaa_callbacks_;
};

// WebAuthenticationProxyServiceFactory creates instances of
// WebAuthenticationProxyService for a given BrowserContext.
class WebAuthenticationProxyServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static WebAuthenticationProxyServiceFactory* GetInstance();

  static WebAuthenticationProxyService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend class base::NoDestructor<WebAuthenticationProxyServiceFactory>;

  WebAuthenticationProxyServiceFactory();
  ~WebAuthenticationProxyServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEB_AUTHENTICATION_PROXY_WEB_AUTHENTICATION_PROXY_SERVICE_H_
