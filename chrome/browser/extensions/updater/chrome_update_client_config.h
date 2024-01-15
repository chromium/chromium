// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_UPDATER_CHROME_UPDATE_CLIENT_CONFIG_H_
#define CHROME_BROWSER_EXTENSIONS_UPDATER_CHROME_UPDATE_CLIENT_CONFIG_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/component_updater/configurator_impl.h"
#include "components/update_client/configurator.h"

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace content {
class BrowserContext;
}

namespace update_client {
class CrxDownloaderFactory;
class NetworkFetcherFactory;
class ProtocolHandlerFactory;
}  // namespace update_client

namespace extensions {

class ExtensionUpdateClientBaseTest;

class ChromeUpdateClientConfig : public update_client::Configurator {
 public:
  using FactoryCallback =
      base::RepeatingCallback<scoped_refptr<ChromeUpdateClientConfig>(
          content::BrowserContext* context)>;

  static scoped_refptr<ChromeUpdateClientConfig> Create(
      content::BrowserContext* context,
      std::optional<GURL> url_override);

  ChromeUpdateClientConfig(content::BrowserContext* context,
                           std::optional<GURL> url_override);

  ChromeUpdateClientConfig(const ChromeUpdateClientConfig&) = delete;
  ChromeUpdateClientConfig& operator=(const ChromeUpdateClientConfig&) = delete;

  base::TimeDelta InitialDelay() const override;
  base::TimeDelta NextCheckDelay() const override;
  base::TimeDelta OnDemandDelay() const override;
  base::TimeDelta UpdateDelay() const override;
  std::vector<GURL> UpdateUrl() const override;
  std::vector<GURL> PingUrl() const override;
  std::string GetProdId() const override;
  base::Version GetBrowserVersion() const override;
  std::string GetChannel() const override;
  std::string GetLang() const override;
  std::string GetOSLongName() const override;
  base::flat_map<std::string, std::string> ExtraRequestParams() const override;
  std::string GetDownloadPreference() const override;
  scoped_refptr<update_client::NetworkFetcherFactory> GetNetworkFetcherFactory()
      override;
  scoped_refptr<update_client::CrxDownloaderFactory> GetCrxDownloaderFactory()
      override;
  scoped_refptr<update_client::UnzipperFactory> GetUnzipperFactory() override;
  scoped_refptr<update_client::PatcherFactory> GetPatcherFactory() override;
  bool EnabledDeltas() const override;
  bool EnabledBackgroundDownloader() const override;
  bool EnabledCupSigning() const override;
  PrefService* GetPrefService() const override;
  update_client::PersistedData* GetPersistedData() const override;
  bool IsPerUserInstall() const override;
  std::unique_ptr<update_client::ProtocolHandlerFactory>
  GetProtocolHandlerFactory() const override;
  std::optional<bool> IsMachineExternallyManaged() const override;
  update_client::UpdaterStateProvider GetUpdaterStateProvider() const override;
  std::optional<base::FilePath> GetCrxCachePath() const override;
  bool IsConnectionMetered() const override;

 protected:
  friend class base::RefCountedThreadSafe<ChromeUpdateClientConfig>;
  friend class ExtensionUpdateClientBaseTest;

  ~ChromeUpdateClientConfig() override;

  // Injects a new client config by changing the creation factory.
  // Should be used for tests only.
  static void SetChromeUpdateClientConfigFactoryForTesting(
      FactoryCallback factory);

 private:
  raw_ptr<content::BrowserContext, LeakedDanglingUntriaged> context_ = nullptr;
  component_updater::ConfiguratorImpl impl_;
  raw_ptr<PrefService, LeakedDanglingUntriaged> pref_service_;
  std::unique_ptr<update_client::PersistedData> persisted_data_;
  scoped_refptr<update_client::NetworkFetcherFactory> network_fetcher_factory_;
  scoped_refptr<update_client::CrxDownloaderFactory> crx_downloader_factory_;
  scoped_refptr<update_client::UnzipperFactory> unzip_factory_;
  scoped_refptr<update_client::PatcherFactory> patch_factory_;
  std::optional<GURL> url_override_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_UPDATER_CHROME_UPDATE_CLIENT_CONFIG_H_
