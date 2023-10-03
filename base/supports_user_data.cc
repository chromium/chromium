// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/supports_user_data.h"

#include "base/feature_list.h"
#include "base/sequence_checker.h"

namespace base {

std::unique_ptr<SupportsUserData::Data> SupportsUserData::Data::Clone() {
  return nullptr;
}

SupportsUserData::SupportsUserData() {
  // Harmless to construct on a different execution sequence to subsequent
  // usage.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

SupportsUserData::SupportsUserData(SupportsUserData&&) = default;
SupportsUserData& SupportsUserData::operator=(SupportsUserData&&) = default;

SupportsUserData::Data* SupportsUserData::GetUserData(const void* key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Avoid null keys; they are too vulnerable to collision.
  DCHECK(key);
  auto found = user_data_.find(key);
  if (found != user_data_.end()) {
    return found->second.get();
  }
  return nullptr;
}

std::unique_ptr<SupportsUserData::Data> SupportsUserData::TakeUserData(
    const void* key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Null keys are too vulnerable to collision.
  CHECK(key);
  auto found = user_data_.find(key);
  if (found != user_data_.end()) {
    std::unique_ptr<SupportsUserData::Data> deowned;
    deowned.swap(found->second);
    user_data_.erase(key);
    return deowned;
  }
  return nullptr;
}

void SupportsUserData::SetUserData(const void* key,
                                   std::unique_ptr<Data> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!in_destructor_) << "Calling SetUserData() when SupportsUserData is "
                            "being destroyed is not supported.";
  // Avoid null keys; they are too vulnerable to collision.
  DCHECK(key);
  if (data.get()) {
    user_data_[key] = std::move(data);
  } else {
    RemoveUserData(key);
  }
}

void SupportsUserData::RemoveUserData(const void* key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = user_data_.find(key);
  if (it != user_data_.end()) {
    // Remove the entry from the map before deleting `owned_data` to avoid
    // reentrancy issues when `owned_data` owns `this`. Otherwise:
    //
    // 1. `RemoveUserData()` calls `erase()`.
    // 2. `erase()` deletes `owned_data`.
    // 3. `owned_data` deletes `this`.
    //
    // At this point, `erase()` is still on the stack even though the
    // backing map (owned by `this`) has already been destroyed, and it
    // may simply crash, cause a use-after-free, or any other number of
    // interesting things.
    auto owned_data = std::move(it->second);
    user_data_.erase(it);
  }
}

void SupportsUserData::DetachFromSequence() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void SupportsUserData::CloneDataFrom(const SupportsUserData& other) {
  for (const auto& data_pair : other.user_data_) {
    auto cloned_data = data_pair.second->Clone();
    if (cloned_data) {
      SetUserData(data_pair.first, std::move(cloned_data));
    }
  }
}

SupportsUserData::~SupportsUserData() {
  if (!user_data_.empty()) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }
  in_destructor_ = true;
  absl::flat_hash_map<const void*, std::unique_ptr<Data>> user_data;
  user_data_.swap(user_data);
  // Now this->user_data_ is empty, and any destructors called transitively from
  // the destruction of |local_user_data| will see it that way instead of
  // examining a being-destroyed object.
}

void SupportsUserData::ClearAllUserData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  user_data_.clear();
}

}  // namespace base
