// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_COOKIES_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_COOKIES_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observer.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/network/cookie_settings.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom.h"

class Profile;

namespace content {
class BrowserContext;
struct CookieStoreConfig;
}  // namespace content

namespace net {
class CookieStore;
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
  // Gets (or creates) an appropriate instance for given |context| from
  // ChromeExtensionCookiesFactory.
  static ChromeExtensionCookies* Get(content::BrowserContext* context);

  // Creates a RestrictedCookieManager for a chrome-extension:// URL
  // with origin |origin|, bound to |receiver|. Whether this will use disk
  // storage or not depends on the Profile |this| was created for.
  void CreateRestrictedCookieManager(
      const url::Origin& origin,
      const GURL& site_for_cookies,
      const url::Origin& top_frame_origin,
      mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver);

  // Deletes all cookies matching the host of |origin|.
  void ClearCookies(const GURL& origin);

  // Test-only method to get the raw underlying test store. This can only be
  // called when the UI thread and the IO thread are actually the same thread
  // (e.g. if BrowserTaskEnvironment is in use).
  net::CookieStore* GetCookieStoreForTesting();

 private:
  friend class ChromeExtensionCookiesFactory;

  // State lives on the IO thread, and operations performed there.
  class IOData {
   public:
    IOData(std::unique_ptr<content::CookieStoreConfig> creation_config,
           network::mojom::CookieManagerParamsPtr initial_mojo_cookie_settings);
    ~IOData();

    void CreateRestrictedCookieManager(
        const url::Origin& origin,
        const GURL& site_for_cookies,
        const url::Origin& top_frame_origin,
        mojo::PendingReceiver<network::mojom::RestrictedCookieManager>
            receiver);
    void ClearCookies(const GURL& origin);

    void OnContentSettingChanged(ContentSettingsForOneType settings);
    void OnThirdPartyCookieBlockingChanged(bool block_third_party_cookies);

    net::CookieStore* GetOrCreateCookieStore();

   private:
    // Syncs |mojo_cookie_settings_| -> |network_cookie_settings_|.
    void UpdateNetworkCookieSettings();

    std::unique_ptr<content::CookieStoreConfig> creation_config_;

    std::unique_ptr<net::CookieStore> cookie_store_;
    // Cookie blocking preferences in form RestrictedCookieManager needs.
    network::CookieSettings network_cookie_settings_;

    // Intermediate form needed for |cookie_settings|_ ->
    // |network_cookie_settings_| conversion.
    network::mojom::CookieManagerParamsPtr mojo_cookie_settings_;

    mojo::UniqueReceiverSet<network::mojom::RestrictedCookieManager>
        restricted_cookie_managers_;

    DISALLOW_COPY_AND_ASSIGN(IOData);
  };

  explicit ChromeExtensionCookies(Profile* profile);
  ~ChromeExtensionCookies() override;

  // content_settings::Observer:
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type,
                               const std::string& resource_identifier) override;

  // content_settings::CookieSettings::Observer:
  void OnThirdPartyCookieBlockingChanged(
      bool block_third_party_cookies) override;

  // KeyedService:
  void Shutdown() override;

  Profile* profile_ = nullptr;

  // Lives on the IO thread, null after Shutdown().
  std::unique_ptr<IOData> io_data_;

  // Cookie config Chrome-side.
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  ScopedObserver<content_settings::CookieSettings,
                 content_settings::CookieSettings::Observer>
      cookie_settings_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(ChromeExtensionCookies);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_COOKIES_H_
