// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/chrome_component_updater_configurator.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/sequence_checker.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/component_updater_utils.h"
#include "chrome/browser/component_updater/updater_state.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/update_client/chrome_update_query_params_delegate.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "components/component_updater/component_updater_command_line_config_policy.h"
#include "components/component_updater/configurator_impl.h"
#include "components/prefs/pref_service.h"
#include "components/services/patch/content/patch_service.h"
#include "components/services/unzip/content/unzip_service.h"
#include "components/update_client/activity_data_service.h"
#include "components/update_client/crx_downloader_factory.h"
#include "components/update_client/net/network_chromium.h"
#include "components/update_client/patch/patch_impl.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/protocol_handler.h"
#include "components/update_client/unzip/unzip_impl.h"
#include "components/update_client/unzipper.h"
#include "components/update_client/update_query_params.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_WIN)
#include "base/enterprise_util.h"
#include "chrome/installer/util/google_update_settings.h"
#endif

namespace component_updater {
namespace {

class ChromeConfigurator : public update_client::Configurator {
 public:
  ChromeConfigurator(const base::CommandLine* cmdline,
                     PrefService* pref_service);

  // update_client::Configurator overrides.
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

 private:
  friend class base::RefCountedThreadSafe<ChromeConfigurator>;

  std::optional<base::FilePath> GetBackgroundDownloaderCache() const;

  SEQUENCE_CHECKER(sequence_checker_);
  ConfiguratorImpl configurator_impl_;
  raw_ptr<PrefService, LeakedDanglingUntriaged> pref_service_;
  std::unique_ptr<update_client::PersistedData> persisted_data_;
  scoped_refptr<update_client::NetworkFetcherFactory> network_fetcher_factory_;
  scoped_refptr<update_client::CrxDownloaderFactory> crx_downloader_factory_;
  scoped_refptr<update_client::UnzipperFactory> unzip_factory_;
  scoped_refptr<update_client::PatcherFactory> patch_factory_;

  ~ChromeConfigurator() override = default;
};

// Allows the component updater to use non-encrypted communication with the
// update backend. The security of the update checks is enforced using
// a custom message signing protocol and it does not depend on using HTTPS.
ChromeConfigurator::ChromeConfigurator(const base::CommandLine* cmdline,
                                       PrefService* pref_service)
    : configurator_impl_(ComponentUpdaterCommandLineConfigPolicy(cmdline),
                         /*require_encryption=*/false),
      pref_service_(pref_service),
      persisted_data_(
          update_client::CreatePersistedData(pref_service, nullptr)) {
  CHECK(pref_service_);
}

base::TimeDelta ChromeConfigurator::InitialDelay() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return configurator_impl_.InitialDelay();
}

base::TimeDelta ChromeConfigurator::NextCheckDelay() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return configurator_impl_.NextCheckDelay();
}

base::TimeDelta ChromeConfigurator::OnDemandDelay() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return configurator_impl_.OnDemandDelay();
}

base::TimeDelta ChromeConfigurator::UpdateDelay() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return configurator_impl_.UpdateDelay();
}

std::vector<GURL> ChromeConfigurator::UpdateUrl() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return configurator_impl_.UpdateUrl();
}

std::vector<GURL> ChromeConfigurator::PingUrl() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return configurator_impl_.PingUrl();
}

std::string ChromeConfigurator::GetProdId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return update_client::UpdateQueryParams::GetProdIdString(
      update_client::UpdateQueryParams::ProdId::CHROME);
}

base::Version ChromeConfigurator::GetBrowserVersion() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return configurator_impl_.GetBrowserVersion();
}

std::string ChromeConfigurator::GetChannel() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return chrome::GetChannelName(chrome::WithExtendedStable(true));
}

std::string ChromeConfigurator::GetLang() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ChromeUpdateQueryParamsDelegate::GetLang();
}

std::string ChromeConfigurator::GetOSLongName() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return configurator_impl_.GetOSLongName();
}

base::flat_map<std::string, std::string>
ChromeConfigurator::ExtraRequestParams() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return configurator_impl_.ExtraRequestParams();
}

