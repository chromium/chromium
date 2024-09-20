// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/sequence_local_storage_map.h"

#include <ostream>
#include <utility>

#include "base/check_op.h"
#include "base/sequence_token.h"

namespace base {
namespace internal {

namespace {

constinit thread_local SequenceLocalStorageMap* current_sequence_local_storage =
    nullptr;

}  // namespace

SequenceLocalStorageMap::SequenceLocalStorageMap() = default;

SequenceLocalStorageMap::~SequenceLocalStorageMap() = default;

// static
SequenceLocalStorageMap& SequenceLocalStorageMap::GetForCurrentThread() {
  CHECK(!CurrentTaskIsRunningSynchronously());
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

bool SequenceLocalStorageMap::Has(int slot_id) const {
  return const_cast<SequenceLocalStorageMap*>(this)->Get(slot_id) != nullptr;
}

void SequenceLocalStorageMap::Reset(int slot_id) {
  sls_map_.erase(slot_id);
}

SequenceLocalStorageMap::Value* SequenceLocalStorageMap::Get(int slot_id) {
  auto it = sls_map_.find(slot_id);
  if (it != sls_map_.end()) {
    return it->second.get();
  }
  return nullptr;
}

SequenceLocalStorageMap::Value* SequenceLocalStorageMap::Set(
    int slot_id,
    SequenceLocalStorageMap::ValueDestructorPair value_destructor_pair) {
  auto it = sls_map_.find(slot_id);

  if (it == sls_map_.end())
    it = sls_map_.emplace(slot_id, std::move(value_destructor_pair)).first;
  else
    it->second = std::move(value_destructor_pair);

  // The maximum number of entries in the map is 256. This can be adjusted, but
  // will require reviewing the choice of data structure for the map.
  DCHECK_LE(sls_map_.size(), 256U);
  return it->second.get();
}

SequenceLocalStorageMap::ValueDestructorPair::ValueDestructorPair()
    : destructor_(nullptr) {}

SequenceLocalStorageMap::ValueDestructorPair::ValueDestructorPair(
    ExternalValue value,
    DestructorFunc* destructor)
    : value_{.external_value = std::move(value)}, destructor_(destructor) {}

SequenceLocalStorageMap::ValueDestructorPair::ValueDestructorPair(
    InlineValue value,
    DestructorFunc* destructor)
    : value_{.inline_value = std::move(value)}, destructor_(destructor) {}

SequenceLocalStorageMap::ValueDestructorPair::~ValueDestructorPair() {
  if (destructor_) {
    destructor_(&value_);
  }
}

SequenceLocalStorageMap::ValueDestructorPair::ValueDestructorPair(
    ValueDestructorPair&& value_destructor_pair)
    : value_(value_destructor_pair.value_),
      destructor_(value_destructor_pair.destructor_) {
  value_destructor_pair.destructor_ = nullptr;
}

SequenceLocalStorageMap::ValueDestructorPair&
SequenceLocalStorageMap::ValueDestructorPair::operator=(
    ValueDestructorPair&& value_destructor_pair) {
  if (this == &value_destructor_pair) {
    return *this;
  }
  // Destroy |value_| before overwriting it with a new value.
  if (destructor_) {
    destructor_(&value_);
  }
  value_ = value_destructor_pair.value_;
  destructor_ = std::exchange(value_destructor_pair.destructor_, nullptr);

  return *this;
}

SequenceLocalStorageMap::ValueDestructorPair::operator bool() const {
  return destructor_ != nullptr;
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
