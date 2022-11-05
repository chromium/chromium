// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_LOCAL_ACCOUNT_EXTERNAL_CACHE_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_LOCAL_ACCOUNT_EXTERNAL_CACHE_H_

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/browser/ash/extensions/external_cache_delegate.h"
#include "chrome/browser/chromeos/extensions/device_local_account_external_policy_loader.h"
#include "chrome/browser/extensions/external_loader.h"

namespace chromeos {

class ExternalCache;

/**
 * Wrapper class around ExternalCache that also handles the callbacks from
 * ExternalCacheDelegate.
 */
class DeviceLocalAccountExternalCache : public ExternalCacheDelegate {
 public:
  DeviceLocalAccountExternalCache(const std::string& user_id,
                                  const base::FilePath& cache_dir);
  ~DeviceLocalAccountExternalCache() override;

  // Start the cache using the supplied |cache_task_runner|.
  void StartCache(
      const scoped_refptr<base::SequencedTaskRunner>& cache_task_runner);

  // Stop the cache. When the cache is stopped, |callback| will be invoked.
  void StopCache(base::OnceClosure callback);

  // Return whether the cache is currently running.
  bool IsCacheRunning() const;

  // Send the new extension dictionary down to the ExternalCache.
  void UpdateExtensionsList(base::Value::Dict dict);

  // ExternalCacheDelegate:
  void OnExtensionListsUpdated(const base::Value::Dict& prefs) override;

  scoped_refptr<extensions::ExternalLoader> GetExtensionLoader();

  base::Value::Dict GetCachedExtensions() const;

 private:
  const std::string user_id_;
  const base::FilePath cache_dir_;
  std::unique_ptr<ExternalCache> external_cache_;
  scoped_refptr<DeviceLocalAccountExternalPolicyLoader> loader_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_LOCAL_ACCOUNT_EXTERNAL_CACHE_H_
