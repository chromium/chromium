// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEB_AUTHENTICATION_PROXY_WEB_AUTHENTICATION_PROXY_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEB_AUTHENTICATION_PROXY_WEB_AUTHENTICATION_PROXY_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/common/extensions/api/web_authentication_proxy.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/web_authentication_request_proxy.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

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
      public KeyedService,
      public ExtensionRegistryObserver {
 public:
  using RespondCallback = base::OnceCallback<void(absl::optional<std::string>)>;

  // Returns the extension registered as the request proxy, or `nullptr` if none
  // is active.
  const Extension* GetActiveRequestProxy();

  // Registers a new active request proxy. `extension` must be an enabled
  // extension. No other extension may currently be set; call
  // ClearActiveRequestProxy() first to unregister an active proxy.
  void SetActiveRequestProxy(const Extension* extension);

  // Unregisters the currently active request proxy extension, if any.
  void ClearActiveRequestProxy();

  // Injects the result for the `onCreateRequest` extension API event
  // with `EventId` matching the one in `details`.
  //
  // On completion, `callback` is invoked with an error or `absl::nullopt` on
  // success.
  void CompleteCreateRequest(
      const api::web_authentication_proxy::CreateResponseDetails& details,
      RespondCallback callback);

  // Injects the result for the `onGetRequest` extension API event with
  // `EventId` matching the one in `details`.
  //
  // On completion, `callback` is invoked with an error or `absl::nullopt` on
  // success.
  void CompleteGetRequest(
      const api::web_authentication_proxy::GetResponseDetails& details,
      RespondCallback callback);

  // Injects the result for the
  // `events::WEB_AUTHENTICATION_PROXY_ON_ISUVPAA_REQUEST` event with
  // `event_id`. `is_uvpaa` is the result to be returned to the original caller
  // of the PublicKeyCredential.IsUserPlatformAuthenticatorAvailable().
  //
  // Returns whether the ID was valid.
  bool CompleteIsUvpaaRequest(
      const api::web_authentication_proxy::IsUvpaaResponseDetails& details);

 private:
  friend class WebAuthenticationProxyServiceFactory;

  explicit WebAuthenticationProxyService(
      content::BrowserContext* browser_context);
  ~WebAuthenticationProxyService() override;

  void CancelPendingCallbacks();
  RequestId NewRequestId();
  void OnParseCreateResponse(
      RespondCallback respondCallback,
      RequestId request_id,
      data_decoder::DataDecoder::ValueOrError value_or_error);
  void OnParseGetResponse(
      RespondCallback respondCallback,
      RequestId request_id,
      data_decoder::DataDecoder::ValueOrError value_or_error);

  // content::WebAuthenticationRequestProxy:
  bool IsActive() override;
  RequestId SignalCreateRequest(
      const blink::mojom::PublicKeyCredentialCreationOptionsPtr& options,
      CreateCallback callback) override;
  RequestId SignalGetRequest(
      const blink::mojom::PublicKeyCredentialRequestOptionsPtr& options,
      GetCallback callback) override;
  RequestId SignalIsUvpaaRequest(IsUvpaaCallback callback) override;
  void CancelRequest(RequestId request_id) override;

  // ExtensionRegistryObserver:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};

  raw_ptr<EventRouter> event_router_ = nullptr;
  raw_ptr<ExtensionRegistry> extension_registry_ = nullptr;

  // The extension that is currently acting as the WebAuthn request proxy, if
  // any. An extension becomes the active proxy by calling `attach()`. It
  // unregisters by calling `detach()` or getting unloaded.
  absl::optional<std::string> active_request_proxy_extension_id_;

  std::map<RequestId, IsUvpaaCallback> pending_is_uvpaa_callbacks_;
  std::map<RequestId, CreateCallback> pending_create_callbacks_;
  std::map<RequestId, GetCallback> pending_get_callbacks_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<WebAuthenticationProxyService> weak_ptr_factory_{this};
};

// WebAuthenticationProxyServiceFactory creates instances of
// WebAuthenticationProxyService for a given BrowserContext.
class WebAuthenticationProxyServiceFactory : public ProfileKeyedServiceFactory {
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
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEB_AUTHENTICATION_PROXY_WEB_AUTHENTICATION_PROXY_SERVICE_H_
