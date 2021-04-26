// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_DEVICE_LOCAL_ACCOUNT_EXTERNAL_POLICY_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_DEVICE_LOCAL_ACCOUNT_EXTERNAL_POLICY_LOADER_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/chromeos/extensions/external_cache_delegate.h"
#include "chrome/browser/extensions/external_loader.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"

namespace chromeos {

class ExternalCache;

// A specialization of the ExternalLoader that serves external extensions from
// the enterprise policy force-install list. This class is used for device-local
// accounts in place of the ExternalPolicyLoader. The difference is that while
// the ExternalPolicyLoader requires extensions to be downloaded on-the-fly,
// this class caches them, allowing for offline installation.
class DeviceLocalAccountExternalPolicyLoader
    : public extensions::ExternalLoader,
      public policy::CloudPolicyStore::Observer,
      public ExternalCacheDelegate {
 public:
  // The list of force-installed extensions will be read from |store| and the
  // extensions will be cached in the |cache_dir_|.
  DeviceLocalAccountExternalPolicyLoader(policy::CloudPolicyStore* store,
                                         const base::FilePath& cache_dir);

  // While running, the cache requires exclusive write access to the
  // |cache_dir_|.
  bool IsCacheRunning() const;

  // Start the cache. This method must only be invoked when there are no pending
  // write operations to |cache_dir_| on any thread and none will be initiated
  // while the cache is running.
  void StartCache(
      const scoped_refptr<base::SequencedTaskRunner>& cache_task_runner);

  // Stop the cache. The |callback| will be invoked when the cache has shut down
  // completely and write access to the |cache_dir_| is permitted again.
  void StopCache(base::OnceClosure callback);

  // extensions::ExternalLoader:
  void StartLoading() override;

  // policy::CloudPolicyStore::Observer:
  void OnStoreLoaded(policy::CloudPolicyStore* store) override;
  void OnStoreError(policy::CloudPolicyStore* store) override;

  // ExternalCacheDelegate:
  void OnExtensionListsUpdated(const base::DictionaryValue* prefs) override;

  ExternalCache* GetExternalCacheForTesting();

 private:
  // If the cache was started, it must be stopped before |this| is destroyed.
  ~DeviceLocalAccountExternalPolicyLoader() override;

  // Pass the current list of force-installed extensions from the |store_| to
  // the |external_cache_|.
  void UpdateExtensionListFromStore();

  policy::CloudPolicyStore* store_;
  const base::FilePath cache_dir_;
  std::unique_ptr<ExternalCache> external_cache_;
  std::unique_ptr<base::DictionaryValue> prefs_;

  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountExternalPolicyLoader);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_DEVICE_LOCAL_ACCOUNT_EXTERNAL_POLICY_LOADER_H_
