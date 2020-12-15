// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_API_MANAGED_CONFIGURATION_STORE_H_
#define CHROME_BROWSER_DEVICE_API_MANAGED_CONFIGURATION_STORE_H_

#include "base/files/file_path.h"
#include "base/observer_list_threadsafe.h"
#include "chrome/browser/device_api/managed_configuration_api.h"
#include "extensions/browser/value_store/leveldb_value_store.h"
#include "url/origin.h"

namespace base {
class SequencedTaskRunner;
}

// Class responsible for internal storage of the managed configuration. Adding
// and removing observers is allowed on any thread, while setting/getting the
// data is only allowed on the FILE thread.
//
// By itself, this class can be percieved as a handle to access levelDB database
// stored at |path|.
class ManagedConfigurationStore {
 public:
  ManagedConfigurationStore(
      scoped_refptr<base::SequencedTaskRunner> backend_sequence,
      const url::Origin& origin,
      const base::FilePath& path);
  ~ManagedConfigurationStore();

  // Initializes connection to the database. Must be called on
  // |backend_sequence_|.
  void InitializeOnBackend();

  void AddObserver(ManagedConfigurationAPI::Observer* observer);
  void RemoveObserver(ManagedConfigurationAPI::Observer* observer);

  // Read/Write operations must be called on |backend_sequence_|.
  void SetCurrentPolicy(const base::DictionaryValue& current_configuration);
  ValueStore::ReadResult Get(const std::vector<std::string>& keys);

 private:
  scoped_refptr<base::SequencedTaskRunner> backend_sequence_;
  const url::Origin origin_;
  const base::FilePath path_;
  std::unique_ptr<ValueStore> store_;
  scoped_refptr<base::ObserverListThreadSafe<ManagedConfigurationAPI::Observer>>
      observers_;
};

#endif  // CHROME_BROWSER_DEVICE_API_MANAGED_CONFIGURATION_STORE_H_
