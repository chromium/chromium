// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/sequence_local_storage_map.h"

#include <ostream>
#include <utility>

#include "base/check_op.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"

namespace base {
namespace internal {

namespace {

ABSL_CONST_INIT thread_local SequenceLocalStorageMap*
    current_sequence_local_storage = nullptr;

}  // namespace

SequenceLocalStorageMap::SequenceLocalStorageMap() = default;

SequenceLocalStorageMap::~SequenceLocalStorageMap() = default;

// static
SequenceLocalStorageMap& SequenceLocalStorageMap::GetForCurrentThread() {
  DCHECK(IsSetForCurrentThread())
      << "SequenceLocalStorageSlot cannot be used because no "
         "SequenceLocalStorageMap was stored in TLS. Use "
         "ScopedSetSequenceLocalStorageMapForCurrentThread to store a "
         "SequenceLocalStorageMap object in TLS.";

  return *current_sequence_local_storage;
}

// static
bool SequenceLocalStorageMap::IsSetForCurrentThread() {
  return current_sequence_local_storage != nullptr;
}

void* SequenceLocalStorageMap::Get(int slot_id) {
  const auto it = sls_map_.find(slot_id);
  if (it == sls_map_.end())
    return nullptr;
  return it->second.value();
}

void SequenceLocalStorageMap::Set(
    int slot_id,
    SequenceLocalStorageMap::ValueDestructorPair value_destructor_pair) {
  auto it = sls_map_.find(slot_id);

  if (it == sls_map_.end())
    sls_map_.emplace(slot_id, std::move(value_destructor_pair));
  else
    it->second = std::move(value_destructor_pair);

  // The maximum number of entries in the map is 256. This can be adjusted, but
  // will require reviewing the choice of data structure for the map.
  DCHECK_LE(sls_map_.size(), 256U);
}

SequenceLocalStorageMap::ValueDestructorPair::ValueDestructorPair(
    void* value,
    DestructorFunc* destructor)
    : value_(value), destructor_(destructor) {}

SequenceLocalStorageMap::ValueDestructorPair::~ValueDestructorPair() {
  if (value_)
    destructor_(value_);
}

SequenceLocalStorageMap::ValueDestructorPair::ValueDestructorPair(
    ValueDestructorPair&& value_destructor_pair)
    : value_(value_destructor_pair.value_),
      destructor_(value_destructor_pair.destructor_) {
  value_destructor_pair.value_ = nullptr;
}

SequenceLocalStorageMap::ValueDestructorPair&
SequenceLocalStorageMap::ValueDestructorPair::operator=(
    ValueDestructorPair&& value_destructor_pair) {
  // Destroy |value_| before overwriting it with a new value.
  if (value_)
    destructor_(value_);

  value_ = value_destructor_pair.value_;
  destructor_ = value_destructor_pair.destructor_;

  value_destructor_pair.value_ = nullptr;

  return *this;
}

ScopedSetSequenceLocalStorageMapForCurrentThread::
    ScopedSetSequenceLocalStorageMapForCurrentThread(
        SequenceLocalStorageMap* sequence_local_storage)
    : resetter_(&current_sequence_local_storage,
                sequence_local_storage,
                nullptr) {}

ScopedSetSequenceLocalStorageMapForCurrentThread::
    ~ScopedSetSequenceLocalStorageMapForCurrentThread() = default;

}  // namespace internal
}  // namespace base
