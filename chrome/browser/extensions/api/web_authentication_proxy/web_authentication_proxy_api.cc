// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_authentication_proxy/web_authentication_proxy_api.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "chrome/browser/extensions/api/web_authentication_proxy/web_authentication_proxy_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/web_authentication_proxy.h"
#include "content/public/browser/browser_context.h"

namespace extensions {

BrowserContextKeyedAPIFactory<WebAuthenticationProxyAPI>*
WebAuthenticationProxyAPI::GetFactoryInstance() {
  static base::NoDestructor<
      BrowserContextKeyedAPIFactory<WebAuthenticationProxyAPI>>
      instance;
  return instance.get();
}

WebAuthenticationProxyAPI::WebAuthenticationProxyAPI(
    content::BrowserContext* context)
    : context_(context) {
  EventRouter::Get(context_)->RegisterObserver(
      this,
      api::web_authentication_proxy::OnRemoteSessionStateChange::kEventName);
}

WebAuthenticationProxyAPI::~WebAuthenticationProxyAPI() = default;

void WebAuthenticationProxyAPI::Shutdown() {
  EventRouter::Get(context_)->UnregisterObserver(this);
}

void WebAuthenticationProxyAPI::OnListenerAdded(
    const EventListenerInfo& details) {
  DCHECK_EQ(
      details.event_name,
      api::web_authentication_proxy::OnRemoteSessionStateChange::kEventName);
  // This may be called multiple times for the same extension, but we only need
  // to instantiate a notifier once.
  session_state_change_notifiers_.try_emplace(
      details.extension_id, EventRouter::Get(context_), details.extension_id);
}

void WebAuthenticationProxyAPI::OnListenerRemoved(
    const EventListenerInfo& details) {
  DCHECK_EQ(
      details.event_name,
      api::web_authentication_proxy::OnRemoteSessionStateChange::kEventName);
  if (EventRouter::Get(context_)->ExtensionHasEventListener(
          details.extension_id, api::web_authentication_proxy::
                                    OnRemoteSessionStateChange::kEventName)) {
    // This wasn't necessarily the last remaining listener for this extension.
    return;
  }
  auto it = session_state_change_notifiers_.find(details.extension_id);
  CHECK(it != session_state_change_notifiers_.end(), base::NotFatalUntil::M130);
  session_state_change_notifiers_.erase(it);
}

WebAuthenticationProxyAttachFunction::WebAuthenticationProxyAttachFunction() =
    default;
WebAuthenticationProxyAttachFunction::~WebAuthenticationProxyAttachFunction() =
    default;

ExtensionFunction::ResponseAction WebAuthenticationProxyAttachFunction::Run() {
  DCHECK(extension());

  const bool success =
      WebAuthenticationProxyRegistrarFactory::GetForBrowserContext(
          browser_context())
          ->SetRequestProxy(Profile::FromBrowserContext(browser_context()),
                            extension());
  return RespondNow(success ? NoArguments()
                            : Error("Another extension is already attached"));
}

WebAuthenticationProxyDetachFunction::WebAuthenticationProxyDetachFunction() =
    default;
WebAuthenticationProxyDetachFunction::~WebAuthenticationProxyDetachFunction() =
    default;

ExtensionFunction::ResponseAction WebAuthenticationProxyDetachFunction::Run() {
  DCHECK(extension());

  WebAuthenticationProxyService* proxy_service =
      WebAuthenticationProxyService::GetIfProxyAttached(browser_context());
  if (!proxy_service || proxy_service->GetActiveRequestProxy() != extension()) {
    return RespondNow(NoArguments());
  }

  WebAuthenticationProxyRegistrar* proxy_registrar =
      WebAuthenticationProxyRegistrarFactory::GetForBrowserContext(
          browser_context());
  proxy_registrar->ClearRequestProxy(
      Profile::FromBrowserContext(browser_context()));
  return RespondNow(NoArguments());
}

WebAuthenticationProxyCompleteCreateRequestFunction::
    WebAuthenticationProxyCompleteCreateRequestFunction() = default;
WebAuthenticationProxyCompleteCreateRequestFunction::
    ~WebAuthenticationProxyCompleteCreateRequestFunction() = default;

void WebAuthenticationProxyCompleteCreateRequestFunction::DoRespond(
    std::optional<std::string> error) {
  Respond(error ? Error(std::move(*error)) : NoArguments());
}

ExtensionFunction::ResponseAction
WebAuthenticationProxyCompleteCreateRequestFunction::Run() {
  DCHECK(extension());
  auto params =
      api::web_authentication_proxy::CompleteCreateRequest::Params::Create(
          args());
  EXTENSION_FUNCTION_VALIDATE(params);
  WebAuthenticationProxyService* proxy_service =
      WebAuthenticationProxyService::GetIfProxyAttached(browser_context());
  if (!proxy_service || proxy_service->GetActiveRequestProxy() != extension()) {
    return RespondNow(Error("Invalid sender"));
  }
  proxy_service->CompleteCreateRequest(
      params->details,
      base::BindOnce(
          &WebAuthenticationProxyCompleteCreateRequestFunction::DoRespond,
          this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

WebAuthenticationProxyCompleteGetRequestFunction::
    WebAuthenticationProxyCompleteGetRequestFunction() = default;
WebAuthenticationProxyCompleteGetRequestFunction::
    ~WebAuthenticationProxyCompleteGetRequestFunction() = default;

void WebAuthenticationProxyCompleteGetRequestFunction::DoRespond(
    std::optional<std::string> error) {
  Respond(error ? Error(std::move(*error)) : NoArguments());
}

ExtensionFunction::ResponseAction
WebAuthenticationProxyCompleteGetRequestFunction::Run() {
  DCHECK(extension());
  auto params =
      api::web_authentication_proxy::CompleteGetRequest::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  WebAuthenticationProxyService* proxy_service =
      WebAuthenticationProxyService::GetIfProxyAttached(browser_context());
  if (!proxy_service || proxy_service->GetActiveRequestProxy() != extension()) {
    return RespondNow(Error("Invalid sender"));
  }
  proxy_service->CompleteGetRequest(
      params->details,
      base::BindOnce(
          &WebAuthenticationProxyCompleteGetRequestFunction::DoRespond, this));
  return did_respond() ? AlreadyResponded() : RespondLater();
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
  EXTENSION_FUNCTION_VALIDATE(params);
  WebAuthenticationProxyService* proxy_service =
      WebAuthenticationProxyService::GetIfProxyAttached(browser_context());
  if (!proxy_service || proxy_service->GetActiveRequestProxy() != extension()) {
    return RespondNow(Error("Invalid sender"));
  }
  if (!proxy_service->CompleteIsUvpaaRequest(params->details)) {
    return RespondNow(Error("Invalid request id"));
  }
  return RespondNow(NoArguments());
}

}  // namespace extensions
