// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/web_authentication_proxy/web_authentication_proxy_service.h"

#include <limits>

#include "base/functional/overloaded.h"
#include "base/json/json_string_value_serializer.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/web_authentication_proxy.h"
#include "components/webauthn/json/value_conversions.h"
#include "content/public/browser/browser_context.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/permissions/permissions_data.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-shared.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "url/gurl.h"

namespace extensions {

namespace {

bool ProxyMayAttachToHost(const Extension& extension,
                          const url::Origin& origin) {
  // Prevent the proxy from being active on policy blocked hosts.
  //
  // We do not consider user restricted hosts here because those require host
  // permissions, and the webAuthenticationProxy permission is granted on all
  // hosts. We also don't block Chrome-restricted hosts in order to allow
  // authentication to the Chrome Web Store from inside a remote desktop
  // session, for example.
  return !extension.permissions_data()->IsPolicyBlockedHost(origin.GetURL());
}

}  // namespace

WebAuthenticationProxyRegistrar::WebAuthenticationProxyRegistrar(
    Profile* profile)
    : profile_(profile), extension_registry_(ExtensionRegistry::Get(profile)) {
  // Extension services such as ExtensionRegistry are shared between
  // off-the-record and regular Profile in the same way as
  // WebAuthenticationProxyRegistrar.
  extension_registry_observation_.Observe(extension_registry_);
  profile_observation_.Observe(profile_);
}

WebAuthenticationProxyRegistrar::~WebAuthenticationProxyRegistrar() = default;

bool WebAuthenticationProxyRegistrar::SetRequestProxy(
    Profile* profile,
    const Extension* extension) {
  // It is invalid to call this method with an unrelated BrowserContext.
  DCHECK(profile->IsSameOrParent(profile_));
  DCHECK(extension);
  DCHECK(extension_registry_->enabled_extensions().Contains(extension->id()));
  DCHECK(!attach_regular_proxy_to_both_contexts_ || active_regular_proxy_);

  if ((active_regular_proxy_ && *active_regular_proxy_ != extension->id()) ||
      (active_otr_split_proxy_ &&
       *active_otr_split_proxy_ != extension->id())) {
    // A different extension is attached. Attaching split mode extensions is
    // only supported for attaching the same extension in both
    // off-the-record/regular mode.
    return false;
  }

  if (profile->IsOffTheRecord()) {
    // Only a split mode extension can pass an off-the-record profile here. All
    // others use the same regular profile.
    active_otr_split_proxy_ = extension->id();
    WebAuthenticationProxyServiceFactory::GetForBrowserContext(profile)
        ->SetRequestProxy(PassKey(), extension);
    return true;
  }

  // Regular browser context.
  active_regular_proxy_ = extension->id();
  WebAuthenticationProxyServiceFactory::GetForBrowserContext(profile)
      ->SetRequestProxy(PassKey(), extension);
  // Also attach spanning mode extensions to the incognito profile. If no
  // incognito profile currently exists, OnOffTheRecordProfileCreated() may
  // attach later.
  if (util::CanCrossIncognito(extension, profile)) {
    attach_regular_proxy_to_both_contexts_ = true;
    if (profile->HasPrimaryOTRProfile()) {
      WebAuthenticationProxyServiceFactory::GetForBrowserContext(
          profile->GetPrimaryOTRProfile(/*create_if_needed=*/false))
          ->SetRequestProxy(PassKey(), extension);
    }
  }
  return true;
}

void WebAuthenticationProxyRegistrar::ClearRequestProxy(Profile* profile) {
  // It is invalid to call this method with an unrelated BrowserContext.
  DCHECK(profile->IsSameOrParent(profile_));

  if (profile->IsOffTheRecord()) {
    // Only a split mode extension can pass an off-the-record profile here. All
    // others use the same regular profile.
    DCHECK(active_otr_split_proxy_);
    DCHECK(!attach_regular_proxy_to_both_contexts_);
    active_otr_split_proxy_.reset();
    WebAuthenticationProxyServiceFactory::GetForBrowserContext(profile)
        ->ClearRequestProxy(PassKey());
    return;
  }

  // Regular browser context.
  DCHECK(active_regular_proxy_);
  active_regular_proxy_.reset();
  WebAuthenticationProxyServiceFactory::GetForBrowserContext(profile)
      ->ClearRequestProxy(PassKey());
  // Also clear spanning mode extensions from the incognito profile, if
  // necessary.
  if (attach_regular_proxy_to_both_contexts_ &&
      profile->HasPrimaryOTRProfile()) {
    WebAuthenticationProxyServiceFactory::GetForBrowserContext(
        profile->GetPrimaryOTRProfile(/*create_if_needed=*/false))
        ->ClearRequestProxy(PassKey());
  }
  attach_regular_proxy_to_both_contexts_ = false;
}

WebAuthenticationProxyRegistrar::ProxyStatus
WebAuthenticationProxyRegistrar::ProxyActiveForProfile(Profile* profile) {
  DCHECK(profile->IsSameOrParent(profile_));
  if (profile->IsOffTheRecord()) {
    if (active_otr_split_proxy_) {
      return ProxyStatus::kActive;
    }
    return active_regular_proxy_ && attach_regular_proxy_to_both_contexts_
               ? ProxyStatus::kActiveUseOriginalProfile
               : ProxyStatus::kInactive;
  }
  return active_regular_proxy_ ? ProxyStatus::kActive : ProxyStatus::kInactive;
}

void WebAuthenticationProxyRegistrar::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  // ExtensionRegistry redirects to the original profile for incognito, so this
  // only gets called once for the original profile, for both split and spanning
  // mode extensions.
  auto* profile = Profile::FromBrowserContext(browser_context);
  DCHECK_EQ(profile, profile_);
  auto* maybe_incognito_profile =
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/false);

  if (extension->id() == active_regular_proxy_) {
    active_regular_proxy_.reset();
    WebAuthenticationProxyServiceFactory::GetForBrowserContext(profile)
        ->ClearRequestProxy(PassKey());
    if (attach_regular_proxy_to_both_contexts_ && maybe_incognito_profile) {
      WebAuthenticationProxyServiceFactory::GetForBrowserContext(
          maybe_incognito_profile)
          ->ClearRequestProxy(PassKey());
    }
    attach_regular_proxy_to_both_contexts_ = false;
  }
  if (extension->id() == active_otr_split_proxy_) {
    DCHECK(!attach_regular_proxy_to_both_contexts_);
    active_otr_split_proxy_.reset();
    if (maybe_incognito_profile) {
      WebAuthenticationProxyServiceFactory::GetForBrowserContext(
          maybe_incognito_profile)
          ->ClearRequestProxy(PassKey());
    }
  }
}

