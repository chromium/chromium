// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_cookies.h"

#include <optional>

#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/chrome_extension_cookies_factory.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/cookie_config/cookie_store_util.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_store_factory.h"
#include "extensions/common/constants.h"
#include "net/cookies/cookie_partition_key_collection.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "services/network/cookie_manager.h"
#include "services/network/restricted_cookie_manager.h"

namespace extensions {

ChromeExtensionCookies::ChromeExtensionCookies(Profile* profile)
    : profile_(profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  cookie_settings_ = CookieSettingsFactory::GetForProfile(profile);
  cookie_settings_observation_.Observe(cookie_settings_.get());
  HostContentSettingsMapFactory::GetForProfile(profile_)->AddObserver(this);

  std::optional<content::CookieStoreConfig> creation_config;

  if (profile_->IsIncognitoProfile() || profile_->AsTestingProfile()) {
    creation_config.emplace(content::CookieStoreConfig());
  } else {
    creation_config.emplace(content::CookieStoreConfig(
        profile_->GetPath().Append(chrome::kExtensionsCookieFilename),
        profile_->ShouldRestoreOldSessionCookies(),
        profile_->ShouldPersistSessionCookies()));
    creation_config->crypto_delegate = cookie_config::GetCookieCryptoDelegate();
  }
  creation_config->cookieable_schemes.push_back(extensions::kExtensionScheme);

  network::mojom::CookieManagerParamsPtr initial_settings =
      ProfileNetworkContextService::CreateCookieManagerParams(
          profile_, *cookie_settings_);

  io_data_ = std::make_unique<IOData>(std::move(*creation_config),
                                      std::move(initial_settings));
}

ChromeExtensionCookies::~ChromeExtensionCookies() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!io_data_);
}

// static
ChromeExtensionCookies* ChromeExtensionCookies::Get(
    content::BrowserContext* context) {
  return ChromeExtensionCookiesFactory::GetForBrowserContext(context);
}

void ChromeExtensionCookies::CreateRestrictedCookieManager(
    const url::Origin& origin,
    const net::IsolationInfo& isolation_info,
    mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!io_data_)
    return;

  // Safe since |io_data_| is non-null so no IOData deletion is queued.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &IOData::ComputeFirstPartySetMetadataAndCreateRestrictedCookieManager,
          base::Unretained(io_data_.get()), origin, isolation_info,
          std::move(receiver)));
}

void ChromeExtensionCookies::ClearCookies(const GURL& origin,
                                          base::OnceClosure done_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!io_data_)  // null after shutdown.
    return;

  auto callback_wrapper = [](base::OnceClosure done_callback, uint32_t result) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                                 std::move(done_callback));
  };
  // Safe since |io_data_| is non-null so no IOData deletion is queued.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &IOData::ClearCookies, base::Unretained(io_data_.get()), origin,
          base::BindOnce(callback_wrapper, std::move(done_callback))));
}

net::CookieStore* ChromeExtensionCookies::GetCookieStoreForTesting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!io_data_)  // null after shutdown.
    return nullptr;

  return io_data_->GetOrCreateCookieStore();
}

ChromeExtensionCookies::IOData::IOData(
    content::CookieStoreConfig creation_config,
    network::mojom::CookieManagerParamsPtr initial_mojo_cookie_settings)
    : creation_config_(std::move(creation_config)),
      mojo_cookie_settings_(std::move(initial_mojo_cookie_settings)) {
  UpdateNetworkCookieSettings();
}

ChromeExtensionCookies::IOData::~IOData() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void ChromeExtensionCookies::IOData::
    ComputeFirstPartySetMetadataAndCreateRestrictedCookieManager(
        const url::Origin& origin,
        const net::IsolationInfo& isolation_info,
        mojo::PendingReceiver<network::mojom::RestrictedCookieManager>
            receiver) {
  network::RestrictedCookieManager::ComputeFirstPartySetMetadata(
      origin, GetOrCreateCookieStore(), isolation_info,
      base::BindOnce(&IOData::CreateRestrictedCookieManager,
                     weak_factory_.GetWeakPtr(), origin, isolation_info,
                     std::move(receiver)));
}

