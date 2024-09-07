// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/lightweight_quarantine.h"

#include "partition_alloc/internal_allocator.h"
#include "partition_alloc/partition_page.h"
#include "partition_alloc/partition_root.h"

namespace partition_alloc::internal {
namespace {

// An utility to lock only if a condition is met.
class PA_SCOPED_LOCKABLE ConditionalScopedGuard {
 public:
  PA_ALWAYS_INLINE ConditionalScopedGuard(bool condition, Lock& lock)
      PA_EXCLUSIVE_LOCK_FUNCTION(lock)
      : condition_(condition), lock_(lock) {
    if (condition_) {
      lock_.Acquire();
    }
  }
  PA_ALWAYS_INLINE ~ConditionalScopedGuard() PA_UNLOCK_FUNCTION() {
    if (condition_) {
      lock_.Release();
    }
  }

 private:
  const bool condition_;
  Lock& lock_;
};

}  // namespace

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

bool LightweightQuarantineBranch::Quarantine(
    void* object,
    SlotSpanMetadata<MetadataKind::kReadOnly>* slot_span,
    uintptr_t slot_start,
    size_t usable_size) {
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

  {
    ConditionalScopedGuard guard(lock_required_, lock_);

    // Dequarantine some entries as required.
    PurgeInternal(capacity_in_bytes - usable_size);

    // Put the entry onto the list.
    branch_size_in_bytes_ += usable_size;
    slots_.push_back({slot_start, usable_size});

    // Swap randomly so that the quarantine list remain shuffled.
    // This is not uniformly random, but sufficiently random.
    const size_t random_index = random_.RandUint32() % slots_.size();
    std::swap(slots_[random_index], slots_.back());
  }

  // Update stats (not locked).
  root_.count_.fetch_add(1, std::memory_order_relaxed);
  root_.size_in_bytes_.fetch_add(usable_size, std::memory_order_relaxed);
  root_.cumulative_count_.fetch_add(1, std::memory_order_relaxed);
  root_.cumulative_size_in_bytes_.fetch_add(usable_size,
                                            std::memory_order_relaxed);
  return true;
}

bool LightweightQuarantineBranch::IsQuarantinedForTesting(void* object) {
  ConditionalScopedGuard guard(lock_required_, lock_);
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
  ConditionalScopedGuard guard(lock_required_, lock_);
  PurgeInternal(0);
  PA_DCHECK(slots_.empty());
  slots_.shrink_to_fit();
}

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

}  // namespace partition_alloc::internal
