// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_API_MANAGED_CONFIGURATION_STORE_H_
#define CHROME_BROWSER_DEVICE_API_MANAGED_CONFIGURATION_STORE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/values.h"
#include "components/value_store/value_store.h"
#include "url/origin.h"

// Class responsible for internal storage of the managed configuration.
// Setting/getting the data is a blocking operation and need to be run on a
// thread that supports this.
//
// By itself, this class can be percieved as a handle to access levelDB database
// stored at |path|.
class ManagedConfigurationStore {
 public:
  ManagedConfigurationStore(const url::Origin& origin,
                            const base::FilePath& path);
  ~ManagedConfigurationStore();
  ManagedConfigurationStore(const ManagedConfigurationStore&) = delete;
  ManagedConfigurationStore& operator=(const ManagedConfigurationStore&) =
      delete;

  // Returns |true| if the new policy is different from the previously set
  // policy.
  bool SetCurrentPolicy(const base::Value::Dict& current_configuration);
  std::optional<base::Value::Dict> Get(const std::vector<std::string>& keys);

 private:
  // Initializes connection to the database.
  void Initialize();

  const url::Origin origin_;
  const base::FilePath path_;
  std::unique_ptr<value_store::ValueStore> store_;
};

#endif  // CHROME_BROWSER_DEVICE_API_MANAGED_CONFIGURATION_STORE_H_
