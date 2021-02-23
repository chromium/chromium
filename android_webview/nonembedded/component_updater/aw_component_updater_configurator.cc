// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/component_updater/aw_component_updater_configurator.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/version.h"
#include "components/component_updater/component_updater_command_line_config_policy.h"
#include "components/component_updater/configurator_impl.h"
#include "components/prefs/pref_service.h"
#include "components/services/patch/content/patch_service.h"
#include "components/services/unzip/content/unzip_service.h"
#include "components/update_client/activity_data_service.h"
#include "components/update_client/crx_downloader_factory.h"
#include "components/update_client/network.h"
#include "components/update_client/patch/patch_impl.h"
#include "components/update_client/protocol_handler.h"
#include "components/update_client/unzip/unzip_impl.h"
#include "components/update_client/unzipper.h"
#include "components/update_client/update_query_params.h"
#include "components/version_info/android/channel_getter.h"
#include "components/version_info/version_info.h"
#include "components/version_info/version_info_values.h"

namespace android_webview {

namespace {

class AwConfigurator : public update_client::Configurator {
 public:
  AwConfigurator(const base::CommandLine* cmdline, PrefService* pref_service);

  // update_client::Configurator overrides.
  double InitialDelay() const override;
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
  scoped_refptr<update_client::CrxDownloaderFactory> GetCrxDownloaderFactory()
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
  std::unique_ptr<update_client::ProtocolHandlerFactory>
  GetProtocolHandlerFactory() const override;

 private:
  friend class base::RefCountedThreadSafe<AwConfigurator>;

  component_updater::ConfiguratorImpl configurator_impl_;
  PrefService* pref_service_;  // This member is not owned by this class.
  scoped_refptr<update_client::NetworkFetcherFactory> network_fetcher_factory_;
  scoped_refptr<update_client::CrxDownloaderFactory> crx_downloader_factory_;
  scoped_refptr<update_client::UnzipperFactory> unzip_factory_;
  scoped_refptr<update_client::PatcherFactory> patch_factory_;

  ~AwConfigurator() override = default;
};

AwConfigurator::AwConfigurator(const base::CommandLine* cmdline,
                               PrefService* pref_service)
    : configurator_impl_(
          component_updater::ComponentUpdaterCommandLineConfigPolicy(cmdline),
          false),
      pref_service_(pref_service) {}

double AwConfigurator::InitialDelay() const {
  // Initial delay acts as a "registration window" for components, so we should
  // have a reasonable window to allow for all components to complete
  // registration. We are choosing a small window of 10 seconds here because
  // WebView has a short list of components and components registration happens
  // in an android background service so we want to start the update as soon as
  // possible.
  // TODO(crbug.com/1181094): git rid of dependency in initial delay for
  // WebView.
  return 10;
}

int AwConfigurator::NextCheckDelay() const {
  return configurator_impl_.NextCheckDelay();
}

int AwConfigurator::OnDemandDelay() const {
  return configurator_impl_.OnDemandDelay();
}

int AwConfigurator::UpdateDelay() const {
  // No need to have any delays between components updates. In WebView this
  // doesn't run in a browser and shouldn't affect user's experience.
  // Furthermore, this will be a background service that is scheduled by
  // JobScheduler, so we want to do as much work in as little time as possible.
  // However, if we ever invoked installation on-demand, we should be less
  // aggressive here.
  return 0;
}

std::vector<GURL> AwConfigurator::UpdateUrl() const {
  return configurator_impl_.UpdateUrl();
}

std::vector<GURL> AwConfigurator::PingUrl() const {
  return configurator_impl_.PingUrl();
}

std::string AwConfigurator::GetProdId() const {
  return update_client::UpdateQueryParams::GetProdIdString(
      update_client::UpdateQueryParams::ProdId::WEBVIEW);
}

base::Version AwConfigurator::GetBrowserVersion() const {
  return configurator_impl_.GetBrowserVersion();
}

std::string AwConfigurator::GetChannel() const {
  return version_info::GetChannelString(version_info::android::GetChannel());
}

std::string AwConfigurator::GetBrand() const {
  // WebView isn't branded.
  return std::string();
}

std::string AwConfigurator::GetLang() const {
  // WebView uses the locale of the embedding app. Components are shared with
  // WebView instances across apps so we don't set a locale.
  return std::string();
}

std::string AwConfigurator::GetOSLongName() const {
  return configurator_impl_.GetOSLongName();
}

base::flat_map<std::string, std::string> AwConfigurator::ExtraRequestParams()
    const {
  return configurator_impl_.ExtraRequestParams();
}

std::string AwConfigurator::GetDownloadPreference() const {
  // Hints for the server about caching URLs, "" means let the server decide the
  // best URLs to return according to its policies.
  return configurator_impl_.GetDownloadPreference();
}

scoped_refptr<update_client::NetworkFetcherFactory>
AwConfigurator::GetNetworkFetcherFactory() {
  if (!network_fetcher_factory_) {
    // TODO(crbug.com/1174140) create network fetcher factory.
  }
  return network_fetcher_factory_;
}

scoped_refptr<update_client::CrxDownloaderFactory>
AwConfigurator::GetCrxDownloaderFactory() {
  if (!crx_downloader_factory_) {
    crx_downloader_factory_ =
        update_client::MakeCrxDownloaderFactory(GetNetworkFetcherFactory());
  }
  return crx_downloader_factory_;
}

scoped_refptr<update_client::UnzipperFactory>
AwConfigurator::GetUnzipperFactory() {
  if (!unzip_factory_) {
    unzip_factory_ = base::MakeRefCounted<update_client::UnzipChromiumFactory>(
        base::BindRepeating(&unzip::LaunchUnzipper));
  }
  return unzip_factory_;
}

scoped_refptr<update_client::PatcherFactory>
AwConfigurator::GetPatcherFactory() {
  if (!patch_factory_) {
    patch_factory_ = base::MakeRefCounted<update_client::PatchChromiumFactory>(
        base::BindRepeating(&patch::LaunchFilePatcher));
  }
  return patch_factory_;
}

bool AwConfigurator::EnabledDeltas() const {
  return configurator_impl_.EnabledDeltas();
}

bool AwConfigurator::EnabledComponentUpdates() const {
  // Always enabled.
  return configurator_impl_.EnabledComponentUpdates();
}

bool AwConfigurator::EnabledBackgroundDownloader() const {
  return configurator_impl_.EnabledBackgroundDownloader();
}

bool AwConfigurator::EnabledCupSigning() const {
  return configurator_impl_.EnabledCupSigning();
}

PrefService* AwConfigurator::GetPrefService() const {
  return pref_service_;
}

update_client::ActivityDataService* AwConfigurator::GetActivityDataService()
    const {
  // This tracks user's activity using the component, doesn't apply to
  // components and safe to be null.
  return nullptr;
}

bool AwConfigurator::IsPerUserInstall() const {
  // Android doesn't have per user updaters.
  NOTREACHED();
  return true;
}

std::unique_ptr<update_client::ProtocolHandlerFactory>
AwConfigurator::GetProtocolHandlerFactory() const {
  return configurator_impl_.GetProtocolHandlerFactory();
}

}  // namespace

scoped_refptr<update_client::Configurator> MakeAwComponentUpdaterConfigurator(
    const base::CommandLine* cmdline,
    PrefService* pref_service) {
  return base::MakeRefCounted<AwConfigurator>(cmdline, pref_service);
}

}  // namespace android_webview
