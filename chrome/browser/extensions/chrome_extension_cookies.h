// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_COOKIES_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_COOKIES_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/cookie_store_factory.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "net/cookies/cookie_store.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "services/network/cookie_settings.h"
#include "services/network/public/mojom/cookie_manager.mojom-forward.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom.h"

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace net {
class CookieStore;
class IsolationInfo;
}

namespace url {
class Origin;
}

namespace extensions {

// Manages cookie store for chrome-extension:// URLs, and associated
// RestrictedCookieManager objects. All public APIs are for UI thread use.
class ChromeExtensionCookies
    : public KeyedService,
      public content_settings::Observer,
      public content_settings::CookieSettings::Observer {
 public:
  explicit ChromeExtensionCookies(Profile* profile);
  ~ChromeExtensionCookies() override;
  ChromeExtensionCookies(const ChromeExtensionCookies&) = delete;
  ChromeExtensionCookies& operator=(const ChromeExtensionCookies&) = delete;

  // Gets (or creates) an appropriate instance for given |context| from
  // ChromeExtensionCookiesFactory.
  static ChromeExtensionCookies* Get(content::BrowserContext* context);

  // Creates a RestrictedCookieManager for a chrome-extension:// URL
  // with origin |origin|, bound to |receiver|. Whether this will use disk
  // storage or not depends on the Profile |this| was created for.
  void CreateRestrictedCookieManager(
      const url::Origin& origin,
      const net::IsolationInfo& isolation_info,
      mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver);

  // Deletes all cookies matching the host of |origin| and
  // synchronously invokes |done_callback| once all cookies are deleted.
  void ClearCookies(const GURL& origin, base::OnceClosure done_callback);

  // Test-only method to get the raw underlying test store. This can only be
  // called when the UI thread and the IO thread are actually the same thread
  // (e.g. if BrowserTaskEnvironment is in use).
  net::CookieStore* GetCookieStoreForTesting();

 private:
  friend class ChromeExtensionCookiesFactory;

  // State lives on the IO thread, and operations performed there.
  class IOData {
   public:
    IOData(content::CookieStoreConfig creation_config,
           network::mojom::CookieManagerParamsPtr initial_mojo_cookie_settings);

    IOData(const IOData&) = delete;
    IOData& operator=(const IOData&) = delete;

    ~IOData();

    // Computes the First-Party Set metadata associated with this instance, and
    // finishes creating the RestrictedCookieManager.
    //
    // The RestrictedCookieManager instance may be created either synchronously
    // or asynchronously.
    void ComputeFirstPartySetMetadataAndCreateRestrictedCookieManager(
        const url::Origin& origin,
        const net::IsolationInfo& isolation_info,
        mojo::PendingReceiver<network::mojom::RestrictedCookieManager>
            receiver);

    // Asynchronously deletes all cookie info matching |origin| and
    // synchronously invokes |done_callback| once all cookie info is deleted.
    void ClearCookies(const GURL& origin,
                      net::CookieStore::DeleteCallback done_callback);

    void OnContentSettingChanged(ContentSettingsForOneType settings);
    void OnThirdPartyCookieBlockingChanged(bool block_third_party_cookies);

    net::CookieStore* GetOrCreateCookieStore();

   private:
    // Syncs |mojo_cookie_settings_| -> |network_cookie_settings_|.
    void UpdateNetworkCookieSettings();

    // Asynchronously creates a RestrictedCookieManager.
    void CreateRestrictedCookieManager(
        const url::Origin& origin,
        const net::IsolationInfo& isolation_info,
        mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver,
        net::FirstPartySetMetadata first_party_set_metadata);

    content::CookieStoreConfig creation_config_;

    std::unique_ptr<net::CookieStore> cookie_store_;
    // Cookie blocking preferences in form RestrictedCookieManager needs.
    network::CookieSettings network_cookie_settings_;

    // Intermediate form needed for |cookie_settings|_ ->
    // |network_cookie_settings_| conversion.
    network::mojom::CookieManagerParamsPtr mojo_cookie_settings_;

    mojo::UniqueReceiverSet<network::mojom::RestrictedCookieManager>
        restricted_cookie_managers_;

    base::WeakPtrFactory<IOData> weak_factory_{this};
  };

  // content_settings::Observer:
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;

  // content_settings::CookieSettings::Observer:
  void OnThirdPartyCookieBlockingChanged(
      bool block_third_party_cookies) override;

  // KeyedService:
  void Shutdown() override;

  raw_ptr<Profile> profile_ = nullptr;

  // Lives on the IO thread, null after Shutdown().
  std::unique_ptr<IOData> io_data_;

  // Cookie config Chrome-side.
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  base::ScopedObservation<content_settings::CookieSettings,
                          content_settings::CookieSettings::Observer>
      cookie_settings_observation_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_COOKIES_H_
