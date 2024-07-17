// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/chrome_update_client_config.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/browser/component_updater/component_updater_utils.h"
#include "chrome/browser/extensions/updater/extension_update_client_command_line_config_policy.h"
#include "chrome/browser/extensions/updater/extension_updater_switches.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/update_client/chrome_update_query_params_delegate.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "components/prefs/pref_service.h"
#include "components/services/patch/content/patch_service.h"
#include "components/services/unzip/content/unzip_service.h"
#include "components/update_client/activity_data_service.h"
#include "components/update_client/crx_downloader_factory.h"
#include "components/update_client/net/network_chromium.h"
#include "components/update_client/patch/patch_impl.h"
#include "components/update_client/patcher.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/protocol_handler.h"
#include "components/update_client/unzip/unzip_impl.h"
#include "components/update_client/unzipper.h"
#include "components/update_client/update_query_params.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_observer.h"

namespace extensions {

namespace {

using FactoryCallback = ChromeUpdateClientConfig::FactoryCallback;

// static
static FactoryCallback& GetFactoryCallback() {
  static base::NoDestructor<FactoryCallback> factory;
  return *factory;
}

class ExtensionActivityDataService final
    : public update_client::ActivityDataService,
      public ExtensionPrefsObserver {
 public:
  explicit ExtensionActivityDataService(ExtensionPrefs* extension_prefs);

  ExtensionActivityDataService(const ExtensionActivityDataService&) = delete;
  ExtensionActivityDataService& operator=(const ExtensionActivityDataService&) =
      delete;

  ~ExtensionActivityDataService() override = default;

  // update_client::ActivityDataService:
  void GetActiveBits(const std::vector<std::string>& ids,
                     base::OnceCallback<void(const std::set<std::string>&)>
                         callback) const override;
  void GetAndClearActiveBits(
      const std::vector<std::string>& ids,
      base::OnceCallback<void(const std::set<std::string>&)> callback) override;
  int GetDaysSinceLastActive(const std::string& id) const override;
  int GetDaysSinceLastRollCall(const std::string& id) const override;

  // ExtensionPrefsObserver:
  void OnExtensionPrefsWillBeDestroyed(ExtensionPrefs* prefs) override;

 private:
  // This member is not owned by this class, it's owned by a profile keyed
  // service.
  raw_ptr<ExtensionPrefs> extension_prefs_;

  base::ScopedObservation<ExtensionPrefs, ExtensionPrefsObserver>
      prefs_observation_{this};
};

// Calculates the value to use for the ping days parameter.
int CalculatePingDays(const base::Time& last_ping_day) {
  return last_ping_day.is_null()
             ? update_client::kDaysFirstTime
             : std::max((base::Time::Now() - last_ping_day).InDays(), 0);
}

ExtensionActivityDataService::ExtensionActivityDataService(
    ExtensionPrefs* extension_prefs)
    : extension_prefs_(extension_prefs) {
  DCHECK(extension_prefs_);

  prefs_observation_.Observe(extension_prefs);
}

void ExtensionActivityDataService::GetActiveBits(
    const std::vector<std::string>& ids,
    base::OnceCallback<void(const std::set<std::string>&)> callback) const {
  std::set<std::string> actives;
  if (extension_prefs_) {
    for (const auto& id : ids) {
      if (extension_prefs_->GetActiveBit(id)) {
        actives.insert(id);
      }
    }
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), actives));
}

int ExtensionActivityDataService::GetDaysSinceLastActive(
    const std::string& id) const {
  if (!extension_prefs_) {
    return update_client::kDaysUnknown;
  }
  return CalculatePingDays(extension_prefs_->LastActivePingDay(id));
}

int ExtensionActivityDataService::GetDaysSinceLastRollCall(
    const std::string& id) const {
  if (!extension_prefs_) {
    return update_client::kDaysUnknown;
  }
  return CalculatePingDays(extension_prefs_->LastPingDay(id));
}

