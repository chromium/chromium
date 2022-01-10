// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_authentication_proxy/web_authentication_proxy_service.h"

#include <limits>

#include "base/json/json_string_value_serializer.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
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

namespace {
int32_t NewRequestId() {
  return base::RandGenerator(std::numeric_limits<uint32_t>::max()) + 1;
}

absl::optional<blink::mojom::AuthenticatorStatus>
ToAuthenticatorStatusForMakeCredential(const std::string& error_name) {
  constexpr std::pair<const char*, blink::mojom::AuthenticatorStatus>
      kErrors[] = {
          {"NotAllowedError",
           blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR},
          {"InvalidStateError",
           blink::mojom::AuthenticatorStatus::CREDENTIAL_EXCLUDED},
          {"OperationError",
           blink::mojom::AuthenticatorStatus::PENDING_REQUEST},
          {"NotSupportedError",
           blink::mojom::AuthenticatorStatus::ALGORITHM_UNSUPPORTED},
          {"AbortError", blink::mojom::AuthenticatorStatus::ABORT_ERROR},
          {"NotReadableError",
           blink::mojom::AuthenticatorStatus::UNKNOWN_ERROR},
          {"SecurityError", blink::mojom::AuthenticatorStatus::INVALID_DOMAIN},
      };
  for (const auto& err : kErrors) {
    if (err.first == error_name) {
      return err.second;
    }
  }
  return absl::nullopt;
}

}  // namespace

WebAuthenticationProxyService::WebAuthenticationProxyService(
    content::BrowserContext* browser_context)
    : event_router_(EventRouter::Get(browser_context)),
      extension_registry_(ExtensionRegistry::Get(browser_context)) {
  extension_registry_observation_.Observe(
      ExtensionRegistry::Get(browser_context));
}

WebAuthenticationProxyService::~WebAuthenticationProxyService() = default;

const Extension* WebAuthenticationProxyService::GetActiveRequestProxy() {
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
  if (!active_request_proxy_extension_id_) {
    return;
  }
  CancelPendingCallbacks();
  active_request_proxy_extension_id_.reset();
}

void WebAuthenticationProxyService::SetActiveRequestProxy(
    const Extension* extension) {
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
  DCHECK(error_out);
  auto callback_it = pending_create_callbacks_.find(details.request_id);
  if (callback_it == pending_create_callbacks_.end()) {
    *error_out = "Invalid requestId";
    return false;
  }
  CreateCallback callback = std::move(callback_it->second);
  pending_create_callbacks_.erase(callback_it);

  if (details.error_name) {
    absl::optional<blink::mojom::AuthenticatorStatus> status =
        ToAuthenticatorStatusForMakeCredential(*details.error_name);
    // TODO(https://crbug.com/1231802): Allow passing DOMErrors through the
    // Authenticator interface rather than having to project them back into
    // AuthenticatorStatus values, which is inherently lossy.
    if (!status) {
      *error_out = "Invalid CreateResponseDetails.errorName";
      return false;
    }
    std::move(callback).Run(*status, nullptr);
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
      FromValue(*response_value);
  if (!response) {
    *error_out = "Invalid responseJson";
    return false;
  }

  std::move(callback).Run(blink::mojom::AuthenticatorStatus::SUCCESS,
                          std::move(response));
  return true;
}

bool WebAuthenticationProxyService::CompleteIsUvpaaRequest(
    const api::web_authentication_proxy::IsUvpaaResponseDetails& details) {
  auto callback_it = pending_is_uvpaa_callbacks_.find(details.request_id);
  if (callback_it == pending_is_uvpaa_callbacks_.end()) {
    return false;
  }
  IsUvpaaCallback callback = std::move(callback_it->second);
  pending_is_uvpaa_callbacks_.erase(callback_it);
  std::move(callback).Run(details.is_uvpaa);
  return true;
}

void WebAuthenticationProxyService::CancelPendingCallbacks() {
  DCHECK(IsActive());
  for (auto& pair : pending_create_callbacks_) {
    std::move(pair.second)
        .Run(blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR, nullptr);
  }
  pending_create_callbacks_.clear();

  for (auto& pair : pending_is_uvpaa_callbacks_) {
    std::move(pair.second).Run(/*is_uvpaa=*/false);
  }
  pending_is_uvpaa_callbacks_.clear();
}

bool WebAuthenticationProxyService::IsActive() {
  return active_request_proxy_extension_id_.has_value();
}

void WebAuthenticationProxyService::SignalCreateRequest(
    const blink::mojom::PublicKeyCredentialCreationOptionsPtr& options_ptr,
    CreateCallback callback) {
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
}

void WebAuthenticationProxyService::SignalIsUvpaaRequest(
    IsUvpaaCallback callback) {
  DCHECK(IsActive());

  int32_t request_id = NewRequestId();
  // Technically, this could spin forever if there are 4 billion active
  // requests. However, there's no real risk to this happening (no security or
  // DOS concerns).
  while (pending_is_uvpaa_callbacks_.find(request_id) !=
         pending_is_uvpaa_callbacks_.end()) {
    request_id = NewRequestId();
  }
  pending_is_uvpaa_callbacks_.emplace(request_id, std::move(callback));
  api::web_authentication_proxy::IsUvpaaRequest request;
  request.request_id = request_id;
  event_router_->DispatchEventToExtension(
      *active_request_proxy_extension_id_,
      std::make_unique<Event>(
          events::WEB_AUTHENTICATION_PROXY_ON_ISUVPAA_REQUEST,
          api::web_authentication_proxy::OnIsUvpaaRequest::kEventName,
          api::web_authentication_proxy::OnIsUvpaaRequest::Create(request)));
}

void WebAuthenticationProxyService::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
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