void ChromeExtensionCookies::IOData::CreateRestrictedCookieManager(
    const url::Origin& origin,
    const net::IsolationInfo& isolation_info,
    mojo::PendingReceiver<network::mojom::RestrictedCookieManager> receiver,
    net::FirstPartySetMetadata first_party_set_metadata) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // TODO(crbug.com/40247160): Consider whether the following check should
  // somehow determine real CookieSettingOverrides rather than default to none.
  restricted_cookie_managers_.Add(
      std::make_unique<network::RestrictedCookieManager>(
          network::mojom::RestrictedCookieManagerRole::SCRIPT,
          GetOrCreateCookieStore(), network_cookie_settings_, origin,
          isolation_info, net::CookieSettingOverrides(),
          /* null cookies_observer disables logging */
          mojo::NullRemote(), std::move(first_party_set_metadata)),
      std::move(receiver));
}

void ChromeExtensionCookies::IOData::ClearCookies(
    const GURL& origin,
    net::CookieStore::DeleteCallback done_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  net::CookieDeletionInfo delete_info;
  delete_info.host = origin.host();
  GetOrCreateCookieStore()->DeleteAllMatchingInfoAsync(
      std::move(delete_info), std::move(done_callback));
}

void ChromeExtensionCookies::IOData::OnContentSettingChanged(
    ContentSettingsForOneType settings) {
  mojo_cookie_settings_->content_settings[ContentSettingsType::COOKIES] =
      std::move(settings);
  UpdateNetworkCookieSettings();
}

void ChromeExtensionCookies::IOData::OnThirdPartyCookieBlockingChanged(
    bool block_third_party_cookies) {
  mojo_cookie_settings_->block_third_party_cookies = block_third_party_cookies;
  UpdateNetworkCookieSettings();
}

net::CookieStore* ChromeExtensionCookies::IOData::GetOrCreateCookieStore() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!cookie_store_) {
    cookie_store_ = content::CreateCookieStore(std::move(creation_config_),
                                               nullptr /* netlog */);
  }
  return cookie_store_.get();
}

void ChromeExtensionCookies::IOData::UpdateNetworkCookieSettings() {
  network::CookieManager::ConfigureCookieSettings(*mojo_cookie_settings_,
                                                  &network_cookie_settings_);
}

void ChromeExtensionCookies::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!io_data_)  // null after shutdown.
    return;

  if (!content_type_set.Contains(ContentSettingsType::COOKIES))
    return;

  ContentSettingsForOneType settings =
      HostContentSettingsMapFactory::GetForProfile(profile_)
          ->GetSettingsForOneType(ContentSettingsType::COOKIES);

  // Safe since |io_data_| is non-null so no IOData deletion is queued.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&IOData::OnContentSettingChanged,
                     base::Unretained(io_data_.get()), std::move(settings)));
}

void ChromeExtensionCookies::OnThirdPartyCookieBlockingChanged(
    bool block_third_party_cookies) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!io_data_)  // null after shutdown.
    return;

  // Safe since |io_data_| is non-null so no IOData deletion is queued.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&IOData::OnThirdPartyCookieBlockingChanged,
                                base::Unretained(io_data_.get()),
                                block_third_party_cookies));
}

void ChromeExtensionCookies::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Async delete on IO thread, sequencing it after any previously posted
  // operations.
  //
  // Note: during tests this may be called with IO thread == UI thread. If this
  // were to use unique_ptr<.., DeleteOnIOThread> that case would result in
  // unwanted synchronous deletion; hence DeleteSoon is used by hand.
  content::GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE,
                                                 std::move(io_data_));
  profile_ = nullptr;
}

}  // namespace extensions