void ExtensionActivityDataService::GetAndClearActiveBits(
    const std::vector<std::string>& ids,
    base::OnceCallback<void(const std::set<std::string>&)> callback) {
  std::set<std::string> actives;
  if (extension_prefs_) {
    for (const auto& id : ids) {
      if (extension_prefs_->GetActiveBit(id)) {
        actives.insert(id);
      }
      extension_prefs_->SetActiveBit(id, false);
    }
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), actives));
}

void ExtensionActivityDataService::OnExtensionPrefsWillBeDestroyed(
    ExtensionPrefs* prefs) {
  DCHECK(prefs_observation_.IsObservingSource(prefs));
  prefs_observation_.Reset();
  extension_prefs_ = nullptr;
}

}  // namespace

// For privacy reasons, requires encryption of the component updater
// communication with the update backend.
ChromeUpdateClientConfig::ChromeUpdateClientConfig(
    content::BrowserContext* context,
    std::optional<GURL> url_override)
    : context_(context),
      impl_(ExtensionUpdateClientCommandLineConfigPolicy(
                base::CommandLine::ForCurrentProcess()),
            /*require_encryption=*/true),
      pref_service_(ExtensionPrefs::Get(context)->pref_service()),
      persisted_data_(update_client::CreatePersistedData(
          pref_service_,
          std::make_unique<ExtensionActivityDataService>(
              ExtensionPrefs::Get(context)))),
      url_override_(url_override) {
  DCHECK(pref_service_);
}

ChromeUpdateClientConfig::~ChromeUpdateClientConfig() = default;

base::TimeDelta ChromeUpdateClientConfig::InitialDelay() const {
  return impl_.InitialDelay();
}

base::TimeDelta ChromeUpdateClientConfig::NextCheckDelay() const {
  return impl_.NextCheckDelay();
}

base::TimeDelta ChromeUpdateClientConfig::OnDemandDelay() const {
  return impl_.OnDemandDelay();
}

base::TimeDelta ChromeUpdateClientConfig::UpdateDelay() const {
  return impl_.UpdateDelay();
}

std::vector<GURL> ChromeUpdateClientConfig::UpdateUrl() const {
  if (url_override_.has_value()) {
    return {*url_override_};
  }
  return impl_.UpdateUrl();
}

std::vector<GURL> ChromeUpdateClientConfig::PingUrl() const {
  if (url_override_.has_value()) {
    return {*url_override_};
  }
  return impl_.PingUrl();
}

std::string ChromeUpdateClientConfig::GetProdId() const {
  return update_client::UpdateQueryParams::GetProdIdString(
      update_client::UpdateQueryParams::ProdId::CRX);
}

base::Version ChromeUpdateClientConfig::GetBrowserVersion() const {
  return impl_.GetBrowserVersion();
}

std::string ChromeUpdateClientConfig::GetChannel() const {
  return GetChannelForExtensionUpdates();
}

std::string ChromeUpdateClientConfig::GetLang() const {
  return ChromeUpdateQueryParamsDelegate::GetLang();
}

std::string ChromeUpdateClientConfig::GetOSLongName() const {
  return impl_.GetOSLongName();
}

base::flat_map<std::string, std::string>
ChromeUpdateClientConfig::ExtraRequestParams() const {
  return impl_.ExtraRequestParams();
}

std::string ChromeUpdateClientConfig::GetDownloadPreference() const {
  return std::string();
}

scoped_refptr<update_client::NetworkFetcherFactory>
ChromeUpdateClientConfig::GetNetworkFetcherFactory() {
  if (!network_fetcher_factory_) {
    network_fetcher_factory_ =
        base::MakeRefCounted<update_client::NetworkFetcherChromiumFactory>(
            context_->GetDefaultStoragePartition()
                ->GetURLLoaderFactoryForBrowserProcess(),
            // Only extension updates that require authentication are served
            // from chrome.google.com, so send cookies if and only if that is
            // the download domain.
            base::BindRepeating([](const GURL& url) {
              return url.DomainIs("chrome.google.com");
            }));
  }
  return network_fetcher_factory_;
}

