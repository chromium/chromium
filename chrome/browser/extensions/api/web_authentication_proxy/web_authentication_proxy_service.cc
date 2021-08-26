// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_authentication_proxy/web_authentication_proxy_service.h"

#include <limits>

#include "base/rand_util.h"
#include "chrome/common/extensions/api/web_authentication_proxy.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace extensions {

namespace {
int32_t NewRequestId() {
  return base::RandGenerator(std::numeric_limits<uint32_t>::max()) + 1;
}
}  // namespace

WebAuthenticationProxyService::WebAuthenticationProxyService(
    content::BrowserContext* browser_context)
    : event_router_(EventRouter::Get(browser_context)) {}

WebAuthenticationProxyService::~WebAuthenticationProxyService() = default;

bool WebAuthenticationProxyService::CompleteIsUvpaaRequest(EventId event_id,
                                                           bool is_uvpaa) {
  auto callback_it = pending_is_uvpaa_callbacks_.find(event_id);
  if (callback_it == pending_is_uvpaa_callbacks_.end()) {
    return false;
  }
  IsUvpaaCallback callback = std::move(callback_it->second);
  pending_is_uvpaa_callbacks_.erase(callback_it);
  std::move(callback).Run(is_uvpaa);
  return true;
}

bool WebAuthenticationProxyService::IsActive() {
  return event_router_->HasEventListener(
      api::web_authentication_proxy::OnIsUvpaaRequest::kEventName);
}

void WebAuthenticationProxyService::SignalIsUvpaaRequest(
    IsUvpaaCallback callback) {
  int32_t request_id = NewRequestId();
  // Technically, this could spin forever if there are 4 billion active
  // requests. However, there's no real risk to this happening (no security or
  // DOS concerns).
  while (pending_is_uvpaa_callbacks_.find(request_id) !=
         pending_is_uvpaa_callbacks_.end()) {
    request_id = NewRequestId();
  }
  pending_is_uvpaa_callbacks_.emplace(request_id, std::move(callback));
  base::Value args(base::Value::Type::LIST);
  args.Append(request_id);
  event_router_->BroadcastEvent(std::make_unique<Event>(
      events::WEB_AUTHENTICATION_PROXY_ON_ISUVPAA_REQUEST,
      api::web_authentication_proxy::OnIsUvpaaRequest::kEventName,
      std::move(args).TakeList()));
}

WebAuthenticationProxyServiceFactory*
WebAuthenticationProxyServiceFactory::GetInstance() {
  static base::NoDestructor<WebAuthenticationProxyServiceFactory> instance;
  return instance.get();
}

WebAuthenticationProxyServiceFactory::WebAuthenticationProxyServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "WebAuthentcationProxyService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(EventRouterFactory::GetInstance());
}

WebAuthenticationProxyServiceFactory::~WebAuthenticationProxyServiceFactory() =
    default;

WebAuthenticationProxyService*
WebAuthenticationProxyServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<WebAuthenticationProxyService*>(
      WebAuthenticationProxyServiceFactory::GetInstance()
          ->GetServiceForBrowserContext(context, true));
}

KeyedService* WebAuthenticationProxyServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new WebAuthenticationProxyService(context);
}

content::BrowserContext*
WebAuthenticationProxyServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace extensions
