// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_authentication_proxy/web_authentication_proxy_service.h"

#include <limits>

#include "base/json/json_string_value_serializer.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "chrome/browser/extensions/api/web_authentication_proxy/value_conversions.h"
#include "chrome/common/extensions/api/web_authentication_proxy.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "extensions/browser/extension_registry_factory.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-shared.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"

namespace extensions {

WebAuthenticationProxyService::WebAuthenticationProxyService(
    content::BrowserContext* browser_context)
    : event_router_(EventRouter::Get(browser_context)),
      extension_registry_(ExtensionRegistry::Get(browser_context)) {
  extension_registry_observation_.Observe(
      ExtensionRegistry::Get(browser_context));
}

WebAuthenticationProxyService::~WebAuthenticationProxyService() = default;

const Extension* WebAuthenticationProxyService::GetActiveRequestProxy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!active_request_proxy_extension_id_) {
    return nullptr;
  }
  const Extension* extension =
      extension_registry_->enabled_extensions().GetByID(
          *active_request_proxy_extension_id_);
  DCHECK(extension);
  return extension;
}

void WebAuthenticationProxyService::ClearActiveRequestProxy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!active_request_proxy_extension_id_) {
    return;
  }
  CancelPendingCallbacks();
  active_request_proxy_extension_id_.reset();
}

void WebAuthenticationProxyService::SetActiveRequestProxy(
    const Extension* extension) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(extension);
  DCHECK(extension_registry_->enabled_extensions().Contains(extension->id()));
  // Callers must explicitly clear the active request proxy first.
  DCHECK(!active_request_proxy_extension_id_)
      << "SetActiveRequestProxy() called with an active proxy";
  // Clearing the request proxy should have resolved all outstanding requests.
  DCHECK(pending_create_callbacks_.empty());
  DCHECK(pending_is_uvpaa_callbacks_.empty());

  active_request_proxy_extension_id_ = extension->id();
}

bool WebAuthenticationProxyService::CompleteCreateRequest(
    const api::web_authentication_proxy::CreateResponseDetails& details,
    std::string* error_out) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(error_out);
  auto callback_it = pending_create_callbacks_.find(details.request_id);
  if (callback_it == pending_create_callbacks_.end()) {
    *error_out = "Invalid requestId";
    return false;
  }
  CreateCallback callback = std::move(callback_it->second);
  pending_create_callbacks_.erase(callback_it);

  if (details.error) {
    std::move(callback).Run(details.request_id,
                            blink::mojom::WebAuthnDOMExceptionDetails::New(
                                details.error->name, details.error->message),
                            nullptr);
    return true;
  }
  if (!details.response_json) {
    *error_out = "Missing CreateResponseDetails.responseJson";
    return false;
  }

  JSONStringValueDeserializer deserializer(*details.response_json);
  std::string deserialize_error;
  std::unique_ptr<base::Value> response_value =
      deserializer.Deserialize(/*error_code=*/nullptr, &deserialize_error);
  if (!response_value) {
    *error_out = "Parsing responseJson failed: " + deserialize_error;
    return false;
  }
  blink::mojom::MakeCredentialAuthenticatorResponsePtr response =
      MakeCredentialResponseFromValue(*response_value);
  if (!response) {
    *error_out = "Invalid responseJson";
    return false;
  }

  std::move(callback).Run(details.request_id, nullptr, std::move(response));
  return true;
}

bool WebAuthenticationProxyService::CompleteGetRequest(
    const api::web_authentication_proxy::GetResponseDetails& details,
    std::string* error_out) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(error_out);
  auto callback_it = pending_get_callbacks_.find(details.request_id);
  if (callback_it == pending_get_callbacks_.end()) {
    *error_out = "Invalid requestId";
    return false;
  }
  GetCallback callback = std::move(callback_it->second);
  pending_get_callbacks_.erase(callback_it);

  if (details.error) {
    std::move(callback).Run(details.request_id,
                            blink::mojom::WebAuthnDOMExceptionDetails::New(
                                details.error->name, details.error->message),
                            nullptr);
    return true;
  }
  if (!details.response_json) {
    *error_out = "Missing GetResponseDetails.responseJson";
    return false;
  }

  JSONStringValueDeserializer deserializer(*details.response_json);
  std::string deserialize_error;
  std::unique_ptr<base::Value> response_value =
      deserializer.Deserialize(/*error_code=*/nullptr, &deserialize_error);
  if (!response_value) {
    *error_out = "Parsing responseJson failed: " + deserialize_error;
    return false;
  }
  blink::mojom::GetAssertionAuthenticatorResponsePtr response =
      GetAssertionResponseFromValue(*response_value);
  if (!response) {
    *error_out = "Invalid responseJson";
    return false;
  }

  std::move(callback).Run(details.request_id, nullptr, std::move(response));
  return true;
}

bool WebAuthenticationProxyService::CompleteIsUvpaaRequest(
    const api::web_authentication_proxy::IsUvpaaResponseDetails& details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto callback_it = pending_is_uvpaa_callbacks_.find(details.request_id);
  if (callback_it == pending_is_uvpaa_callbacks_.end()) {
    return false;
  }
  IsUvpaaCallback callback = std::move(callback_it->second);
  pending_is_uvpaa_callbacks_.erase(callback_it);
  std::move(callback).Run(details.is_uvpaa);
  return true;
}