void WebAuthenticationProxyRegistrar::OnOffTheRecordProfileCreated(
    Profile* otr_profile) {
  if (!attach_regular_proxy_to_both_contexts_) {
    return;
  }
  // A spanning mode extension attached to the regular profile before
  // this OTR profile existed. Attach it to the new OTR profile, too.
  DCHECK(active_regular_proxy_);
  const Extension* extension =
      extension_registry_->enabled_extensions().GetByID(*active_regular_proxy_);
  DCHECK(extension);
  WebAuthenticationProxyServiceFactory::GetForBrowserContext(otr_profile)
      ->SetRequestProxy(PassKey(), extension);
}

void WebAuthenticationProxyRegistrar::OnProfileWillBeDestroyed(
    Profile* profile) {
  // Reset any active split mode proxy if its profile is about to be destroyed.
  // No need to this clean up for regular profiles; this KeyedService will
  // simply be destroyed along with the profile.
  if (profile->IsOffTheRecord()) {
    active_otr_split_proxy_.reset();
  }
}

WebAuthenticationProxyRegistrarFactory*
WebAuthenticationProxyRegistrarFactory::GetInstance() {
  static base::NoDestructor<WebAuthenticationProxyRegistrarFactory> instance;
  return instance.get();
}

WebAuthenticationProxyRegistrarFactory::WebAuthenticationProxyRegistrarFactory()
    : ProfileKeyedServiceFactory(
          "WebAuthentcationProxyRegistrar",
          // Off-the-record profiles such as Incognito use the instance of their
          // original profile. `WebAuthenticationProxyRegistrar::IsActive()`
          // considers the extension's `incognito` manifest value to decide
          // whether proxying is actually effective in the OTR profile.
          //
          // Non-component extensions don't run in some OTR profile types, such
          // as Guest. So while we do return a `WebAuthenticationProxyRegistrar`
          // for those profile types `IsActive()` will always return false
          // there.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(ExtensionRegistryFactory::GetInstance());
}

