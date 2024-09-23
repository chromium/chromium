// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/component_updater/aw_component_updater_configurator.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "android_webview/nonembedded/net/network_impl.h"
#include "base/android/path_utils.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/version.h"
#include "components/component_updater/component_updater_command_line_config_policy.h"
#include "components/component_updater/configurator_impl.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/crx_downloader_factory.h"
#include "components/update_client/network.h"
#include "components/update_client/patch/in_process_patcher.h"
#include "components/update_client/patcher.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/protocol_handler.h"
#include "components/update_client/unzip/in_process_unzipper.h"
#include "components/update_client/unzipper.h"
#include "components/update_client/update_query_params.h"
#include "components/version_info/android/channel_getter.h"
#include "components/version_info/version_info.h"
#include "components/version_info/version_info_values.h"

namespace android_webview {

AwComponentUpdaterConfigurator::AwComponentUpdaterConfigurator(
    const base::CommandLine* cmdline,
    PrefService* pref_service)
    : configurator_impl_(
          component_updater::ComponentUpdaterCommandLineConfigPolicy(cmdline),
          false),
      pref_service_(pref_service),
      persisted_data_(
          update_client::CreatePersistedData(pref_service, nullptr)) {}

AwComponentUpdaterConfigurator::~AwComponentUpdaterConfigurator() = default;

base::TimeDelta AwComponentUpdaterConfigurator::InitialDelay() const {
  // Initial delay acts as a "registration window" for components, so we should
  // have a reasonable window to allow for all components to complete
  // registration. We are choosing a small window of 10 seconds here because
  // WebView has a short list of components and components registration happens
  // in an android background service so we want to start the update as soon as
  // possible.
  // TODO(crbug.com/40750670): get rid of dependency in initial delay for
  // WebView.
  return base::Seconds(10);
}

base::TimeDelta AwComponentUpdaterConfigurator::NextCheckDelay() const {
  return configurator_impl_.NextCheckDelay();
}

base::TimeDelta AwComponentUpdaterConfigurator::OnDemandDelay() const {
  return configurator_impl_.OnDemandDelay();
}

base::TimeDelta AwComponentUpdaterConfigurator::UpdateDelay() const {
  // No need to have any delays between components updates. In WebView this
  // doesn't run in a browser and shouldn't affect user's experience.
  // Furthermore, this will be a background service that is scheduled by
  // JobScheduler, so we want to do as much work in as little time as possible.
  // However, if we ever invoked installation on-demand, we should be less
  // aggressive here.
  return base::Seconds(0);
}

std::vector<GURL> AwComponentUpdaterConfigurator::UpdateUrl() const {
  return configurator_impl_.UpdateUrl();
}

std::vector<GURL> AwComponentUpdaterConfigurator::PingUrl() const {
  return configurator_impl_.PingUrl();
}

std::string AwComponentUpdaterConfigurator::GetProdId() const {
  return update_client::UpdateQueryParams::GetProdIdString(
      update_client::UpdateQueryParams::ProdId::WEBVIEW);
}

base::Version AwComponentUpdaterConfigurator::GetBrowserVersion() const {
  return configurator_impl_.GetBrowserVersion();
}

std::string AwComponentUpdaterConfigurator::GetChannel() const {
  return std::string(
      version_info::GetChannelString(version_info::android::GetChannel()));
}

std::string AwComponentUpdaterConfigurator::GetLang() const {
  // WebView uses the locale of the embedding app. Components are shared with
  // WebView instances across apps so we don't set a locale.
  return std::string();
}

std::string AwComponentUpdaterConfigurator::GetOSLongName() const {
  return configurator_impl_.GetOSLongName();
}

base::flat_map<std::string, std::string>
AwComponentUpdaterConfigurator::ExtraRequestParams() const {
  return configurator_impl_.ExtraRequestParams();
}

std::string AwComponentUpdaterConfigurator::GetDownloadPreference() const {
  // Hints for the server about caching URLs, "" means let the server decide the
  // best URLs to return according to its policies.
  return configurator_impl_.GetDownloadPreference();
}

scoped_refptr<update_client::NetworkFetcherFactory>
AwComponentUpdaterConfigurator::GetNetworkFetcherFactory() {
  if (!network_fetcher_factory_) {
    network_fetcher_factory_ =
        base::MakeRefCounted<NetworkFetcherFactoryImpl>();
  }
  return network_fetcher_factory_;
}

scoped_refptr<update_client::CrxDownloaderFactory>
AwComponentUpdaterConfigurator::GetCrxDownloaderFactory() {
  if (!crx_downloader_factory_) {
    crx_downloader_factory_ =
        update_client::MakeCrxDownloaderFactory(GetNetworkFetcherFactory());
  }
  return crx_downloader_factory_;
}

scoped_refptr<update_client::UnzipperFactory>
AwComponentUpdaterConfigurator::GetUnzipperFactory() {
  if (!unzip_factory_) {
    unzip_factory_ =
        base::MakeRefCounted<update_client::InProcessUnzipperFactory>();
  }
  return unzip_factory_;
}

scoped_refptr<update_client::PatcherFactory>
AwComponentUpdaterConfigurator::GetPatcherFactory() {
  if (!patch_factory_) {
    patch_factory_ =
        base::MakeRefCounted<update_client::InProcessPatcherFactory>();
  }
  return patch_factory_;
}

bool AwComponentUpdaterConfigurator::EnabledDeltas() const {
  return configurator_impl_.EnabledDeltas();
}

bool AwComponentUpdaterConfigurator::EnabledBackgroundDownloader() const {
  return configurator_impl_.EnabledBackgroundDownloader();
}

bool AwComponentUpdaterConfigurator::EnabledCupSigning() const {
  return configurator_impl_.EnabledCupSigning();
}

PrefService* AwComponentUpdaterConfigurator::GetPrefService() const {
  return pref_service_;
}

update_client::PersistedData* AwComponentUpdaterConfigurator::GetPersistedData()
    const {
  return persisted_data_.get();
}

bool AwComponentUpdaterConfigurator::IsPerUserInstall() const {
  return true;
}

std::unique_ptr<update_client::ProtocolHandlerFactory>
AwComponentUpdaterConfigurator::GetProtocolHandlerFactory() const {
  return configurator_impl_.GetProtocolHandlerFactory();
}

std::optional<bool> AwComponentUpdaterConfigurator::IsMachineExternallyManaged()
    const {
  return std::nullopt;
}

update_client::UpdaterStateProvider
AwComponentUpdaterConfigurator::GetUpdaterStateProvider() const {
  return configurator_impl_.GetUpdaterStateProvider();
}

scoped_refptr<update_client::Configurator> MakeAwComponentUpdaterConfigurator(
    const base::CommandLine* cmdline,
    PrefService* pref_service) {
  return base::MakeRefCounted<AwComponentUpdaterConfigurator>(cmdline,
                                                              pref_service);
}

std::optional<base::FilePath> AwComponentUpdaterConfigurator::GetCrxCachePath()
    const {
  base::FilePath path;
  return base::android::GetCacheDirectory(&path)
             ? std::optional<base::FilePath>(
                   path.AppendASCII(("webview_crx_cache")))
             : std::nullopt;
}

bool AwComponentUpdaterConfigurator::IsConnectionMetered() const {
  return configurator_impl_.IsConnectionMetered();
}

}  // namespace android_webview
