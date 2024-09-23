// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_STORAGE_POLICY_VALUE_STORE_H_
#define CHROME_BROWSER_EXTENSIONS_API_STORAGE_POLICY_VALUE_STORE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "components/value_store/value_store.h"
#include "extensions/browser/api/storage/settings_observer.h"
#include "extensions/common/extension_id.h"

namespace policy {
class PolicyMap;
}

namespace extensions {

// A ValueStore that is backed by another, persistent ValueStore, and stores
// the policies for a specific extension there. This ValueStore is used to
// run the function of the storage.managed namespace; it's read-only for the
// extension. The ManagedValueStoreCache sends updated policy to this store
// and manages its lifetime.
class PolicyValueStore : public value_store::ValueStore {
 public:
  PolicyValueStore(const ExtensionId& extension_id,
                   SequenceBoundSettingsChangedCallback observer,
                   std::unique_ptr<value_store::ValueStore> delegate);

  PolicyValueStore(const PolicyValueStore&) = delete;
  PolicyValueStore& operator=(const PolicyValueStore&) = delete;

  ~PolicyValueStore() override;

  // Stores |policy| in the persistent database represented by the |delegate_|
  // and notifies observers with the changes from the previous policy.
  void SetCurrentPolicy(const policy::PolicyMap& policy);

  // Clears all the stored data and deletes the database.
  void DeleteStorage();

  // ValueStore implementation:
  size_t GetBytesInUse(const std::string& key) override;
  size_t GetBytesInUse(const std::vector<std::string>& keys) override;
  size_t GetBytesInUse() override;
  ReadResult Get(const std::string& key) override;
  ReadResult Get(const std::vector<std::string>& keys) override;
  ReadResult Get() override;
  WriteResult Set(WriteOptions options,
                  const std::string& key,
                  const base::Value& value) override;
  WriteResult Set(WriteOptions options,
                  const base::Value::Dict& values) override;
  WriteResult Remove(const std::string& key) override;
  WriteResult Remove(const std::vector<std::string>& keys) override;
  WriteResult Clear() override;

  // For unit tests.
  value_store::ValueStore* delegate() { return delegate_.get(); }

 private:
  ExtensionId extension_id_;
  SequenceBoundSettingsChangedCallback observer_;
  std::unique_ptr<value_store::ValueStore> delegate_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_STORAGE_POLICY_VALUE_STORE_H_