WebAuthenticationProxyRegistrarFactory::
    ~WebAuthenticationProxyRegistrarFactory() = default;

WebAuthenticationProxyRegistrar*
WebAuthenticationProxyRegistrarFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<WebAuthenticationProxyRegistrar*>(
      WebAuthenticationProxyRegistrarFactory::GetInstance()
          ->GetServiceForBrowserContext(context, true));
}

std::unique_ptr<KeyedService>
WebAuthenticationProxyRegistrarFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<WebAuthenticationProxyRegistrar>(
      Profile::FromBrowserContext(context));
}

WebAuthenticationProxyService*
WebAuthenticationProxyService::GetIfProxyAttached(
    content::BrowserContext* browser_context) {
  auto* profile = Profile::FromBrowserContext(browser_context);
  switch (WebAuthenticationProxyRegistrarFactory::GetForBrowserContext(profile)
              ->ProxyActiveForProfile(profile)) {
    case WebAuthenticationProxyRegistrar::ProxyStatus::kActive:
      return WebAuthenticationProxyServiceFactory::GetForBrowserContext(
          profile);
    case WebAuthenticationProxyRegistrar::ProxyStatus::
        kActiveUseOriginalProfile:
      return WebAuthenticationProxyServiceFactory::GetForBrowserContext(
          profile->GetOriginalProfile());
    case WebAuthenticationProxyRegistrar::ProxyStatus::kInactive:
      return nullptr;
  }
}

WebAuthenticationProxyService::WebAuthenticationProxyService(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      event_router_(EventRouter::Get(browser_context)),
      extension_registry_(ExtensionRegistry::Get(browser_context)) {}

WebAuthenticationProxyService::~WebAuthenticationProxyService() = default;

const Extension* WebAuthenticationProxyService::GetActiveRequestProxy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!active_proxy_) {
    return nullptr;
  }
  const Extension* extension =
      extension_registry_->enabled_extensions().GetByID(*active_proxy_);
  DCHECK(extension);
  return extension;
}

