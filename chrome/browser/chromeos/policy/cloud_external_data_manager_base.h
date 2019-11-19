// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_CLOUD_EXTERNAL_DATA_MANAGER_BASE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_CLOUD_EXTERNAL_DATA_MANAGER_BASE_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/policy_details.h"

namespace base {
class SequencedTaskRunner;
}

namespace policy {

class CloudExternalDataStore;

// Downloads, verifies, caches and retrieves external data referenced by
// policies.
// This is a common base class used by specializations for regular users and
// device-local accounts.
class CloudExternalDataManagerBase : public CloudExternalDataManager {
 public:
  // |get_policy_details| is used to determine the maximum size that the
  // data referenced by each policy can have. Download scheduling, verification,
  // caching and retrieval tasks are done via the |backend_task_runner|, which
  // must support file I/O.
  CloudExternalDataManagerBase(
      const GetChromePolicyDetailsCallback& get_policy_details,
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner);
  ~CloudExternalDataManagerBase() override;

  // Allows downloaded external data to be cached in |external_data_store|.
  // Ownership of the store is taken. The store can be destroyed by calling
  // SetExternalDataStore(std::unique_ptr<CloudExternalDataStore>()). Accesses
  // to the
  // store are made via |backend_task_runner_| only.
  void SetExternalDataStore(
      std::unique_ptr<CloudExternalDataStore> external_data_store);

  // CloudExternalDataManager:
  void SetPolicyStore(CloudPolicyStore* policy_store) override;
  void OnPolicyStoreLoaded() override;
  void Connect(scoped_refptr<network::SharedURLLoaderFactory>
                   url_loader_factory) override;
  void Disconnect() override;
  void Fetch(const std::string& policy,
             ExternalDataFetcher::FetchCallback callback) override;

  // Allows policies to reference |max_size| bytes of external data even if no
  // |max_size| was specified in policy_templates.json.
  // TODO(bartfab): This override is only needed because there are no policies
  // that reference external data and have a |max_size| yet. Once the first such
  // policy is added, use that policy in tests and remove the override.
  static void SetMaxExternalDataSizeForTesting(int max_size);

 protected:
  friend class CloudExternalDataManagerBaseTest;

  // Try to download and cache all external data referenced by policies in
  // |policy_store_|.
  void FetchAll();

  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

 private:
  // The |backend_| handles all data download scheduling, verification, caching
  // and retrieval. It is instantiated on the thread |this| runs on but after
  // that, must only be accessed and eventually destroyed via the
  // |backend_task_runner_|.
  class Backend;
  std::unique_ptr<Backend> backend_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(CloudExternalDataManagerBase);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_CLOUD_EXTERNAL_DATA_MANAGER_BASE_H_
