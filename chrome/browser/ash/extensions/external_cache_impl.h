// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_EXTERNAL_CACHE_IMPL_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_EXTERNAL_CACHE_IMPL_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/extensions/external_cache.h"
#include "chrome/browser/extensions/updater/local_extension_cache.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"
#include "extensions/common/extension_id.h"
#include "net/base/backoff_entry.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class CrxInstaller;
class ExtensionDownloader;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace chromeos {

class ExternalCacheDelegate;

// The ExternalCacheImpl manages a cache for external extensions.
class ExternalCacheImpl : public ExternalCache,
                          public extensions::ExtensionDownloaderDelegate {
 public:
  // The |url_loader_factory| is used for update checks. All file I/O is done
  // via the |backend_task_runner|. If |always_check_updates| is |false|, update
  // checks are performed for extensions that have an |external_update_url|
  // only. If |wait_for_cache_initialization| is |true|, the cache contents will
  // not be read until a flag file appears in the cache directory, signaling
  // that the cache is ready. By default ExternalCacheImpl updates the cache at
  // startup and when policy changes (UpdateExtensionsList is called). However,
  // if both |allow_scheduled_updates| and the KioskCRXManifestUpdateURLIgnored
  // are|true|, the ExternalCache will run additional update checks from time
  // to time (about very 5 hours, as per kDefaultUpdateFrequencySeconds).
  // Currently it's only enabled for Chrome App Kiosk, see description of the
  // KioskCRXManifestUpdateURLIgnored policy for details.
  // TODO(https://crbug.com/1262158) Postpone starting new update check when the
  // previous one is not finished yet.
  ExternalCacheImpl(
      const base::FilePath& cache_dir,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const scoped_refptr<base::SequencedTaskRunner>& backend_task_runner,
      ExternalCacheDelegate* delegate,
      bool always_check_updates,
      bool wait_for_cache_initialization,
      bool allow_scheduled_updates);

  ExternalCacheImpl(const ExternalCacheImpl&) = delete;
  ExternalCacheImpl& operator=(const ExternalCacheImpl&) = delete;

  ~ExternalCacheImpl() override;

  // Implementation of ExternalCache:
  const base::Value::Dict& GetCachedExtensions() override;
  void Shutdown(base::OnceClosure callback) override;
  void UpdateExtensionsList(base::Value::Dict prefs) override;
  void OnDamagedFileDetected(const base::FilePath& path) override;
  void RemoveExtensions(
      const std::vector<extensions::ExtensionId>& ids) override;
  bool GetExtension(const extensions::ExtensionId& id,
                    base::FilePath* file_path,
                    std::string* version) override;
  bool ExtensionFetchPending(const extensions::ExtensionId& id) override;
  void PutExternalExtension(const extensions::ExtensionId& id,
                            const base::FilePath& crx_file_path,
                            const std::string& version,
                            PutExternalExtensionCallback callback) override;
  void SetBackoffPolicy(
      std::optional<net::BackoffEntry::Policy> backoff_policy) override;

  // Implementation of ExtensionDownloaderDelegate:
  void OnExtensionDownloadFailed(const extensions::ExtensionId& id,
                                 Error error,
                                 const PingResult& ping_result,
                                 const std::set<int>& request_ids,
                                 const FailureData& data) override;
  void OnExtensionDownloadFinished(const extensions::CRXFileInfo& file,
                                   bool file_ownership_passed,
                                   const GURL& download_url,
                                   const PingResult& ping_result,
                                   const std::set<int>& request_ids,
                                   InstallCallback callback) override;
  bool IsExtensionPending(const extensions::ExtensionId& id) override;
  bool GetExtensionExistingVersion(const extensions::ExtensionId& id,
                                   std::string* version) override;
  RequestRollbackResult RequestRollback(
      const extensions::ExtensionId& id) override;

  void set_flush_on_put(bool flush_on_put) { flush_on_put_ = flush_on_put; }

 private:
  class AnyInstallFailureObserver;

  // Notifies the that the cache has been updated, providing
  // extensions loader with an updated list of extensions.
  void UpdateExtensionLoader();

  // Checks the cache contents and initiate download if needed.
  void CheckCache();

  // Schedule a new cache check in some near future (around 5 hours, according
  // to kDefaultUpdateFrequencySeconds) if a corresponding policy and flag
  // enable this.
  void MaybeScheduleNextCacheCheck();

  // Invoked on the UI thread when a new entry has been installed in the cache.
  void OnPutExtension(const extensions::ExtensionId& id,
                      const base::FilePath& file_path,
                      bool file_ownership_passed);

  // Invoked on the UI thread when the external extension has been installed
  // in the local cache by calling PutExternalExtension.
  void OnPutExternalExtension(const extensions::ExtensionId& id,
                              PutExternalExtensionCallback callback,
                              const base::FilePath& file_path,
                              bool file_ownership_passed);

  // Removes the cached file for |id| from |cached_extensions_| and
  // |local_cache_| and notifies the |delegate_|. This method should be followed
  // by a call to UpdateExtensionLoader().
  void RemoveCachedExtension(const extensions::ExtensionId& id);
  void OnCrxInstallFailure(content::BrowserContext* context,
                           const extensions::CrxInstaller& installer);

  std::unique_ptr<AnyInstallFailureObserver> any_install_failure_observer_;

  extensions::LocalExtensionCache local_cache_;

  // URL lader factory used by the |downloader_|.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Task runner for executing file I/O tasks.
  const scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  // Delegate that would like to get notifications about cache updates.
  raw_ptr<ExternalCacheDelegate> delegate_;

  // Updates needs to be check for the extensions with external_crx too.
  bool always_check_updates_;

  // Set to true if cache should wait for initialization flag file.
  bool wait_for_cache_initialization_;

  // Set to true if scheduled updated are possible, currently in kiosk mode.
  bool allow_scheduled_updates_;

  // Whether to flush the crx file after putting into |local_cache_|.
  bool flush_on_put_ = false;

  // This is the list of extensions currently configured.
  base::Value::Dict extensions_;

  // This contains extensions that are both currently configured
  // and that have a valid crx in the cache.
  base::Value::Dict cached_extensions_;

  // Used to download the extensions and to check for updates.
  std::unique_ptr<extensions::ExtensionDownloader> downloader_;

  // Backoff policy of extension downloader.
  std::optional<net::BackoffEntry::Policy> backoff_policy_;

  // Used to observe CrosSettings.
  base::CallbackListSubscription kiosk_crx_updates_from_policy_subscription_;

  // Weak factody for scheduled updates.
  base::WeakPtrFactory<ExternalCacheImpl> scheduler_weak_ptr_factory_{this};

  // Weak factory for callbacks.
  base::WeakPtrFactory<ExternalCacheImpl> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_EXTERNAL_CACHE_IMPL_H_
