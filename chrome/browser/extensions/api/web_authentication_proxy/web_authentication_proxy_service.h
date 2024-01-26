// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEB_AUTHENTICATION_PROXY_WEB_AUTHENTICATION_PROXY_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEB_AUTHENTICATION_PROXY_WEB_AUTHENTICATION_PROXY_SERVICE_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/profiles/profile_observer.h"
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
class ExtensionRegistry;

// WebAuthenticationProxyRegistrar keeps track of extensions attaching
// themselves via the webAuthenticationRequestProxy API to intercept WebAuthn
// request processing.
//
// You can obtain the instance for a given profile from
// WebAuthenticationProxyRegistrarFactory. Off-the-record profiles like
// incognito are redirected to their "regular" main profile.
class WebAuthenticationProxyRegistrar : public KeyedService,
                                        public ExtensionRegistryObserver,
                                        public ProfileObserver {
 public:
  explicit WebAuthenticationProxyRegistrar(Profile* profile);
  ~WebAuthenticationProxyRegistrar() override;

  // Sets the active request proxy. `profile` must be associated with this
  // instance (i.e. the regular profile or an associated off-the-record
  // profile). `extension` must be an enabled extension.
  //
  // Incognito-enabled spanning mode extensions will automatically be set on any
  // current and and future associated off-the-record profile. For split mode,
  // you need to set them separately. However, you cannot set different
  // extensions as proxies for regular and incognito mode.
  //
  // Returns true if `extension` is the active proxy upon return, or false if
  // another extension was already active.
  bool SetRequestProxy(Profile* profile, const Extension* extension);

  // Clears the currently active request proxy extension. `profile` must be
  // associated with this instance.
  //
  // If the active proxy is an incognito-enabled spanning mode extension, it
  // will be cleared from regular and incognito profiles. Split mode extensions
  // must be cleared separately.
  void ClearRequestProxy(Profile* profile);

  // Indicates whether a proxy is active for a given profile.
  enum class ProxyStatus {
    // The proxy service is active for this profile.
    kActive,
    // The proxy service for the original profile to this incognito profile is
    // active and handles both contexts.
    kActiveUseOriginalProfile,
    // No active proxy for this profile.
    kInactive,
  };

  ProxyStatus ProxyActiveForProfile(Profile* profile);

 private:
  using PassKey = base::PassKey<WebAuthenticationProxyRegistrar>;
  friend class WebAuthenticationProxyRegistrarFactory;

  // ExtensionRegistryObserver:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // ProfileObserver:
  void OnOffTheRecordProfileCreated(Profile* off_the_record) override;
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // The extension that is currently acting as the WebAuthn request proxy, if
  // any. An extension becomes the active proxy by calling `attach()`. It
  // unregisters by calling `detach()` or getting unloaded.
  std::optional<ExtensionId> active_regular_proxy_;

  // Set if `active_regular_proxy_` is a spanning mode extension which should
  // attach to associated incognito profiles too.
  bool attach_regular_proxy_to_both_contexts_ = false;

  // A split mode extension that is the current proxy for the associated
  // incognito profile. If set, `attach_regular_proxy_contexts_` must be false.
  // But `active_regular_proxy_` may still be true.
  std::optional<ExtensionId> active_otr_split_proxy_;

  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<ExtensionRegistry> extension_registry_ = nullptr;
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
};

// WebAuthenticationProxyRegistrarFactory  creates instances of
// WebAuthenticationProxyRegistrar for a given BrowserContext.
class WebAuthenticationProxyRegistrarFactory
    : public ProfileKeyedServiceFactory {
 public:
  static WebAuthenticationProxyRegistrarFactory* GetInstance();

  static WebAuthenticationProxyRegistrar* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend class base::NoDestructor<WebAuthenticationProxyRegistrarFactory>;

  WebAuthenticationProxyRegistrarFactory();
  ~WebAuthenticationProxyRegistrarFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

// WebAuthenticationProxyService is an implementation of the
// content::WebAuthenticationRequestProxy interface that integrates Chrome's Web
// Authentication API with the webAuthenticationProxy extension API.
//
// Use GetIfProxyAttached() to obtain an instance of this class.
class WebAuthenticationProxyService
    : public content::WebAuthenticationRequestProxy,
      public KeyedService {
 public:
  using RespondCallback = base::OnceCallback<void(std::optional<std::string>)>;

  // Returns the service instance for the given BrowserContext, if a proxy is
  // currently attached, and nulltpr otherwise. References to this class should
  // not be stored because they become invalid whenever the proxy detaches.
  //
  // Service instances are shared between incognito and regular contexts if the
  // attached proxy is incognito-enabled in spanning mode. Otherwise, regular
  // and incognito contexts use separate instances.
  static WebAuthenticationProxyService* GetIfProxyAttached(
      content::BrowserContext* browser_context);

  // Returns the extension registered as the request proxy or `nullptr` if none
  // is active.
  const Extension* GetActiveRequestProxy();

  // Injects the result for the `onCreateRequest` extension API event with
  // `EventId` matching the one in `details`.
  //
  // On completion, `callback` is invoked with an error or `std::nullopt` on
  // success.
  void CompleteCreateRequest(
      const api::web_authentication_proxy::CreateResponseDetails& details,
      RespondCallback callback);

  // Injects the result for the `onGetRequest` extension API event with
  // `EventId` matching the one in `details`.
  //
  // On completion, `callback` is invoked with an error or `std::nullopt` on
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

  // Sets or clears the current request proxy. This should only be called by
  // WebAuthenticationProxyRegistrar; external callers should set the proxy
  // through that class.
  void SetRequestProxy(base::PassKey<WebAuthenticationProxyRegistrar>,
                       const Extension* extension);
  void ClearRequestProxy(base::PassKey<WebAuthenticationProxyRegistrar>);

  // content::WebAuthenticationRequestProxy:
  bool IsActive(const url::Origin& caller_origin) override;
  RequestId SignalCreateRequest(
      const blink::mojom::PublicKeyCredentialCreationOptionsPtr& options,
      CreateCallback callback) override;
  RequestId SignalGetRequest(
      const blink::mojom::PublicKeyCredentialRequestOptionsPtr& options,
      GetCallback callback) override;
  RequestId SignalIsUvpaaRequest(IsUvpaaCallback callback) override;
  void CancelRequest(RequestId request_id) override;

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

  raw_ptr<content::BrowserContext> browser_context_ = nullptr;
  raw_ptr<EventRouter> event_router_ = nullptr;
  raw_ptr<ExtensionRegistry> extension_registry_ = nullptr;

  // The active proxy extension for this instance's profile, if any.
  std::optional<ExtensionId> active_proxy_;

  using CallbackType =
      absl::variant<IsUvpaaCallback, CreateCallback, GetCallback>;
  std::map<RequestId, CallbackType> pending_callbacks_;

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
