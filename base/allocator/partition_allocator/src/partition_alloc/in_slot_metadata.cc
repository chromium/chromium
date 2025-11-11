// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/slot_start.h"
#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "partition_alloc/in_slot_metadata.h"

#include <atomic>

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_base/logging.h"
#include "partition_alloc/partition_bucket.h"
#include "partition_alloc/partition_freelist_entry.h"
#include "partition_alloc/partition_page.h"
#include "partition_alloc/partition_root.h"
#include "partition_alloc/thread_cache.h"

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

namespace partition_alloc::internal {

namespace {

// If double-free, the freed `slot` will be a freelist entry.
bool IsInFreelist(UntaggedSlotStart slot_start,
                  SlotSpanMetadata* slot_span,
                  size_t& position) {
  size_t slot_size = slot_span->bucket->slot_size;

  // Next check if the `slot_start` is in the `slot_span`'s freelist.
  FreelistEntry* node = slot_span->get_freelist_head();

  size_t length = slot_span->GetFreelistLength();
  size_t index = 0;
  while (node != nullptr && index < length) {
    if (UntagAddr(reinterpret_cast<uintptr_t>(node)) == slot_start.value()) {
      // This means `double-free`.
      position = index;
      return true;
    }
    // GetNext() causes crash if the freelist is corrupted.
    node = node->GetNext(slot_size);
    ++index;
  }
  position = 0;
  return false;
}

#if PA_BUILDFLAG(IS_IOS)
std::atomic_bool suppress_double_free_detected_crash = false;
std::atomic_bool suppress_corruption_detected_crash = false;
#endif  // PA_BUILDFLAG(IS_IOS)
std::atomic<void (*)(uintptr_t)> corruption_detected_fn = nullptr;
}  // namespace

// static
void InSlotMetadata::SetCorruptionDetectedFn(void (*fn)(uintptr_t)) {
  corruption_detected_fn.store(fn, std::memory_order_relaxed);
}

#if !PA_BUILDFLAG(IS_IOS)
[[noreturn]]
#endif  // !PA_BUILDFLAG(IS_IOS)
PA_NOINLINE PA_NOT_TAIL_CALLED void DoubleFreeDetected(size_t position) {
#if PA_BUILDFLAG(IS_IOS)
  if (suppress_double_free_detected_crash) {
    return;
  }
#endif  // PA_BUILDFLAG(IS_IOS)

  // If the double free happens very soon, `position` will be small.
  // We can use the value to estimate how large buffer we need to remember
  // freed slots. i.e. |slot_size * position| bytes.
  PA_DEBUG_DATA_ON_STACK("entrypos", position);
  // If we want to add more data related to the double-free, we will
  // add PA_DEBUG_DATA_ON_STACK() here.
  PA_NO_CODE_FOLDING();
  PA_IMMEDIATE_CRASH();
}

#if !PA_BUILDFLAG(IS_IOS)
[[noreturn]]
#endif  // !PA_BUILDFLAG(IS_IOS)
PA_NOINLINE PA_NOT_TAIL_CALLED void CorruptionDetected() {
#if PA_BUILDFLAG(IS_IOS)
  if (suppress_corruption_detected_crash) {
    return;
  }
#endif  // PA_BUILDFLAG(IS_IOS)

  // If we want to add more data related to the corruption, we will
  // add PA_DEBUG_DATA_ON_STACK() here.
  PA_NO_CODE_FOLDING();
  PA_IMMEDIATE_CRASH();
}

#if !PA_BUILDFLAG(IS_IOS)
[[noreturn]]
#endif  // !PA_BUILDFLAG(IS_IOS)
PA_NOINLINE PA_NOT_TAIL_CALLED void
InSlotMetadata::DoubleFreeOrCorruptionDetected(InSlotMetadata::CountType count,
                                               UntaggedSlotStart slot_start,
                                               SlotSpanMetadata* slot_span) {
  // Lock the PartitionRoot here, because to travserse SlotSpanMetadata's
  // freelist, we need PartitionRootLock().
  PartitionRoot* root = PartitionRoot::FromSlotSpanMetadata(slot_span);
  size_t slot_size = slot_span->bucket->slot_size;
  size_t position = 0;
  ScopedGuard scope(PartitionRootLock(root));

  PA_DEBUG_DATA_ON_STACK("refcount", count);
  // Record `slot_size` here. If crashes inside IsInFreelist(), minidump
  // will have `slot_size` in its stack data.
  PA_DEBUG_DATA_ON_STACK("slotsize", slot_size);

  void (*hook)(uintptr_t) =
      corruption_detected_fn.load(std::memory_order_relaxed);
  if (hook) {
    (*hook)(slot_start.value());
  }

  auto* thread_cache = root->GetThreadCache();
  if (ThreadCache::IsValid(thread_cache)) {
    size_t bucket_index = slot_span->bucket - root->buckets;
    if (thread_cache->IsInFreelist(slot_start, bucket_index, position)) {
      DoubleFreeDetected(position);
    }
  }
  if (IsInFreelist(slot_start, slot_span, position)) {
    DoubleFreeDetected(position);
  }

  CorruptionDetected();
}

#if PA_BUILDFLAG(IS_IOS)
void SuppressDoubleFreeDetectedCrash() {
  suppress_double_free_detected_crash = true;
}
void SuppressCorruptionDetectedCrash() {
  suppress_corruption_detected_crash = true;
}
#endif  // PA_BUILDFLAG(IS_IOS)

}  // namespace partition_alloc::internal

#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
