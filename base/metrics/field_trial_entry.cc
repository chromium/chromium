// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/field_trial_entry.h"

#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"

namespace base::internal {

// static
std::vector<const FieldTrialEntry*>
FieldTrialEntry::GetAllFieldTrialsFromPersistentAllocator(
    const PersistentMemoryAllocator& allocator) {
  std::vector<const FieldTrialEntry*> entries;
  PersistentMemoryAllocator::Iterator iter(&allocator);
  const FieldTrialEntry* entry;
  while ((entry = iter.GetNextOfObject<FieldTrialEntry>()) != nullptr) {
    entries.push_back(entry);
  }
  return entries;
}

bool FieldTrialEntry::GetState(std::string_view& trial_name,
                               std::string_view& group_name,
                               bool& overridden) const {
  PickleIterator iter = GetPickleIterator();
  return ReadHeader(iter, trial_name, group_name, overridden);
}

bool FieldTrialEntry::GetParams(
    std::map<std::string, std::string>* params) const {
  PickleIterator iter = GetPickleIterator();
  std::string_view tmp_string;
  bool tmp_bool;
  // Skip reading trial and group name, and overridden bit.
  if (!ReadHeader(iter, tmp_string, tmp_string, tmp_bool)) {
    return false;
  }

  while (true) {
    std::string_view key;
    std::string_view value;
    if (!ReadStringPair(&iter, &key, &value)) {
      return key.empty();  // Non-empty is bad: got one of a pair.
    }
    (*params)[std::string(key)] = std::string(value);
  }
}

PickleIterator FieldTrialEntry::GetPickleIterator() const {
  Pickle pickle = Pickle::WithUnownedBuffer(
      // TODO(crbug.com/40284755): FieldTrialEntry should be constructed with a
      // span over the pickle memory.
      UNSAFE_TODO(
          span(GetPickledDataPtr(), checked_cast<size_t>(pickle_size))));
  return PickleIterator(pickle);
}

bool FieldTrialEntry::ReadHeader(PickleIterator& iter,
                                 std::string_view& trial_name,
                                 std::string_view& group_name,
                                 bool& overridden) const {
  return ReadStringPair(&iter, &trial_name, &group_name) &&
         iter.ReadBool(&overridden);
}

bool FieldTrialEntry::ReadStringPair(PickleIterator* iter,
                                     std::string_view* trial_name,
                                     std::string_view* group_name) const {
  if (!iter->ReadStringPiece(trial_name)) {
    return false;
  }
  if (!iter->ReadStringPiece(group_name)) {
    return false;
  }
  return true;
}

}  // namespace base::internal
