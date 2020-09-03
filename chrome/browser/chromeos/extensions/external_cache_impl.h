// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTERNAL_CACHE_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTERNAL_CACHE_IMPL_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/chromeos/extensions/external_cache.h"
#include "chrome/browser/extensions/updater/local_extension_cache.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"
#include "extensions/common/extension_id.h"

namespace base {
class DictionaryValue;
}

namespace extensions {
class ExtensionDownloader;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace chromeos {

class ExternalCacheDelegate;

// The ExternalCacheImpl manages a cache for external extensions.
class ExternalCacheImpl : public ExternalCache,
                          public content::NotificationObserver,
                          public extensions::ExtensionDownloaderDelegate {
 public:
  // The |url_loader_factory| is used for update checks. All file I/O is done
  // via the |backend_task_runner|. If |always_check_updates| is |false|, update
  // checks are performed for extensions that have an |external_update_url|
  // only. If |wait_for_cache_initialization| is |true|, the cache contents will
  // not be read until a flag file appears in the cache directory, signaling
  // that the cache is ready.
  ExternalCacheImpl(
      const base::FilePath& cache_dir,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const scoped_refptr<base::SequencedTaskRunner>& backend_task_runner,
      ExternalCacheDelegate* delegate,
      bool always_check_updates,
      bool wait_for_cache_initialization);
  ~ExternalCacheImpl() override;

  // Implementation of ExternalCache:
  const base::DictionaryValue* GetCachedExtensions() override;
  void Shutdown(base::OnceClosure callback) override;
  void UpdateExtensionsList(
      std::unique_ptr<base::DictionaryValue> prefs) override;
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

  // Implementation of content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

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

  void set_flush_on_put(bool flush_on_put) { flush_on_put_ = flush_on_put; }

 private:
  // Notifies the that the cache has been updated, providing
  // extensions loader with an updated list of extensions.
  void UpdateExtensionLoader();

  // Checks the cache contents and initiate download if needed.
  void CheckCache();

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

  extensions::LocalExtensionCache local_cache_;

  // URL lader factory used by the |downloader_|.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Task runner for executing file I/O tasks.
  const scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  // Delegate that would like to get notifications about cache updates.
  ExternalCacheDelegate* delegate_;

  // Updates needs to be check for the extensions with external_crx too.
  bool always_check_updates_;

  // Set to true if cache should wait for initialization flag file.
  bool wait_for_cache_initialization_;

  // Whether to flush the crx file after putting into |local_cache_|.
  bool flush_on_put_ = false;

  // This is the list of extensions currently configured.
  std::unique_ptr<base::DictionaryValue> extensions_;

  // This contains extensions that are both currently configured
  // and that have a valid crx in the cache.
  std::unique_ptr<base::DictionaryValue> cached_extensions_;

  // Used to download the extensions and to check for updates.
  std::unique_ptr<extensions::ExtensionDownloader> downloader_;

  // Observes failures to install CRX files.
  content::NotificationRegistrar notification_registrar_;

  // Weak factory for callbacks.
  base::WeakPtrFactory<ExternalCacheImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ExternalCacheImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_EXTERNAL_CACHE_IMPL_H_