void WebAuthenticationProxyService::CancelRequest(RequestId request_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsActive());
  if (base::Contains(pending_create_callbacks_, request_id)) {
    pending_create_callbacks_.erase(request_id);
  } else if (base::Contains(pending_get_callbacks_, request_id)) {
    pending_get_callbacks_.erase(request_id);
  } else {
    // Invalid `request_id`. Note that isUvpaa requests cannot be cancelled.
    return;
  }

  event_router_->DispatchEventToExtension(
      *active_request_proxy_extension_id_,
      std::make_unique<Event>(
          events::WEB_AUTHENTICATION_PROXY_REQUEST_CANCELLED,
          api::web_authentication_proxy::OnRequestCanceled::kEventName,
          api::web_authentication_proxy::OnRequestCanceled::Create(
              request_id)));
}

void WebAuthenticationProxyService::CancelPendingCallbacks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsActive());

  auto abort_pending_callbacks = [](auto callbacks) {
    for (auto& pair : callbacks) {
      auto error = blink::mojom::WebAuthnDOMExceptionDetails::New(
          "AbortError", "The operation was aborted.");
      std::move(pair.second).Run(pair.first, std::move(error), nullptr);
    }
  };

  abort_pending_callbacks(std::move(pending_create_callbacks_));
  abort_pending_callbacks(std::move(pending_get_callbacks_));

  auto is_uvpaa_callbacks = std::move(pending_is_uvpaa_callbacks_);
  for (auto& pair : is_uvpaa_callbacks) {
    std::move(pair.second).Run(/*is_uvpaa=*/false);
  }
}

WebAuthenticationProxyService::RequestId
WebAuthenticationProxyService::NewRequestId() {
  int32_t request_id =
      base::RandGenerator(std::numeric_limits<uint32_t>::max()) + 1;
  // Technically, this could spin forever if there are 4 billion active
  // requests. However, there's no real risk to this happening (no security or
  // DOS concerns).
  while (base::Contains(pending_is_uvpaa_callbacks_, request_id) ||
         base::Contains(pending_create_callbacks_, request_id) ||
         base::Contains(pending_get_callbacks_, request_id)) {
    request_id = base::RandGenerator(std::numeric_limits<uint32_t>::max()) + 1;
  }
  return request_id;
}

bool WebAuthenticationProxyService::IsActive() {
  return active_request_proxy_extension_id_.has_value();
}

WebAuthenticationProxyService::RequestId
WebAuthenticationProxyService::SignalCreateRequest(
    const blink::mojom::PublicKeyCredentialCreationOptionsPtr& options_ptr,
    CreateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsActive());

  uint32_t request_id = NewRequestId();
  pending_create_callbacks_.emplace(request_id, std::move(callback));

  api::web_authentication_proxy::CreateRequest request;
  request.request_id = request_id;

  base::Value options_value = ToValue(options_ptr);
  std::string request_json;
  JSONStringValueSerializer serializer(&request.request_details_json);
  CHECK(serializer.Serialize(options_value));

  event_router_->DispatchEventToExtension(
      *active_request_proxy_extension_id_,
      std::make_unique<Event>(
          events::WEB_AUTHENTICATION_PROXY_ON_CREATE_REQUEST,
          api::web_authentication_proxy::OnCreateRequest::kEventName,
          api::web_authentication_proxy::OnCreateRequest::Create(request)));
  return request_id;
}

WebAuthenticationProxyService::RequestId
WebAuthenticationProxyService::SignalGetRequest(
    const blink::mojom::PublicKeyCredentialRequestOptionsPtr& options_ptr,
    GetCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsActive());

  uint32_t request_id = NewRequestId();
  pending_get_callbacks_.emplace(request_id, std::move(callback));

  api::web_authentication_proxy::GetRequest request;
  request.request_id = request_id;

  base::Value options_value = ToValue(options_ptr);
  std::string request_json;
  JSONStringValueSerializer serializer(&request.request_details_json);
  CHECK(serializer.Serialize(options_value));

  event_router_->DispatchEventToExtension(
      *active_request_proxy_extension_id_,
      std::make_unique<Event>(
          events::WEB_AUTHENTICATION_PROXY_ON_GET_REQUEST,
          api::web_authentication_proxy::OnGetRequest::kEventName,
          api::web_authentication_proxy::OnGetRequest::Create(request)));
  return request_id;
}

WebAuthenticationProxyService::RequestId
WebAuthenticationProxyService::SignalIsUvpaaRequest(IsUvpaaCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsActive());

  int32_t request_id = NewRequestId();
  pending_is_uvpaa_callbacks_.emplace(request_id, std::move(callback));
  api::web_authentication_proxy::IsUvpaaRequest request;
  request.request_id = request_id;
  event_router_->DispatchEventToExtension(
      *active_request_proxy_extension_id_,
      std::make_unique<Event>(
          events::WEB_AUTHENTICATION_PROXY_ON_ISUVPAA_REQUEST,
          api::web_authentication_proxy::OnIsUvpaaRequest::kEventName,
          api::web_authentication_proxy::OnIsUvpaaRequest::Create(request)));
  return request_id;
}

void WebAuthenticationProxyService::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (extension->id() != active_request_proxy_extension_id_) {
    return;
  }
  ClearActiveRequestProxy();
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
  DependsOn(ExtensionRegistryFactory::GetInstance());
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