void WebAuthenticationProxyService::CompleteCreateRequest(
    const api::web_authentication_proxy::CreateResponseDetails& details,
    RespondCallback respond_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto callback_it = pending_callbacks_.find(details.request_id);
  if (callback_it == pending_callbacks_.end() ||
      !absl::holds_alternative<CreateCallback>(callback_it->second)) {
    std::move(respond_callback).Run("Invalid requestId");
    return;
  }
  if (details.error) {
    // The proxied request yielded a DOMException.
    auto create_callback =
        absl::get<CreateCallback>(std::move(callback_it->second));
    pending_callbacks_.erase(callback_it);
    std::move(create_callback)
        .Run(details.request_id,
             blink::mojom::WebAuthnDOMExceptionDetails::New(
                 details.error->name, details.error->message),
             nullptr);
    std::move(respond_callback).Run(std::nullopt);
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
  auto callback_it = pending_callbacks_.find(details.request_id);
  if (callback_it == pending_callbacks_.end() ||
      !absl::holds_alternative<GetCallback>(callback_it->second)) {
    std::move(respond_callback).Run("Invalid requestId");
    return;
  }
  if (details.error) {
    // The proxied request yielded a DOMException.
    GetCallback callback =
        absl::get<GetCallback>(std::move(callback_it->second));
    pending_callbacks_.erase(callback_it);
    std::move(callback).Run(details.request_id,
                            blink::mojom::WebAuthnDOMExceptionDetails::New(
                                details.error->name, details.error->message),
                            nullptr);
    std::move(respond_callback).Run(std::nullopt);
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
  auto callback_it = pending_callbacks_.find(details.request_id);
  if (callback_it == pending_callbacks_.end() ||
      !absl::holds_alternative<IsUvpaaCallback>(callback_it->second)) {
    return false;
  }
  IsUvpaaCallback callback =
      absl::get<IsUvpaaCallback>(std::move(callback_it->second));
  pending_callbacks_.erase(callback_it);
  std::move(callback).Run(details.is_uvpaa);
  return true;
}

void WebAuthenticationProxyService::CancelRequest(RequestId request_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Extension* proxy_extension = GetActiveRequestProxy();
  DCHECK(proxy_extension);

  auto callback_it = pending_callbacks_.find(request_id);
  if (callback_it == pending_callbacks_.end() ||
      absl::holds_alternative<IsUvpaaCallback>(callback_it->second)) {
    // Invalid `request_id`. Note that isUvpaa requests cannot be cancelled.
    return;
  }
  pending_callbacks_.erase(request_id);
  event_router_->DispatchEventToExtension(
      proxy_extension->id(),
      std::make_unique<Event>(
          events::WEB_AUTHENTICATION_PROXY_REQUEST_CANCELLED,
          api::web_authentication_proxy::OnRequestCanceled::kEventName,
          api::web_authentication_proxy::OnRequestCanceled::Create(request_id),
          browser_context_));
}

void WebAuthenticationProxyService::SetRequestProxy(
    base::PassKey<WebAuthenticationProxyRegistrar>,
    const Extension* extension) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(extension);
  DCHECK(extension_registry_->enabled_extensions().Contains(extension->id()));
  DCHECK(!active_proxy_ || active_proxy_ == extension->id());
  active_proxy_ = extension->id();
}

void WebAuthenticationProxyService::ClearRequestProxy(
    base::PassKey<WebAuthenticationProxyRegistrar>) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(active_proxy_);
  CancelPendingCallbacks();
  active_proxy_.reset();
}

void WebAuthenticationProxyService::CancelPendingCallbacks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Complete all pending callbacks with a cancellation signal.
  for (auto it = pending_callbacks_.begin(); it != pending_callbacks_.end();) {
    auto& [request_id, callback] = *it;
    absl::visit(
        base::Overloaded{
            [](IsUvpaaCallback& cb) { std::move(cb).Run(/*is_uvpaa=*/false); },
            // CreateCallback or GetCallback:
            [id = request_id](auto& cb) {
              auto error = blink::mojom::WebAuthnDOMExceptionDetails::New(
                  "AbortError", "The operation was aborted.");
              std::move(cb).Run(id, std::move(error), nullptr);
            }},
        callback);
    it = pending_callbacks_.erase(it);
  }
}

