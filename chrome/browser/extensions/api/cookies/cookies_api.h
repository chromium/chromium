// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions Cookies API functions for accessing internet
// cookies, as specified in the extension API JSON.

#ifndef CHROME_BROWSER_EXTENSIONS_API_COOKIES_COOKIES_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_COOKIES_COOKIES_API_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/common/extensions/api/cookies.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"

class Profile;

namespace extensions {

// Observes CookieManager Mojo messages and routes them as events to the
// extension system.
class CookiesEventRouter : public ProfileObserver {
 public:
  explicit CookiesEventRouter(content::BrowserContext* context);

  CookiesEventRouter(const CookiesEventRouter&) = delete;
  CookiesEventRouter& operator=(const CookiesEventRouter&) = delete;

  ~CookiesEventRouter() override;

  // ProfileObserver:
  void OnOffTheRecordProfileCreated(Profile* off_the_record) override;

 private:
  // This helper class connects to the CookieMonster over Mojo, and relays Mojo
  // messages to the owning CookiesEventRouter. This rather clumsy arrangement
  // is necessary to differentiate which CookieMonster the Mojo message comes
  // from (that associated with the incognito profile vs the original profile),
  // since it's not possible to tell the source from inside OnCookieChange().
  class CookieChangeListener : public network::mojom::CookieChangeListener {
   public:
    CookieChangeListener(CookiesEventRouter* router, bool otr);

    CookieChangeListener(const CookieChangeListener&) = delete;
    CookieChangeListener& operator=(const CookieChangeListener&) = delete;

    ~CookieChangeListener() override;

    // network::mojom::CookieChangeListener:
    void OnCookieChange(const net::CookieChangeInfo& change) override;

   private:
    raw_ptr<CookiesEventRouter> router_;
    bool otr_;
  };

  void MaybeStartListening();
  void BindToCookieManager(
      mojo::Receiver<network::mojom::CookieChangeListener>* receiver,
      Profile* profile);
  void OnConnectionError(
      mojo::Receiver<network::mojom::CookieChangeListener>* receiver);
  void OnCookieChange(bool otr, const net::CookieChangeInfo& change);

  // This method dispatches events to the extension message service.
  void DispatchEvent(content::BrowserContext* context,
                     events::HistogramValue histogram_value,
                     const std::string& event_name,
                     base::Value::List event_args,
                     const GURL& cookie_domain);

  raw_ptr<Profile> profile_;

  base::ScopedObservation<Profile, ProfileObserver> profile_observation_;

  // To listen to cookie changes in both the original and the off the record
  // profiles, we need a pair of bindings, as well as a pair of
  // CookieChangeListener instances.
  CookieChangeListener listener_{this, false};
  mojo::Receiver<network::mojom::CookieChangeListener> receiver_{&listener_};

  CookieChangeListener otr_listener_{this, true};
  mojo::Receiver<network::mojom::CookieChangeListener> otr_receiver_{
      &otr_listener_};
};

// Implements the cookies.get() extension function.
class CookiesGetFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("cookies.get", COOKIES_GET)

  CookiesGetFunction();

 protected:
  ~CookiesGetFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void GetCookieListCallback(
      const net::CookieAccessResultList& cookie_list,
      const net::CookieAccessResultList& excluded_cookies);

  // Notify the extension telemetry service when API is called.
  void NotifyExtensionTelemetry();

  GURL url_;
  mojo::Remote<network::mojom::CookieManager> store_browser_cookie_manager_;
  std::optional<api::cookies::Get::Params> parsed_args_;
};

// Implements the cookies.getAll() extension function.
class CookiesGetAllFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("cookies.getAll", COOKIES_GETALL)

  CookiesGetAllFunction();

 protected:
  ~CookiesGetAllFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  // For the two different callback signatures for getting cookies for a URL vs
  // getting all cookies. They do the same thing.
  void GetAllCookiesCallback(const net::CookieList& cookie_list);
  void GetCookieListCallback(
      const net::CookieAccessResultList& cookie_list,
      const net::CookieAccessResultList& excluded_cookies);

  // Notify the extension telemetry service when API is called.
  void NotifyExtensionTelemetry();

  GURL url_;
  mojo::Remote<network::mojom::CookieManager> store_browser_cookie_manager_;
  std::optional<api::cookies::GetAll::Params> parsed_args_;
};

// Implements the cookies.set() extension function.
class CookiesSetFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("cookies.set", COOKIES_SET)

  CookiesSetFunction();

 protected:
  ~CookiesSetFunction() override;
  ResponseAction Run() override;

 private:
  void SetCanonicalCookieCallback(net::CookieAccessResult set_cookie_result);
  void GetCookieListCallback(
      const net::CookieAccessResultList& cookie_list,
      const net::CookieAccessResultList& excluded_cookies);

  enum { NO_RESPONSE, SET_COMPLETED, GET_COMPLETED } state_;
  GURL url_;
  bool success_;
  mojo::Remote<network::mojom::CookieManager> store_browser_cookie_manager_;
  std::optional<api::cookies::Set::Params> parsed_args_;
};

// Implements the cookies.remove() extension function.
class CookiesRemoveFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("cookies.remove", COOKIES_REMOVE)

  CookiesRemoveFunction();

 protected:
  ~CookiesRemoveFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void RemoveCookieCallback(uint32_t /* num_deleted */);

  GURL url_;
  mojo::Remote<network::mojom::CookieManager> store_browser_cookie_manager_;
  std::optional<api::cookies::Remove::Params> parsed_args_;
};

// Implements the cookies.getAllCookieStores() extension function.
class CookiesGetAllCookieStoresFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("cookies.getAllCookieStores",
                             COOKIES_GETALLCOOKIESTORES)

 protected:
  ~CookiesGetAllCookieStoresFunction() override {}

  // ExtensionFunction:
  ResponseAction Run() override;
};

class CookiesAPI : public BrowserContextKeyedAPI, public EventRouter::Observer {
 public:
  explicit CookiesAPI(content::BrowserContext* context);

  CookiesAPI(const CookiesAPI&) = delete;
  CookiesAPI& operator=(const CookiesAPI&) = delete;

  ~CookiesAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<CookiesAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<CookiesAPI>;

  raw_ptr<content::BrowserContext> browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "CookiesAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<CookiesEventRouter> cookies_event_router_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_COOKIES_COOKIES_API_H_
