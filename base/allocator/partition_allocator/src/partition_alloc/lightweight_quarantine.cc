// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/lightweight_quarantine.h"

#include "partition_alloc/internal_allocator.h"
#include "partition_alloc/partition_page.h"
#include "partition_alloc/partition_root.h"

namespace partition_alloc::internal {

// Utility classes to lock only if a condition is met.

template <>
class PA_SCOPED_LOCKABLE
    LightweightQuarantineBranch::CompileTimeConditionalScopedGuard<
        LightweightQuarantineBranch::LockRequired::kNotRequired> {
 public:
  PA_ALWAYS_INLINE explicit CompileTimeConditionalScopedGuard(Lock& lock)
      PA_EXCLUSIVE_LOCK_FUNCTION(lock) {}
  PA_ALWAYS_INLINE ~CompileTimeConditionalScopedGuard() PA_UNLOCK_FUNCTION() {}
};

template <>
class PA_SCOPED_LOCKABLE
    LightweightQuarantineBranch::CompileTimeConditionalScopedGuard<
        LightweightQuarantineBranch::LockRequired::kRequired> {
 public:
  PA_ALWAYS_INLINE explicit CompileTimeConditionalScopedGuard(Lock& lock)
      PA_EXCLUSIVE_LOCK_FUNCTION(lock)
      : lock_(lock) {
    lock_.Acquire();
  }
  PA_ALWAYS_INLINE ~CompileTimeConditionalScopedGuard() PA_UNLOCK_FUNCTION() {
    lock_.Release();
  }

 private:
  Lock& lock_;
};

class PA_SCOPED_LOCKABLE
    LightweightQuarantineBranch::RuntimeConditionalScopedGuard {
 public:
  PA_ALWAYS_INLINE RuntimeConditionalScopedGuard(bool condition, Lock& lock)
      PA_EXCLUSIVE_LOCK_FUNCTION(lock)
      : condition_(condition), lock_(lock) {
    if (condition_) {
      lock_.Acquire();
    }
  }
  PA_ALWAYS_INLINE ~RuntimeConditionalScopedGuard() PA_UNLOCK_FUNCTION() {
    if (condition_) {
      lock_.Release();
    }
  }

 private:
  const bool condition_;
  Lock& lock_;
};

LightweightQuarantineBranch LightweightQuarantineRoot::CreateBranch(
    const LightweightQuarantineBranchConfig& config) {
  return LightweightQuarantineBranch(*this, config);
}

LightweightQuarantineBranch::LightweightQuarantineBranch(
    Root& root,
    const LightweightQuarantineBranchConfig& config)
    : root_(root),
      lock_required_(config.lock_required),
      branch_capacity_in_bytes_(config.branch_capacity_in_bytes) {}

LightweightQuarantineBranch::LightweightQuarantineBranch(
    LightweightQuarantineBranch&& b)
    : root_(b.root_),
      lock_required_(b.lock_required_),
      slots_(std::move(b.slots_)),
      branch_size_in_bytes_(b.branch_size_in_bytes_),
      branch_capacity_in_bytes_(
          b.branch_capacity_in_bytes_.load(std::memory_order_relaxed)) {
  b.branch_size_in_bytes_ = 0;
}

LightweightQuarantineBranch::~LightweightQuarantineBranch() {
  Purge();
}

bool LightweightQuarantineBranch::IsQuarantinedForTesting(void* object) {
  RuntimeConditionalScopedGuard guard(lock_required_, lock_);
  uintptr_t slot_start =
      root_.allocator_root_.ObjectToSlotStartUnchecked(object);
  for (const auto& slot : slots_) {
    if (slot.slot_start == slot_start) {
      return true;
    }
  }
  return false;
}

void LightweightQuarantineBranch::SetCapacityInBytes(size_t capacity_in_bytes) {
  branch_capacity_in_bytes_.store(capacity_in_bytes, std::memory_order_relaxed);
}

void LightweightQuarantineBranch::Purge() {
  RuntimeConditionalScopedGuard guard(lock_required_, lock_);
  PurgeInternal(0);
  slots_.shrink_to_fit();
}

template <LightweightQuarantineBranch::LockRequired lock_required>
bool LightweightQuarantineBranch::QuarantineInternal(
    void* object,
    SlotSpanMetadata<MetadataKind::kReadOnly>* slot_span,
    uintptr_t slot_start,
    size_t usable_size) {
  PA_DCHECK(lock_required_ ? lock_required == LockRequired::kRequired
                           : lock_required == LockRequired::kNotRequired);
  PA_DCHECK(usable_size == root_.allocator_root_.GetSlotUsableSize(slot_span));

  const size_t capacity_in_bytes =
      branch_capacity_in_bytes_.load(std::memory_order_relaxed);
  if (capacity_in_bytes < usable_size) [[unlikely]] {
    // Even if this branch dequarantines all entries held by it, this entry
    // cannot fit within the capacity.
    root_.allocator_root_.FreeNoHooksImmediate(object, slot_span, slot_start);
    root_.quarantine_miss_count_.fetch_add(1u, std::memory_order_relaxed);
    return false;
  }

  if constexpr (lock_required == LockRequired::kNotRequired) {
    // Although there is no need to actually acquire the lock as
    // LockRequired::kNotRequired is specified,
    // a CompileTimeConditionalScopedGuard is necessary in order to touch
    // `slots_` as `slots_` is annotated with `PA_GUARDED_BY(lock_)`.
    // CompileTimeConditionalScopedGuard's ctor and dtor behave as
    // PA_EXCLUSIVE_LOCK_FUNCTION and PA_UNLOCK_FUNCTION.
    CompileTimeConditionalScopedGuard<lock_required> guard(lock_);

    // Dequarantine some entries as required.
    PurgeInternal(capacity_in_bytes - usable_size);

    // Put the entry onto the list.
    branch_size_in_bytes_ += usable_size;
    slots_.push_back({slot_start, usable_size});

    // Swap randomly so that the quarantine list remain shuffled.
    // This is not uniformly random, but sufficiently random.
    const size_t random_index = random_.RandUint32() % slots_.size();
    std::swap(slots_[random_index], slots_.back());
  } else {
    ToBeFreedArray to_be_freed;
    size_t num_of_slots = 0;

    {
      CompileTimeConditionalScopedGuard<lock_required> guard(lock_);

      // Dequarantine some entries as required. Save the objects to be
      // deallocated into `to_be_freed`.
      PurgeInternalWithDefferedFree(capacity_in_bytes - usable_size,
                                    to_be_freed, num_of_slots);

      // Put the entry onto the list.
      branch_size_in_bytes_ += usable_size;
      slots_.push_back({slot_start, usable_size});

      // Swap randomly so that the quarantine list remain shuffled.
      // This is not uniformly random, but sufficiently random.
      const size_t random_index = random_.RandUint32() % slots_.size();
      std::swap(slots_[random_index], slots_.back());
    }

    // Actually deallocate the dequarantined objects.
    BatchFree(to_be_freed, num_of_slots);
  }

  // Update stats (not locked).
  root_.count_.fetch_add(1, std::memory_order_relaxed);
  root_.size_in_bytes_.fetch_add(usable_size, std::memory_order_relaxed);
  root_.cumulative_count_.fetch_add(1, std::memory_order_relaxed);
  root_.cumulative_size_in_bytes_.fetch_add(usable_size,
                                            std::memory_order_relaxed);
  return true;
}

template bool LightweightQuarantineBranch::QuarantineInternal<
    LightweightQuarantineBranch::LockRequired::kNotRequired>(
    void* object,
    SlotSpanMetadata<MetadataKind::kReadOnly>* slot_span,
    uintptr_t slot_start,
    size_t usable_size);

template bool LightweightQuarantineBranch::QuarantineInternal<
    LightweightQuarantineBranch::LockRequired::kRequired>(
    void* object,
    SlotSpanMetadata<MetadataKind::kReadOnly>* slot_span,
    uintptr_t slot_start,
    size_t usable_size);

PA_ALWAYS_INLINE void LightweightQuarantineBranch::PurgeInternal(
    size_t target_size_in_bytes) {
  int64_t freed_count = 0;
  int64_t freed_size_in_bytes = 0;

  // Dequarantine some entries as required.
  while (target_size_in_bytes < branch_size_in_bytes_) {
    PA_DCHECK(!slots_.empty());

    // As quarantined entries are shuffled, picking last entry is equivalent
    // to picking random entry.
    const auto& to_free = slots_.back();
    size_t to_free_size = to_free.usable_size;

    auto* slot_span = SlotSpanMetadata<MetadataKind::kReadOnly>::FromSlotStart(
        to_free.slot_start);
    void* object = root_.allocator_root_.SlotStartToObject(to_free.slot_start);
    PA_DCHECK(slot_span ==
              SlotSpanMetadata<MetadataKind::kReadOnly>::FromObject(object));

    PA_DCHECK(to_free.slot_start);
    root_.allocator_root_.FreeNoHooksImmediate(object, slot_span,
                                               to_free.slot_start);

    freed_count++;
    freed_size_in_bytes += to_free_size;
    branch_size_in_bytes_ -= to_free_size;

    slots_.pop_back();
  }

  root_.size_in_bytes_.fetch_sub(freed_size_in_bytes,
                                 std::memory_order_relaxed);
  root_.count_.fetch_sub(freed_count, std::memory_order_relaxed);
}

PA_ALWAYS_INLINE void
LightweightQuarantineBranch::PurgeInternalWithDefferedFree(
    size_t target_size_in_bytes,
    ToBeFreedArray& to_be_freed,
    size_t& num_of_slots) {
  num_of_slots = 0;

  int64_t freed_size_in_bytes = 0;

  // Dequarantine some entries as required.
  while (target_size_in_bytes < branch_size_in_bytes_) {
    PA_DCHECK(!slots_.empty());

    // As quarantined entries are shuffled, picking last entry is equivalent to
    // picking random entry.
    const QuarantineSlot& to_free = slots_.back();
    const size_t to_free_size = to_free.usable_size;

    to_be_freed[num_of_slots++] = to_free.slot_start;
    slots_.pop_back();

    freed_size_in_bytes += to_free_size;
    branch_size_in_bytes_ -= to_free_size;

    if (num_of_slots >= kMaxFreeTimesPerPurge) {
      break;
    }
  }

  root_.size_in_bytes_.fetch_sub(freed_size_in_bytes,
                                 std::memory_order_relaxed);
  root_.count_.fetch_sub(num_of_slots, std::memory_order_relaxed);
}

PA_ALWAYS_INLINE void LightweightQuarantineBranch::BatchFree(
    const ToBeFreedArray& to_be_freed,
    size_t num_of_slots) {
  for (size_t i = 0; i < num_of_slots; ++i) {
    const uintptr_t slot_start = to_be_freed[i];
    PA_DCHECK(slot_start);
    auto* slot_span =
        SlotSpanMetadata<MetadataKind::kReadOnly>::FromSlotStart(slot_start);
    void* object = root_.allocator_root_.SlotStartToObject(slot_start);
    PA_DCHECK(slot_span ==
              SlotSpanMetadata<MetadataKind::kReadOnly>::FromObject(object));
    root_.allocator_root_.FreeNoHooksImmediate(object, slot_span, slot_start);
  }
}

}  // namespace partition_alloc::internal
