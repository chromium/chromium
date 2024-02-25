// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEB_AUTHENTICATION_PROXY_WEB_AUTHENTICATION_PROXY_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEB_AUTHENTICATION_PROXY_WEB_AUTHENTICATION_PROXY_API_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/api/web_authentication_proxy/remote_session_state_change.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace extensions {

class WebAuthenticationProxyAPI : public BrowserContextKeyedAPI,
                                  public EventRouter::Observer {
 public:
  static BrowserContextKeyedAPIFactory<WebAuthenticationProxyAPI>*
  GetFactoryInstance();

  explicit WebAuthenticationProxyAPI(content::BrowserContext* context);
  WebAuthenticationProxyAPI(const WebAuthenticationProxyAPI&) = delete;
  WebAuthenticationProxyAPI& operator=(const WebAuthenticationProxyAPI&) =
      delete;
  ~WebAuthenticationProxyAPI() override;

 private:
  friend class BrowserContextKeyedAPIFactory<WebAuthenticationProxyAPI>;

  // BrowserContextKeyedAPI:
  static const bool kServiceIsNULLWhileTesting = true;
  static const char* service_name() { return "WebAuthenticationProxyAPI"; }
  void Shutdown() override;

  // EventRouter::Observer:
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override;

  const raw_ptr<content::BrowserContext> context_;
  std::map<ExtensionId, WebAuthenticationProxyRemoteSessionStateChangeNotifier>
      session_state_change_notifiers_;
};

template <>
struct BrowserContextFactoryDependencies<WebAuthenticationProxyAPI> {
  static void DeclareFactoryDependencies(
      BrowserContextKeyedAPIFactory<WebAuthenticationProxyAPI>* factory) {
    factory->DependsOn(
        ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
    factory->DependsOn(EventRouterFactory::GetInstance());
  }
};

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

  void DoRespond(std::optional<std::string> error);

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

  void DoRespond(std::optional<std::string> error);

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
