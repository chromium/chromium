// Copyright 2021 The Chromium Authors
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
#include "device/fido/public_key_credential_rp_entity.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "extensions/browser/extension_registry_factory.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
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

void WebAuthenticationProxyService::CompleteCreateRequest(
    const api::web_authentication_proxy::CreateResponseDetails& details,
    RespondCallback respond_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto callback_it = pending_create_callbacks_.find(details.request_id);
  if (callback_it == pending_create_callbacks_.end()) {
    std::move(respond_callback).Run("Invalid requestId");
    return;
  }
  if (details.error) {
    // The proxied request yielded a DOMException.
    CreateCallback create_callback = std::move(callback_it->second);
    pending_create_callbacks_.erase(callback_it);
    std::move(create_callback)
        .Run(details.request_id,
             blink::mojom::WebAuthnDOMExceptionDetails::New(
                 details.error->name, details.error->message),
             nullptr);
    std::move(respond_callback).Run(absl::nullopt);
    return;
  }
  if (!details.response_json) {
    std::move(respond_callback)
        .Run("Missing CreateResponseDetails.responseJson");
    return;
  }
  data_decoder::DataDecoder::ParseJsonIsolated(
      *details.response_json,
      base::BindOnce(&WebAuthenticationProxyService::OnParseCreateResponse,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(respond_callback), details.request_id));
}

void WebAuthenticationProxyService::CompleteGetRequest(
    const api::web_authentication_proxy::GetResponseDetails& details,
    RespondCallback respond_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto callback_it = pending_get_callbacks_.find(details.request_id);
  if (callback_it == pending_get_callbacks_.end()) {
    std::move(respond_callback).Run("Invalid requestId");
    return;
  }
  if (details.error) {
    // The proxied request yielded a DOMException.
    GetCallback callback = std::move(callback_it->second);
    pending_get_callbacks_.erase(callback_it);
    std::move(callback).Run(details.request_id,
                            blink::mojom::WebAuthnDOMExceptionDetails::New(
                                details.error->name, details.error->message),
                            nullptr);
    std::move(respond_callback).Run(absl::nullopt);
    return;
  }
  if (!details.response_json) {
    std::move(respond_callback).Run("Missing GetResponseDetails.responseJson");
    return;
  }
  data_decoder::DataDecoder::ParseJsonIsolated(
      *details.response_json,
      base::BindOnce(&WebAuthenticationProxyService::OnParseGetResponse,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(respond_callback), details.request_id));
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

void WebAuthenticationProxyService::OnParseCreateResponse(
    RespondCallback respond_callback,
    RequestId request_id,
    data_decoder::DataDecoder::ValueOrError value_or_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!value_or_error.has_value()) {
    std::move(respond_callback)
        .Run("Parsing responseJson failed: " + value_or_error.error());
    return;
  }
  auto [response, error] =
      webauthn_proxy::MakeCredentialResponseFromValue(*value_or_error);
  if (!response) {
    std::move(respond_callback).Run("Invalid responseJson: " + error);
    return;
  }

  auto callback_it = pending_create_callbacks_.find(request_id);
  if (callback_it == pending_create_callbacks_.end()) {
    // The request was canceled while waiting for JSON decoding.
    std::move(respond_callback).Run("Invalid requestId");
    return;
  }

  // Success.
  CreateCallback create_callback = std::move(callback_it->second);
  pending_create_callbacks_.erase(callback_it);
  std::move(create_callback).Run(request_id, nullptr, std::move(response));
  std::move(respond_callback).Run(absl::nullopt);
}

void WebAuthenticationProxyService::OnParseGetResponse(
    RespondCallback respond_callback,
    RequestId request_id,
    data_decoder::DataDecoder::ValueOrError value_or_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!value_or_error.has_value()) {
    std::move(respond_callback)
        .Run("Parsing responseJson failed: " + value_or_error.error());
    return;
  }
  auto [response, error] =
      webauthn_proxy::GetAssertionResponseFromValue(*value_or_error);
  if (!response) {
    std::move(respond_callback).Run("Invalid responseJson: " + error);
    return;
  }

  auto callback_it = pending_get_callbacks_.find(request_id);
  if (callback_it == pending_get_callbacks_.end()) {
    // The request was canceled while waiting for JSON decoding.
    std::move(respond_callback).Run("Invalid requestId");
    return;
  }

  // Success.
  GetCallback get_callback = std::move(callback_it->second);
  pending_get_callbacks_.erase(callback_it);
  std::move(get_callback).Run(request_id, nullptr, std::move(response));
  std::move(respond_callback).Run(absl::nullopt);
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

  base::Value options_value = webauthn_proxy::ToValue(options_ptr);
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

  base::Value options_value = webauthn_proxy::ToValue(options_ptr);
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
    : ProfileKeyedServiceFactory(
          "WebAuthentcationProxyService",
          ProfileSelections::BuildForRegularAndIncognito()) {
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

}  // namespace extensions