std::string ChromeConfigurator::GetDownloadPreference() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(IS_WIN)
  // This group policy is supported only on Windows and only for enterprises.
  return base::IsEnterpriseDevice()
             ? base::SysWideToUTF8(
                   GoogleUpdateSettings::GetDownloadPreference())
             : std::string();
#else
  return std::string();
#endif  // BUILDFLAG(IS_WIN)
}

scoped_refptr<update_client::NetworkFetcherFactory>
ChromeConfigurator::GetNetworkFetcherFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!network_fetcher_factory_) {
    network_fetcher_factory_ =
        base::MakeRefCounted<update_client::NetworkFetcherChromiumFactory>(
            g_browser_process->system_network_context_manager()
                ->GetSharedURLLoaderFactory(),
            // Never send cookies for component update downloads.
            base::BindRepeating([](const GURL& url) { return false; }));
  }
  return network_fetcher_factory_;
}

scoped_refptr<update_client::CrxDownloaderFactory>
ChromeConfigurator::GetCrxDownloaderFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!crx_downloader_factory_) {
    crx_downloader_factory_ = update_client::MakeCrxDownloaderFactory(
        GetNetworkFetcherFactory(), GetBackgroundDownloaderCache());
  }
  return crx_downloader_factory_;
}

scoped_refptr<update_client::UnzipperFactory>
ChromeConfigurator::GetUnzipperFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!unzip_factory_) {
    unzip_factory_ = base::MakeRefCounted<update_client::UnzipChromiumFactory>(
        base::BindRepeating(&unzip::LaunchUnzipper));
  }
  return unzip_factory_;
}

scoped_refptr<update_client::PatcherFactory>
ChromeConfigurator::GetPatcherFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!patch_factory_) {
    patch_factory_ = base::MakeRefCounted<update_client::PatchChromiumFactory>(
        base::BindRepeating(&patch::LaunchFilePatcher));
  }
  return patch_factory_;
}

bool ChromeConfigurator::EnabledDeltas() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return configurator_impl_.EnabledDeltas();
}

bool ChromeConfigurator::EnabledBackgroundDownloader() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return configurator_impl_.EnabledBackgroundDownloader();
}

bool ChromeConfigurator::EnabledCupSigning() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return configurator_impl_.EnabledCupSigning();
}

PrefService* ChromeConfigurator::GetPrefService() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_;
}

update_client::PersistedData* ChromeConfigurator::GetPersistedData() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return persisted_data_.get();
}

bool ChromeConfigurator::IsPerUserInstall() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return component_updater::IsPerUserInstall();
}

std::unique_ptr<update_client::ProtocolHandlerFactory>
ChromeConfigurator::GetProtocolHandlerFactory() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return configurator_impl_.GetProtocolHandlerFactory();
}

std::optional<bool> ChromeConfigurator::IsMachineExternallyManaged() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return configurator_impl_.IsMachineExternallyManaged();
}

update_client::UpdaterStateProvider
ChromeConfigurator::GetUpdaterStateProvider() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  return base::BindRepeating(&UpdaterState::GetState);
#else
  return configurator_impl_.GetUpdaterStateProvider();
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
}

std::optional<base::FilePath> ChromeConfigurator::GetCrxCachePath() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::FilePath path;
  bool result = base::PathService::Get(chrome::DIR_USER_DATA, &path);
  return result ? std::optional<base::FilePath>(
                      path.AppendASCII("component_crx_cache"))
                : std::nullopt;
}

std::optional<base::FilePath> ChromeConfigurator::GetBackgroundDownloaderCache()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::FilePath path;
  bool result = base::PathService::Get(chrome::DIR_USER_DATA, &path);
  return result
             ? std::optional<base::FilePath>(path.AppendASCII("download_cache"))
             : std::nullopt;
}

bool ChromeConfigurator::IsConnectionMetered() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return configurator_impl_.IsConnectionMetered();
}

}  // namespace

scoped_refptr<update_client::Configurator>
MakeChromeComponentUpdaterConfigurator(const base::CommandLine* cmdline,
                                       PrefService* pref_service) {
  return base::MakeRefCounted<ChromeConfigurator>(cmdline, pref_service);
}

}  // namespace component_updater
