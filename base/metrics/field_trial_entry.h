// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_METRICS_FIELD_TRIAL_ENTRY_H_
#define BASE_METRICS_FIELD_TRIAL_ENTRY_H_

#include <stddef.h>
#include <stdint.h>

#include <atomic>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/pickle.h"

namespace base::internal {

// Used by field_trial.cc to represent field trials in a shared memory segment
// that's shared with child processes. One FieldTrialEntry is created per field
// trial and is followed by a base::Pickle object that gets decoded and read.
struct BASE_EXPORT FieldTrialEntry {
  // SHA1(FieldTrialEntry): Increment this if structure changes!
  static constexpr uint32_t kPersistentTypeId = 0xABA17E13 + 3;

  // Expected size for 32/64-bit check.
  static constexpr size_t kExpectedInstanceSize = 16;

  // Retrieves field trial state from an allocator so that it can be analyzed
  // after a crash. The pointers in the returned vector are into the persistent
  // memory segment and so are only valid as long as the allocator is valid.
  static std::vector<const FieldTrialEntry*>
  GetAllFieldTrialsFromPersistentAllocator(
      const PersistentMemoryAllocator& allocator);

  // Return a pointer to the data area immediately following the entry.
  uint8_t* GetPickledDataPtr() {
    return UNSAFE_TODO(reinterpret_cast<uint8_t*>(this + 1));
  }
  const uint8_t* GetPickledDataPtr() const {
    return UNSAFE_TODO(reinterpret_cast<const uint8_t*>(this + 1));
  }

  // Whether or not this field trial is activated. This is really just a
  // boolean but using a 32 bit value for portability reasons.
  std::atomic<uint32_t> activated;

  // On e.g. x86, alignof(uint64_t) is 4.  Ensure consistent size and
  // alignment of `pickle_size` across platforms. This can be considered
  // to be padding for the final 32 bit value (activated). If this struct
  // gains or loses fields, consider if this padding is still needed.
  uint32_t padding;

  // Size of the pickled structure, NOT the total size of this entry.
  uint64_t pickle_size;

  // Calling this is only valid when the entry is initialized. That is, it
  // resides in shared memory and has a pickle containing the trial name,
  // group name, and is_overridden.
  bool GetState(std::string_view& trial_name,
                std::string_view& group_name,
                bool& is_overridden) const;

  // Calling this is only valid when the entry is initialized as well. Reads
  // the parameters following the trial and group name and stores them as
  // key-value mappings in |params|.
  bool GetParams(std::map<std::string, std::string>* params) const;

 private:
  // Returns an iterator over the data containing names and params.
  PickleIterator GetPickleIterator() const;

  // Takes the iterator and writes out the first two items into |trial_name|
  // and |group_name|.
  bool ReadStringPair(PickleIterator* iter,
                      std::string_view* trial_name,
                      std::string_view* group_name) const;

  // Reads the field trial header, which includes the name of the trial and
  // group, and the is_overridden bool.
  bool ReadHeader(PickleIterator& iter,
                  std::string_view& trial_name,
                  std::string_view& group_name,
                  bool& is_overridden) const;
};

}  // namespace base::internal

#endif  // BASE_METRICS_FIELD_TRIAL_ENTRY_H_