WebAuthenticationProxyService::RequestId
WebAuthenticationProxyService::NewRequestId() {
  int32_t request_id =
      base::RandGenerator(std::numeric_limits<uint32_t>::max()) + 1;
  // Technically, this could spin forever if there are 4 billion active
  // requests. However, there's no real risk to this happening (no security or
  // DOS concerns).
  while (base::Contains(pending_callbacks_, request_id)) {
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
      webauthn::MakeCredentialResponseFromValue(*value_or_error);
  if (!response) {
    std::move(respond_callback).Run("Invalid responseJson: " + error);
    return;
  }

  auto callback_it = pending_callbacks_.find(request_id);
  if (callback_it == pending_callbacks_.end() ||
      !absl::holds_alternative<CreateCallback>(callback_it->second)) {
    // The request was canceled while waiting for JSON decoding.
    std::move(respond_callback).Run("Invalid requestId");
    return;
  }

  // Success.
  CreateCallback create_callback =
      absl::get<CreateCallback>(std::move(callback_it->second));
  pending_callbacks_.erase(callback_it);
  std::move(create_callback).Run(request_id, nullptr, std::move(response));
  std::move(respond_callback).Run(std::nullopt);
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
      webauthn::GetAssertionResponseFromValue(*value_or_error);
  if (!response) {
    std::move(respond_callback).Run("Invalid responseJson: " + error);
    return;
  }

  auto callback_it = pending_callbacks_.find(request_id);
  if (callback_it == pending_callbacks_.end() ||
      !absl::holds_alternative<GetCallback>(callback_it->second)) {
    // The request was canceled while waiting for JSON decoding.
    std::move(respond_callback).Run("Invalid requestId");
    return;
  }

  // Success.
  GetCallback get_callback =
      absl::get<GetCallback>(std::move(callback_it->second));
  pending_callbacks_.erase(callback_it);
  std::move(get_callback).Run(request_id, nullptr, std::move(response));
  std::move(respond_callback).Run(std::nullopt);
}

bool WebAuthenticationProxyService::IsActive(const url::Origin& caller_origin) {
  const Extension* proxy = GetActiveRequestProxy();
  return proxy && ProxyMayAttachToHost(*proxy, caller_origin);
}

WebAuthenticationProxyService::RequestId
WebAuthenticationProxyService::SignalCreateRequest(
    const blink::mojom::PublicKeyCredentialCreationOptionsPtr& options_ptr,
    CreateCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Extension* proxy_extension = GetActiveRequestProxy();
  DCHECK(proxy_extension);

  auto request_id = NewRequestId();
  pending_callbacks_.emplace(request_id, std::move(callback));

  api::web_authentication_proxy::CreateRequest request;
  request.request_id = request_id;

  base::Value options_value = webauthn::ToValue(options_ptr);
  std::string request_json;
  JSONStringValueSerializer serializer(&request.request_details_json);
  CHECK(serializer.Serialize(options_value));

  event_router_->DispatchEventToExtension(
      proxy_extension->id(),
      std::make_unique<Event>(
          events::WEB_AUTHENTICATION_PROXY_ON_CREATE_REQUEST,
          api::web_authentication_proxy::OnCreateRequest::kEventName,
          api::web_authentication_proxy::OnCreateRequest::Create(request),
          browser_context_));
  return request_id;
}

WebAuthenticationProxyService::RequestId
WebAuthenticationProxyService::SignalGetRequest(
    const blink::mojom::PublicKeyCredentialRequestOptionsPtr& options_ptr,
    GetCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Extension* proxy_extension = GetActiveRequestProxy();
  DCHECK(proxy_extension);

  auto request_id = NewRequestId();
  pending_callbacks_.emplace(request_id, std::move(callback));

  api::web_authentication_proxy::GetRequest request;
  request.request_id = request_id;

  base::Value options_value = webauthn::ToValue(options_ptr);
  std::string request_json;
  JSONStringValueSerializer serializer(&request.request_details_json);
  CHECK(serializer.Serialize(options_value));

  event_router_->DispatchEventToExtension(
      proxy_extension->id(),
      std::make_unique<Event>(
          events::WEB_AUTHENTICATION_PROXY_ON_GET_REQUEST,
          api::web_authentication_proxy::OnGetRequest::kEventName,
          api::web_authentication_proxy::OnGetRequest::Create(request),
          browser_context_));
  return request_id;
}

WebAuthenticationProxyService::RequestId
WebAuthenticationProxyService::SignalIsUvpaaRequest(IsUvpaaCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const Extension* proxy_extension = GetActiveRequestProxy();
  DCHECK(proxy_extension);

  auto request_id = NewRequestId();
  pending_callbacks_.emplace(request_id, std::move(callback));
  api::web_authentication_proxy::IsUvpaaRequest request;
  request.request_id = request_id;
  event_router_->DispatchEventToExtension(
      proxy_extension->id(),
      std::make_unique<Event>(
          events::WEB_AUTHENTICATION_PROXY_ON_ISUVPAA_REQUEST,
          api::web_authentication_proxy::OnIsUvpaaRequest::kEventName,
          api::web_authentication_proxy::OnIsUvpaaRequest::Create(request),
          browser_context_));
  return request_id;
}

WebAuthenticationProxyServiceFactory*
WebAuthenticationProxyServiceFactory::GetInstance() {
  static base::NoDestructor<WebAuthenticationProxyServiceFactory> instance;
  return instance.get();
}

WebAuthenticationProxyServiceFactory::WebAuthenticationProxyServiceFactory()
    : ProfileKeyedServiceFactory(
          "WebAuthenticationProxyService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
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
