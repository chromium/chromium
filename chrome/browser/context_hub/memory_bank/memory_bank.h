// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_MEMORY_BANK_H_
#define CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_MEMORY_BANK_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank_entry.h"

namespace context_hub {

class MemoryBank {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when `entry` is saved. If an entry with the same ID already
    // exists, it has been replaced by `entry`.
    virtual void OnMemoryBankEntryAdded(const MemoryBankEntry& entry) {}
    // Called when the annotations of the entry with `id` are updated. The
    // given annotations replace the previous ones.
    virtual void OnMemoryBankEntryUpdated(
        int64_t id,
        const std::vector<std::string>& tags,
        const std::optional<std::string>& note,
        const std::optional<std::string>& collection) {}
    // Called when entries are deleted. `ids` may contain IDs that did not
    // match an existing entry, which observers should ignore.
    virtual void OnMemoryBankEntriesDeleted(const std::vector<int64_t>& ids) {}
  };

  virtual ~MemoryBank() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  using OperationCompleteCallback = base::OnceCallback<void(bool)>;
  // Saves or updates an entry in the memory bank.
  virtual void SaveMemoryBankEntry(MemoryBankEntry entry,
                                   OperationCompleteCallback callback) = 0;
  // Updates the annotations (tags, note, collection) for an existing entry.
  virtual void UpdateEntryAnnotations(int64_t id,
                                      std::vector<std::string> tags,
                                      std::optional<std::string> note,
                                      std::optional<std::string> collection,
                                      OperationCompleteCallback callback) = 0;
  // Deletes entries from the memory bank.
  virtual void DeleteEntries(base::span<const int64_t> ids,
                             OperationCompleteCallback callback) = 0;
  using GetEntriesCallback =
      base::OnceCallback<void(std::vector<MemoryBankEntry>)>;
  // Returns all entries from the memory bank via the callback.
  virtual void GetAllEntries(GetEntriesCallback callback) const = 0;
  // Returns entries for the given IDs from the memory bank via the callback.
  virtual void GetEntriesByIds(base::span<const int64_t> ids,
                               GetEntriesCallback callback) const = 0;
  using GetStringsCallback =
      base::OnceCallback<void(const std::vector<std::string>&)>;
  // Returns all unique tags from the memory bank via the callback.
  virtual void GetAllTags(GetStringsCallback callback) const = 0;
  // Returns all unique collections from the memory bank via the callback.
  virtual void GetAllCollections(GetStringsCallback callback) const = 0;
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_MEMORY_BANK_H_