scoped_refptr<update_client::CrxDownloaderFactory>
ChromeUpdateClientConfig::GetCrxDownloaderFactory() {
  if (!crx_downloader_factory_) {
    crx_downloader_factory_ =
        update_client::MakeCrxDownloaderFactory(GetNetworkFetcherFactory());
  }
  return crx_downloader_factory_;
}

scoped_refptr<update_client::UnzipperFactory>
ChromeUpdateClientConfig::GetUnzipperFactory() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!unzip_factory_) {
    unzip_factory_ = base::MakeRefCounted<update_client::UnzipChromiumFactory>(
        base::BindRepeating(&unzip::LaunchUnzipper));
  }
  return unzip_factory_;
}

scoped_refptr<update_client::PatcherFactory>
ChromeUpdateClientConfig::GetPatcherFactory() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!patch_factory_) {
    patch_factory_ = base::MakeRefCounted<update_client::PatchChromiumFactory>(
        base::BindRepeating(&patch::LaunchFilePatcher));
  }
  return patch_factory_;
}

bool ChromeUpdateClientConfig::EnabledDeltas() const {
  return impl_.EnabledDeltas();
}

bool ChromeUpdateClientConfig::EnabledBackgroundDownloader() const {
  // Historically, Chrome hasn't used background downloaders like BITS for
  // extension updates. In theory, they should work in most cases. When they
  // don't (for example because they don't pass the credentials necessary to
  // download private extensions), the system should fall back to the foreground
  // downloader. So, returning `true` here is probably safe, but should likely
  // be done as a Finch experiment that is carefully monitored.
  return false;
}

bool ChromeUpdateClientConfig::EnabledCupSigning() const {
  if (url_override_.has_value()) {
    return false;
  }
  return impl_.EnabledCupSigning();
}

PrefService* ChromeUpdateClientConfig::GetPrefService() const {
  return pref_service_;
}

update_client::PersistedData* ChromeUpdateClientConfig::GetPersistedData()
    const {
  return persisted_data_.get();
}

bool ChromeUpdateClientConfig::IsPerUserInstall() const {
  return component_updater::IsPerUserInstall();
}

std::unique_ptr<update_client::ProtocolHandlerFactory>
ChromeUpdateClientConfig::GetProtocolHandlerFactory() const {
  return impl_.GetProtocolHandlerFactory();
}

std::optional<bool> ChromeUpdateClientConfig::IsMachineExternallyManaged()
    const {
  return impl_.IsMachineExternallyManaged();
}

update_client::UpdaterStateProvider
ChromeUpdateClientConfig::GetUpdaterStateProvider() const {
  return impl_.GetUpdaterStateProvider();
}

// static
scoped_refptr<ChromeUpdateClientConfig> ChromeUpdateClientConfig::Create(
    content::BrowserContext* context,
    std::optional<GURL> update_url_override) {
  FactoryCallback& factory = GetFactoryCallback();
  return factory.is_null() ? base::MakeRefCounted<ChromeUpdateClientConfig>(
                                 context, update_url_override)
                           : factory.Run(context);
}

// static
void ChromeUpdateClientConfig::SetChromeUpdateClientConfigFactoryForTesting(
    FactoryCallback factory) {
  DCHECK(!factory.is_null());
  GetFactoryCallback() = factory;
}

std::optional<base::FilePath> ChromeUpdateClientConfig::GetCrxCachePath()
    const {
  base::FilePath path;
  bool result = base::PathService::Get(chrome::DIR_USER_DATA, &path);
  return result ? std::optional<base::FilePath>(
                      path.AppendASCII("extensions_crx_cache"))
                : std::nullopt;
}

bool ChromeUpdateClientConfig::IsConnectionMetered() const {
  return impl_.IsConnectionMetered();
}

}  // namespace extensions
