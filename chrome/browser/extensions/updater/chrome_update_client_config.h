// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_UPDATER_CHROME_UPDATE_CLIENT_CONFIG_H_
#define CHROME_BROWSER_EXTENSIONS_UPDATER_CHROME_UPDATE_CLIENT_CONFIG_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/component_updater/configurator_impl.h"
#include "components/update_client/configurator.h"

namespace content {
class BrowserContext;
}

namespace update_client {
class ActivityDataService;
class NetworkFetcherFactory;
class ProtocolHandlerFactory;
}

namespace extensions {

class ExtensionUpdateClientBaseTest;

class ChromeUpdateClientConfig : public update_client::Configurator {
 public:
  using FactoryCallback =
      base::RepeatingCallback<scoped_refptr<ChromeUpdateClientConfig>(
          content::BrowserContext* context)>;

  static scoped_refptr<ChromeUpdateClientConfig> Create(
      content::BrowserContext* context);

  int InitialDelay() const override;
  int NextCheckDelay() const override;
  int OnDemandDelay() const override;
  int UpdateDelay() const override;
  std::vector<GURL> UpdateUrl() const override;
  std::vector<GURL> PingUrl() const override;
  std::string GetProdId() const override;
  base::Version GetBrowserVersion() const override;
  std::string GetChannel() const override;
  std::string GetBrand() const override;
  std::string GetLang() const override;
  std::string GetOSLongName() const override;
  base::flat_map<std::string, std::string> ExtraRequestParams() const override;
  std::string GetDownloadPreference() const override;
  scoped_refptr<update_client::NetworkFetcherFactory> GetNetworkFetcherFactory()
      override;
  scoped_refptr<update_client::UnzipperFactory> GetUnzipperFactory() override;
  scoped_refptr<update_client::PatcherFactory> GetPatcherFactory() override;
  bool EnabledDeltas() const override;
  bool EnabledComponentUpdates() const override;
  bool EnabledBackgroundDownloader() const override;
  bool EnabledCupSigning() const override;
  PrefService* GetPrefService() const override;
  update_client::ActivityDataService* GetActivityDataService() const override;
  bool IsPerUserInstall() const override;
  std::vector<uint8_t> GetRunActionKeyHash() const override;
  std::string GetAppGuid() const override;
  std::unique_ptr<update_client::ProtocolHandlerFactory>
  GetProtocolHandlerFactory() const override;
  update_client::RecoveryCRXElevator GetRecoveryCRXElevator() const override;

 protected:
  friend class base::RefCountedThreadSafe<ChromeUpdateClientConfig>;
  friend class ExtensionUpdateClientBaseTest;

  explicit ChromeUpdateClientConfig(content::BrowserContext* context);
  ~ChromeUpdateClientConfig() override;

  // Injects a new client config by changing the creation factory.
  // Should be used for tests only.
  static void SetChromeUpdateClientConfigFactoryForTesting(
      FactoryCallback factory);

 private:
  content::BrowserContext* context_ = nullptr;
  component_updater::ConfiguratorImpl impl_;
  PrefService* pref_service_;
  std::unique_ptr<update_client::ActivityDataService> activity_data_service_;
  scoped_refptr<update_client::NetworkFetcherFactory> network_fetcher_factory_;
  scoped_refptr<update_client::UnzipperFactory> unzip_factory_;
  scoped_refptr<update_client::PatcherFactory> patch_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeUpdateClientConfig);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_UPDATER_CHROME_UPDATE_CLIENT_CONFIG_H_
