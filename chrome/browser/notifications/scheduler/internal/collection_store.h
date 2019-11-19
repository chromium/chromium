// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_COLLECTION_STORE_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_COLLECTION_STORE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"

namespace notifications {

// A storage interface which loads a collection of data type T into memory
// during initialization. When updating the data, T will be copied to the actual
// storage layer since the caller will keep in memory data as well.
template <typename T>
class CollectionStore {
 public:
  using Entries = std::vector<std::unique_ptr<T>>;
  using LoadCallback = base::OnceCallback<void(bool, Entries)>;
  using InitCallback = base::OnceCallback<void(bool)>;
  using UpdateCallback = base::OnceCallback<void(bool)>;

  // Initializes the database and loads all entries into memory.
  virtual void InitAndLoad(LoadCallback callback) = 0;

  // Adds an entry to the storage.
  virtual void Add(const std::string& key,
                   const T& entry,
                   UpdateCallback callback) = 0;

  // Updates an entry.
  virtual void Update(const std::string& key,
                      const T& entry,
                      UpdateCallback callback) = 0;

  // Deletes an entry from storage.
  virtual void Delete(const std::string& key, UpdateCallback callback) = 0;

  CollectionStore() = default;
  virtual ~CollectionStore() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(CollectionStore);
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_COLLECTION_STORE_H_
