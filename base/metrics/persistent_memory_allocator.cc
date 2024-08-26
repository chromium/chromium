// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/metrics/persistent_memory_allocator.h"

#include <assert.h>

#include <algorithm>
#include <atomic>
#include <optional>
#include <string_view>

#include "base/bits.h"
#include "base/containers/contains.h"
#include "base/debug/alias.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/sparse_histogram.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/system/sys_info.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
// Must be after <windows.h>
#include <winbase.h>
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include <sys/mman.h>
#if BUILDFLAG(IS_ANDROID)
#include <sys/prctl.h>
#endif
#endif

namespace {

// Limit of memory segment size. It has to fit in an unsigned 32-bit number
// and should be a power of 2 in order to accommodate almost any page size.
constexpr uint32_t kSegmentMaxSize = 1 << 30;  // 1 GiB

// A constant (random) value placed in the shared metadata to identify
// an already initialized memory segment.
constexpr uint32_t kGlobalCookie = 0x408305DC;

// The current version of the metadata. If updates are made that change
// the metadata, the version number can be queried to operate in a backward-
// compatible manner until the memory segment is completely re-initalized.
// Note: If you update the metadata in a non-backwards compatible way, reset
// |kCompatibleVersions|. Otherwise, add the previous version.
constexpr uint32_t kGlobalVersion = 3;
static constexpr uint32_t kOldCompatibleVersions[] = {2};

// Constant values placed in the block headers to indicate its state.
constexpr uint32_t kBlockCookieFree = 0;
constexpr uint32_t kBlockCookieQueue = 1;
constexpr uint32_t kBlockCookieWasted = 0x4B594F52;
constexpr uint32_t kBlockCookieAllocated = 0xC8799269;

// TODO(bcwhite): When acceptable, consider moving flags to std::atomic<char>
// types rather than combined bitfield.

// Flags stored in the flags_ field of the SharedMetadata structure below.
constexpr uint32_t kFlagCorrupt = 1 << 0;
constexpr uint32_t kFlagFull = 1 << 1;

// Errors that are logged in "errors" histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum AllocatorError : int {
  kMemoryIsCorrupt = 1,
  kMaxValue = kMemoryIsCorrupt,
};

bool CheckFlag(const volatile std::atomic<uint32_t>* flags, uint32_t flag) {
  uint32_t loaded_flags = flags->load(std::memory_order_relaxed);
  return (loaded_flags & flag) != 0;
}

void SetFlag(volatile std::atomic<uint32_t>* flags, uint32_t flag) {
  uint32_t loaded_flags = flags->load(std::memory_order_relaxed);
  for (;;) {
    uint32_t new_flags = (loaded_flags & ~flag) | flag;
    // In the failue case, actual "flags" value stored in loaded_flags.
    // These access are "relaxed" because they are completely independent
    // of all other values.
    if (flags->compare_exchange_weak(loaded_flags, new_flags,
                                     std::memory_order_relaxed,
                                     std::memory_order_relaxed)) {
      break;
    }
  }
}

}  // namespace

namespace base {

// The block-header is placed at the top of every allocation within the
// segment to describe the data that follows it.
struct PersistentMemoryAllocator::BlockHeader {
  uint32_t size;       // Number of bytes in this block, including header.
  uint32_t cookie;     // Constant value indicating completed allocation.
  std::atomic<uint32_t> type_id;  // Arbitrary number indicating data type.
  std::atomic<uint32_t> next;     // Pointer to the next block when iterating.
};

// The shared metadata exists once at the top of the memory segment to
// describe the state of the allocator to all processes. The size of this
// structure must be a multiple of 64-bits to ensure compatibility between
// architectures.
struct PersistentMemoryAllocator::SharedMetadata {
  uint32_t cookie;     // Some value that indicates complete initialization.
  uint32_t size;       // Total size of memory segment.
  uint32_t page_size;  // Paging size within memory segment.
  uint32_t version;    // Version code so upgrades don't break.
  uint64_t id;         // Arbitrary ID number given by creator.
  uint32_t name;       // Reference to stored name string.
  uint32_t padding1;   // Pad-out read-only data to 64-bit alignment.

  // Above is read-only after first construction. Below may be changed and
  // so must be marked "volatile" to provide correct inter-process behavior.

  // State of the memory, plus some padding to keep alignment.
  volatile std::atomic<uint8_t> memory_state;  // MemoryState enum values.
  uint8_t padding2[3];

  // Bitfield of information flags. Access to this should be done through
  // the CheckFlag() and SetFlag() methods defined above.
  volatile std::atomic<uint32_t> flags;

  // Offset/reference to first free space in segment.
  volatile std::atomic<uint32_t> freeptr;

  // The "iterable" queue is an M&S Queue as described here, append-only:
  // https://www.research.ibm.com/people/m/michael/podc-1996.pdf
  // |queue| needs to be 64-bit aligned and is itself a multiple of 64 bits.
  volatile std::atomic<uint32_t> tailptr;  // Last block of iteration queue.
  volatile BlockHeader queue;   // Empty block for linked-list head/tail.
};

// The "queue" block header is used to detect "last node" so that zero/null
// can be used to indicate that it hasn't been added at all. It is part of
// the SharedMetadata structure which itself is always located at offset zero.
const PersistentMemoryAllocator::Reference
    PersistentMemoryAllocator::kReferenceQueue =
        offsetof(SharedMetadata, queue);

const base::FilePath::CharType PersistentMemoryAllocator::kFileExtension[] =
    FILE_PATH_LITERAL(".pma");


PersistentMemoryAllocator::Iterator::Iterator(
    const PersistentMemoryAllocator* allocator)
    : allocator_(allocator), last_record_(kReferenceQueue), record_count_(0) {}

PersistentMemoryAllocator::Iterator::Iterator(
    const PersistentMemoryAllocator* allocator,
    Reference starting_after)
    : allocator_(allocator), last_record_(0), record_count_(0) {
  Reset(starting_after);
}

PersistentMemoryAllocator::Iterator::~Iterator() = default;

void PersistentMemoryAllocator::Iterator::Reset() {
  last_record_.store(kReferenceQueue, std::memory_order_relaxed);
  record_count_.store(0, std::memory_order_relaxed);
}

void PersistentMemoryAllocator::Iterator::Reset(Reference starting_after) {
  if (starting_after == 0) {
    Reset();
    return;
  }

  last_record_.store(starting_after, std::memory_order_relaxed);
  record_count_.store(0, std::memory_order_relaxed);

  // Ensure that the starting point is a valid, iterable block (meaning it can
  // be read and has a non-zero "next" pointer).
  const volatile BlockHeader* block =
      allocator_->GetBlock(starting_after, 0, 0, false, false);
  if (!block || block->next.load(std::memory_order_relaxed) == 0) {
    NOTREACHED();
  }
}

PersistentMemoryAllocator::Reference
PersistentMemoryAllocator::Iterator::GetLast() {
  Reference last = last_record_.load(std::memory_order_relaxed);
  if (last == kReferenceQueue)
    return kReferenceNull;
  return last;
}

PersistentMemoryAllocator::Reference
PersistentMemoryAllocator::Iterator::GetNext(uint32_t* type_return) {
  // Make a copy of the existing count of found-records, acquiring all changes
  // made to the allocator, notably "freeptr" (see comment in loop for why
  // the load of that value cannot be moved above here) that occurred during
  // any previous runs of this method, including those by parallel threads
  // that interrupted it. It pairs with the Release at the end of this method.
  //
  // Otherwise, if the compiler were to arrange the two loads such that
  // "count" was fetched _after_ "freeptr" then it would be possible for
  // this thread to be interrupted between them and other threads perform
  // multiple allocations, make-iterables, and iterations (with the included
  // increment of |record_count_|) culminating in the check at the bottom
  // mistakenly determining that a loop exists. Isn't this stuff fun?
  uint32_t count = record_count_.load(std::memory_order_acquire);

  Reference last = last_record_.load(std::memory_order_acquire);
  Reference next;
  while (true) {
    const volatile BlockHeader* block =
        allocator_->GetBlock(last, 0, 0, true, false);
    if (!block)  // Invalid iterator state.
      return kReferenceNull;

    // The compiler and CPU can freely reorder all memory accesses on which
    // there are no dependencies. It could, for example, move the load of
    // "freeptr" to above this point because there are no explicit dependencies
    // between it and "next". If it did, however, then another block could
    // be queued after that but before the following load meaning there is
    // one more queued block than the future "detect loop by having more
    // blocks that could fit before freeptr" will allow.
    //
    // By "acquiring" the "next" value here, it's synchronized to the enqueue
    // of the node which in turn is synchronized to the allocation (which sets
    // freeptr). Thus, the scenario above cannot happen.
    next = block->next.load(std::memory_order_acquire);
    if (next == kReferenceQueue)  // No next allocation in queue.
      return kReferenceNull;
    block = allocator_->GetBlock(next, 0, 0, false, false);
    if (!block) {  // Memory is corrupt.
      allocator_->SetCorrupt();
      return kReferenceNull;
    }

    // Update the "last_record" pointer to be the reference being returned.
    // If it fails then another thread has already iterated past it so loop
    // again. Failing will also load the existing value into "last" so there
    // is no need to do another such load when the while-loop restarts. A
    // "strong" compare-exchange is used because failing unnecessarily would
    // mean repeating some fairly costly validations above.
    if (last_record_.compare_exchange_strong(
            last, next, std::memory_order_acq_rel, std::memory_order_acquire)) {
      *type_return = block->type_id.load(std::memory_order_relaxed);
      break;
    }
  }

  // Memory corruption could cause a loop in the list. Such must be detected
  // so as to not cause an infinite loop in the caller. This is done by simply
  // making sure it doesn't iterate more times than the absolute maximum
  // number of allocations that could have been made. Callers are likely
  // to loop multiple times before it is detected but at least it stops.
  const uint32_t freeptr = std::min(
      allocator_->shared_meta()->freeptr.load(std::memory_order_relaxed),
      allocator_->mem_size_);
  const uint32_t max_records =
      freeptr / (sizeof(BlockHeader) + kAllocAlignment);
  if (count > max_records) {
    allocator_->SetCorrupt();
    return kReferenceNull;
  }

  // Increment the count and release the changes made above. It pairs with
  // the Acquire at the top of this method. Note that this operation is not
  // strictly synchonized with fetching of the object to return, which would
  // have to be done inside the loop and is somewhat complicated to achieve.
  // It does not matter if it falls behind temporarily so long as it never
  // gets ahead.
  record_count_.fetch_add(1, std::memory_order_release);
  return next;
}

PersistentMemoryAllocator::Reference
PersistentMemoryAllocator::Iterator::GetNextOfType(uint32_t type_match) {
  Reference ref;
  uint32_t type_found;
  while ((ref = GetNext(&type_found)) != 0) {
    if (type_found == type_match)
      return ref;
  }
  return kReferenceNull;
}


// static
bool PersistentMemoryAllocator::IsMemoryAcceptable(const void* base,
                                                   size_t size,
                                                   size_t page_size,
                                                   bool readonly) {
  return ((base && reinterpret_cast<uintptr_t>(base) % kAllocAlignment == 0) &&
          (size >= sizeof(SharedMetadata) && size <= kSegmentMaxSize) &&
          (size % kAllocAlignment == 0 || readonly) &&
          (page_size == 0 || size % page_size == 0 || readonly));
}

PersistentMemoryAllocator::PersistentMemoryAllocator(void* base,
                                                     size_t size,
                                                     size_t page_size,
                                                     uint64_t id,
                                                     std::string_view name,
                                                     AccessMode access_mode)
    : PersistentMemoryAllocator(Memory(base, MEM_EXTERNAL),
                                size,
                                page_size,
                                id,
                                name,
                                access_mode) {}

PersistentMemoryAllocator::PersistentMemoryAllocator(Memory memory,
                                                     size_t size,
                                                     size_t page_size,
                                                     uint64_t id,
                                                     std::string_view name,
                                                     AccessMode access_mode)
    : mem_base_(static_cast<char*>(memory.base)),
      mem_type_(memory.type),
      mem_size_(checked_cast<uint32_t>(size)),
      mem_page_(checked_cast<uint32_t>((page_size ? page_size : size))),
#if BUILDFLAG(IS_NACL)
      vm_page_size_(4096U),  // SysInfo is not built for NACL.
#else
      vm_page_size_(SysInfo::VMAllocationGranularity()),
#endif
      access_mode_(access_mode) {
  // These asserts ensure that the structures are 32/64-bit agnostic and meet
  // all the requirements of use within the allocator. They access private
  // definitions and so cannot be moved to the global scope.
  static_assert(sizeof(PersistentMemoryAllocator::BlockHeader) == 16,
                "struct is not portable across different natural word widths");
  static_assert(sizeof(PersistentMemoryAllocator::SharedMetadata) == 64,
                "struct is not portable across different natural word widths");

  static_assert(sizeof(BlockHeader) % kAllocAlignment == 0,
                "BlockHeader is not a multiple of kAllocAlignment");
  static_assert(sizeof(SharedMetadata) % kAllocAlignment == 0,
                "SharedMetadata is not a multiple of kAllocAlignment");
  static_assert(kReferenceQueue % kAllocAlignment == 0,
                "\"queue\" is not aligned properly; must be at end of struct");

  // Ensure that memory segment is of acceptable size.
  const bool readonly = access_mode == kReadOnly;
  CHECK(IsMemoryAcceptable(memory.base, size, page_size, readonly));

  // These atomics operate inter-process and so must be lock-free.
  DCHECK(SharedMetadata().freeptr.is_lock_free());
  DCHECK(SharedMetadata().flags.is_lock_free());
  DCHECK(BlockHeader().next.is_lock_free());
  CHECK(corrupt_.is_lock_free());

  // When calling SetCorrupt() during initialization, don't write to the memory
  // in kReadOnly and kReadWriteExisting modes.
  const bool allow_write_for_set_corrupt = (access_mode == kReadWrite);
  if (shared_meta()->cookie != kGlobalCookie) {
    if (access_mode != kReadWrite) {
      SetCorrupt(allow_write_for_set_corrupt);
      return;
    }

    // This block is only executed when a completely new memory segment is
    // being initialized. It's unshared and single-threaded...
    volatile BlockHeader* const first_block =
        reinterpret_cast<volatile BlockHeader*>(mem_base_ +
                                                sizeof(SharedMetadata));
    if (shared_meta()->cookie != 0 ||
        shared_meta()->size != 0 ||
        shared_meta()->version != 0 ||
        shared_meta()->freeptr.load(std::memory_order_relaxed) != 0 ||
        shared_meta()->flags.load(std::memory_order_relaxed) != 0 ||
        shared_meta()->id != 0 ||
        shared_meta()->name != 0 ||
        shared_meta()->tailptr != 0 ||
        shared_meta()->queue.cookie != 0 ||
        shared_meta()->queue.next.load(std::memory_order_relaxed) != 0 ||
        first_block->size != 0 ||
        first_block->cookie != 0 ||
        first_block->type_id.load(std::memory_order_relaxed) != 0 ||
        first_block->next != 0) {
      // ...or something malicious has been playing with the metadata.
      CHECK(allow_write_for_set_corrupt);
      SetCorrupt(allow_write_for_set_corrupt);
    }

    // This is still safe to do even if corruption has been detected.
    shared_meta()->cookie = kGlobalCookie;
    shared_meta()->size = mem_size_;
    shared_meta()->page_size = mem_page_;
    shared_meta()->version = kGlobalVersion;
    shared_meta()->id = id;
    // Don't overwrite `freeptr` if it is set since we could have raced with
    // another allocator. In such a case, `freeptr` would get "rewinded", and
    // new objects would be allocated on top of already allocated objects.
    uint32_t empty_freeptr = 0;
    shared_meta()->freeptr.compare_exchange_strong(
        /*expected=*/empty_freeptr, /*desired=*/sizeof(SharedMetadata),
        /*success=*/std::memory_order_release,
        /*failure=*/std::memory_order_relaxed);

    // Set up the queue of iterable allocations.
    shared_meta()->queue.size = sizeof(BlockHeader);
    shared_meta()->queue.cookie = kBlockCookieQueue;
    shared_meta()->queue.next.store(kReferenceQueue, std::memory_order_release);
    shared_meta()->tailptr.store(kReferenceQueue, std::memory_order_release);

    // Allocate space for the name so other processes can learn it.
    if (!name.empty()) {
      const size_t name_length = name.length() + 1;
      shared_meta()->name = Allocate(name_length, 0);
      char* name_cstr = GetAsArray<char>(shared_meta()->name, 0, name_length);
      if (name_cstr)
        memcpy(name_cstr, name.data(), name.length());
    }

    shared_meta()->memory_state.store(MEMORY_INITIALIZED,
                                      std::memory_order_release);
  } else {
    if (shared_meta()->size == 0 ||
        (shared_meta()->version != kGlobalVersion &&
         !Contains(kOldCompatibleVersions, shared_meta()->version)) ||
        shared_meta()->freeptr.load(std::memory_order_relaxed) == 0 ||
        shared_meta()->tailptr == 0 || shared_meta()->queue.cookie == 0 ||
        shared_meta()->queue.next.load(std::memory_order_relaxed) == 0) {
      SetCorrupt(allow_write_for_set_corrupt);
    }
    if (!readonly) {
      // The allocator is attaching to a previously initialized segment of
      // memory. If the initialization parameters differ, make the best of it
      // by reducing the local construction parameters to match those of the
      // actual memory area. This ensures that the local object never tries to
      // write outside of the original bounds.
      // Because the fields are const to ensure that no code other than the
      // constructor makes changes to them as well as to give optimization hints
      // to the compiler, it's necessary to const-cast them for changes here.
      if (shared_meta()->size < mem_size_)
        *const_cast<uint32_t*>(&mem_size_) = shared_meta()->size;
      if (shared_meta()->page_size < mem_page_)
        *const_cast<uint32_t*>(&mem_page_) = shared_meta()->page_size;

      // Ensure that settings are still valid after the above adjustments.
      if (!IsMemoryAcceptable(memory.base, mem_size_, mem_page_, readonly)) {
        SetCorrupt(allow_write_for_set_corrupt);
      }
    }
  }
}

PersistentMemoryAllocator::~PersistentMemoryAllocator() {
  // It's strictly forbidden to do any memory access here in case there is
  // some issue with the underlying memory segment. The "Local" allocator
  // makes use of this to allow deletion of the segment on the heap from
  // within its destructor.
}

uint64_t PersistentMemoryAllocator::Id() const {
  return shared_meta()->id;
}

const char* PersistentMemoryAllocator::Name() const {
  Reference name_ref = shared_meta()->name;
  const char* name_cstr =
      GetAsArray<char>(name_ref, 0, PersistentMemoryAllocator::kSizeAny);
  if (!name_cstr)
    return "";

  size_t name_length = GetAllocSize(name_ref);
  if (name_cstr[name_length - 1] != '\0') {
    NOTREACHED();
  }

  return name_cstr;
}

void PersistentMemoryAllocator::CreateTrackingHistograms(
    std::string_view name) {
  if (name.empty() || access_mode_ == kReadOnly) {
    return;
  }
  std::string name_string(name);

  DCHECK(!used_histogram_);
  used_histogram_ = LinearHistogram::FactoryGet(
      "UMA.PersistentAllocator." + name_string + ".UsedPct", 1, 101, 21,
      HistogramBase::kUmaTargetedHistogramFlag);
}

void PersistentMemoryAllocator::Flush(bool sync) {
  FlushPartial(used(), sync);
}

void PersistentMemoryAllocator::SetMemoryState(uint8_t memory_state) {
  shared_meta()->memory_state.store(memory_state, std::memory_order_relaxed);
  FlushPartial(sizeof(SharedMetadata), false);
}

uint8_t PersistentMemoryAllocator::GetMemoryState() const {
  return shared_meta()->memory_state.load(std::memory_order_relaxed);
}

size_t PersistentMemoryAllocator::used() const {
  return std::min(shared_meta()->freeptr.load(std::memory_order_relaxed),
                  mem_size_);
}

PersistentMemoryAllocator::Reference PersistentMemoryAllocator::GetAsReference(
    const void* memory,
    uint32_t type_id) const {
  uintptr_t address = reinterpret_cast<uintptr_t>(memory);
  if (address < reinterpret_cast<uintptr_t>(mem_base_))
    return kReferenceNull;

  uintptr_t offset = address - reinterpret_cast<uintptr_t>(mem_base_);
  if (offset >= mem_size_ || offset < sizeof(BlockHeader))
    return kReferenceNull;

  Reference ref = static_cast<Reference>(offset) - sizeof(BlockHeader);
  if (!GetBlockData(ref, type_id, kSizeAny))
    return kReferenceNull;

  return ref;
}

size_t PersistentMemoryAllocator::GetAllocSize(Reference ref) const {
  const volatile BlockHeader* const block = GetBlock(ref, 0, 0, false, false);
  if (!block)
    return 0;
  uint32_t size = block->size;
  // Header was verified by GetBlock() but a malicious actor could change
  // the value between there and here. Check it again.
  uint32_t total_size;
  if (size <= sizeof(BlockHeader) ||
      !base::CheckAdd(ref, size).AssignIfValid(&total_size) ||
      total_size > mem_size_) {
    SetCorrupt();
    return 0;
  }
  return size - sizeof(BlockHeader);
}

uint32_t PersistentMemoryAllocator::GetType(Reference ref) const {
  const volatile BlockHeader* const block = GetBlock(ref, 0, 0, false, false);
  if (!block)
    return 0;
  return block->type_id.load(std::memory_order_relaxed);
}

bool PersistentMemoryAllocator::ChangeType(Reference ref,
                                           uint32_t to_type_id,
                                           uint32_t from_type_id,
                                           bool clear) {
  DCHECK_NE(access_mode_, kReadOnly);
  volatile BlockHeader* const block = GetBlock(ref, 0, 0, false, false);
  if (!block)
    return false;

  // "Strong" exchanges are used below because there is no loop that can retry
  // in the wake of spurious failures possible with "weak" exchanges. It is,
  // in aggregate, an "acquire-release" operation so no memory accesses can be
  // reordered either before or after this method (since changes based on type
  // could happen on either side).

  if (clear) {
    // If clearing the memory, first change it to the "transitioning" type so
    // there can be no confusion by other threads. After the memory is cleared,
    // it can be changed to its final type.
    if (!block->type_id.compare_exchange_strong(
            from_type_id, kTypeIdTransitioning, std::memory_order_acquire,
            std::memory_order_acquire)) {
      // Existing type wasn't what was expected: fail (with no changes)
      return false;
    }

    // Clear the memory in an atomic manner. Using "release" stores force
    // every write to be done after the ones before it. This is better than
    // using memset because (a) it supports "volatile" and (b) it creates a
    // reliable pattern upon which other threads may rely.
    volatile std::atomic<int>* data =
        reinterpret_cast<volatile std::atomic<int>*>(
            reinterpret_cast<volatile char*>(block) + sizeof(BlockHeader));
    const uint32_t words = (block->size - sizeof(BlockHeader)) / sizeof(int);
    DCHECK_EQ(0U, (block->size - sizeof(BlockHeader)) % sizeof(int));
    for (uint32_t i = 0; i < words; ++i) {
      data->store(0, std::memory_order_release);
      ++data;
    }

    // If the destination type is "transitioning" then skip the final exchange.
    if (to_type_id == kTypeIdTransitioning)
      return true;

    // Finish the change to the desired type.
    from_type_id = kTypeIdTransitioning;  // Exchange needs modifiable original.
    bool success = block->type_id.compare_exchange_strong(
        from_type_id, to_type_id, std::memory_order_release,
        std::memory_order_relaxed);
    DCHECK(success);  // Should never fail.
    return success;
  }

  // One step change to the new type. Will return false if the existing value
  // doesn't match what is expected.
  return block->type_id.compare_exchange_strong(from_type_id, to_type_id,
                                                std::memory_order_acq_rel,
                                                std::memory_order_acquire);
}

PersistentMemoryAllocator::Reference PersistentMemoryAllocator::Allocate(
    size_t req_size,
    uint32_t type_id) {
  return AllocateImpl(req_size, type_id);
}

PersistentMemoryAllocator::Reference PersistentMemoryAllocator::AllocateImpl(
    size_t req_size,
    uint32_t type_id) {
  DCHECK_NE(access_mode_, kReadOnly);

  // Validate req_size to ensure it won't overflow when used as 32-bit value.
  if (req_size > kSegmentMaxSize - sizeof(BlockHeader)) {
    NOTREACHED();
  }

  // Round up the requested size, plus header, to the next allocation alignment.
  size_t size = bits::AlignUp(req_size + sizeof(BlockHeader), kAllocAlignment);
  if (size <= sizeof(BlockHeader) || size > mem_page_) {
    // This shouldn't be reached through normal means.
    debug::DumpWithoutCrashing();
    return kReferenceNull;
  }

  // Get the current start of unallocated memory. Other threads may
  // update this at any time and cause us to retry these operations.
  // This value should be treated as "const" to avoid confusion through
  // the code below but recognize that any failed compare-exchange operation
  // involving it will cause it to be loaded with a more recent value. The
  // code should either exit or restart the loop in that case.
  /* const */ uint32_t freeptr =
      shared_meta()->freeptr.load(std::memory_order_acquire);

  // Allocation is lockless so we do all our caculation and then, if saving
  // indicates a change has occurred since we started, scrap everything and
  // start over.
  for (;;) {
    if (IsCorrupt())
      return kReferenceNull;

    if (freeptr + size > mem_size_) {
      SetFlag(&shared_meta()->flags, kFlagFull);
      return kReferenceNull;
    }

    // Get pointer to the "free" block. If something has been allocated since
    // the load of freeptr above, it is still safe as nothing will be written
    // to that location until after the compare-exchange below.
    volatile BlockHeader* const block = GetBlock(freeptr, 0, 0, false, true);
    if (!block) {
      SetCorrupt();
      return kReferenceNull;
    }

    // An allocation cannot cross page boundaries. If it would, create a
    // "wasted" block and begin again at the top of the next page. This
    // area could just be left empty but we fill in the block header just
    // for completeness sake.
    const uint32_t page_free = mem_page_ - freeptr % mem_page_;
    if (size > page_free) {
      if (page_free <= sizeof(BlockHeader)) {
        SetCorrupt();
        return kReferenceNull;
      }

#if !BUILDFLAG(IS_NACL)
      // In production, with the current state of the code, this code path
      // should not be reached. However, crash reports have been hinting that it
      // is. Add crash keys to investigate this.
      // TODO(crbug.com/40064026): Remove them once done.
      SCOPED_CRASH_KEY_NUMBER("PersistentMemoryAllocator", "mem_size_",
                              mem_size_);
      SCOPED_CRASH_KEY_NUMBER("PersistentMemoryAllocator", "mem_page_",
                              mem_page_);
      SCOPED_CRASH_KEY_NUMBER("PersistentMemoryAllocator", "freeptr", freeptr);
      SCOPED_CRASH_KEY_NUMBER("PersistentMemoryAllocator", "page_free",
                              page_free);
      SCOPED_CRASH_KEY_NUMBER("PersistentMemoryAllocator", "size", size);
      SCOPED_CRASH_KEY_NUMBER("PersistentMemoryAllocator", "req_size",
                              req_size);
      SCOPED_CRASH_KEY_NUMBER("PersistentMemoryAllocator", "type_id", type_id);
      std::string persistent_file_name = "N/A";
      auto* allocator = GlobalHistogramAllocator::Get();
      if (allocator && allocator->HasPersistentLocation()) {
        persistent_file_name =
            allocator->GetPersistentLocation().BaseName().AsUTF8Unsafe();
      }
      SCOPED_CRASH_KEY_STRING256("PersistentMemoryAllocator", "file_name",
                                 persistent_file_name);
      debug::DumpWithoutCrashing();
#endif  // !BUILDFLAG(IS_NACL)

      const uint32_t new_freeptr = freeptr + page_free;
      if (shared_meta()->freeptr.compare_exchange_strong(
              freeptr, new_freeptr, std::memory_order_acq_rel,
              std::memory_order_acquire)) {
        block->size = page_free;
        block->cookie = kBlockCookieWasted;
      }
      continue;
    }

    // Don't leave a slice at the end of a page too small for anything. This
    // can result in an allocation up to two alignment-sizes greater than the
    // minimum required by requested-size + header + alignment.
    if (page_free - size < sizeof(BlockHeader) + kAllocAlignment) {
      size = page_free;
      if (freeptr + size > mem_size_) {
        SetCorrupt();
        return kReferenceNull;
      }
    }

    // This cast is safe because (freeptr + size) <= mem_size_.
    const uint32_t new_freeptr = static_cast<uint32_t>(freeptr + size);

    // Save our work. Try again if another thread has completed an allocation
    // while we were processing. A "weak" exchange would be permissable here
    // because the code will just loop and try again but the above processing
    // is significant so make the extra effort of a "strong" exchange.
    if (!shared_meta()->freeptr.compare_exchange_strong(
            freeptr, new_freeptr, std::memory_order_acq_rel,
            std::memory_order_acquire)) {
      continue;
    }

    // Given that all memory was zeroed before ever being given to an instance
    // of this class and given that we only allocate in a monotomic fashion
    // going forward, it must be that the newly allocated block is completely
    // full of zeros. If we find anything in the block header that is NOT a
    // zero then something must have previously run amuck through memory,
    // writing beyond the allocated space and into unallocated space.
    if (block->size != 0 ||
        block->cookie != kBlockCookieFree ||
        block->type_id.load(std::memory_order_relaxed) != 0 ||
        block->next.load(std::memory_order_relaxed) != 0) {
      SetCorrupt();
      return kReferenceNull;
    }

    // Make sure the memory exists by writing to the first byte of every memory
    // page it touches beyond the one containing the block header itself.
    // As the underlying storage is often memory mapped from disk or shared
    // space, sometimes things go wrong and those address don't actually exist
    // leading to a SIGBUS (or Windows equivalent) at some arbitrary location
    // in the code. This should concentrate all those failures into this
    // location for easy tracking and, eventually, proper handling.
    volatile char* mem_end = reinterpret_cast<volatile char*>(block) + size;
    volatile char* mem_begin = reinterpret_cast<volatile char*>(
        (reinterpret_cast<uintptr_t>(block) + sizeof(BlockHeader) +
         (vm_page_size_ - 1)) &
        ~static_cast<uintptr_t>(vm_page_size_ - 1));
    for (volatile char* memory = mem_begin; memory < mem_end;
         memory += vm_page_size_) {
      // It's required that a memory segment start as all zeros and thus the
      // newly allocated block is all zeros at this point. Thus, writing a
      // zero to it allows testing that the memory exists without actually
      // changing its contents. The compiler doesn't know about the requirement
      // and so cannot optimize-away these writes.
      *memory = 0;
    }

    // Load information into the block header. There is no "release" of the
    // data here because this memory can, currently, be seen only by the thread
    // performing the allocation. When it comes time to share this, the thread
    // will call MakeIterable() which does the release operation.
    // `size` is at most kSegmentMaxSize, so this cast is safe.
    block->size = static_cast<uint32_t>(size);
    block->cookie = kBlockCookieAllocated;
    block->type_id.store(type_id, std::memory_order_relaxed);
    return freeptr;
  }
}

void PersistentMemoryAllocator::GetMemoryInfo(MemoryInfo* meminfo) const {
  uint32_t remaining = std::max(
      mem_size_ - shared_meta()->freeptr.load(std::memory_order_relaxed),
      (uint32_t)sizeof(BlockHeader));
  meminfo->total = mem_size_;
  meminfo->free = remaining - sizeof(BlockHeader);
}

void PersistentMemoryAllocator::MakeIterable(Reference ref) {
  DCHECK_NE(access_mode_, kReadOnly);
  if (IsCorrupt())
    return;
  volatile BlockHeader* block = GetBlock(ref, 0, 0, false, false);
  if (!block)  // invalid reference
    return;

  Reference empty_ref = 0;
  if (!block->next.compare_exchange_strong(
          /*expected=*/empty_ref, /*desired=*/kReferenceQueue,
          /*success=*/std::memory_order_acq_rel,
          /*failure=*/std::memory_order_acquire)) {
    // Already iterable (or another thread is currently making this iterable).
    return;
  }

  // Try to add this block to the tail of the queue. May take multiple tries.
  // If so, tail will be automatically updated with a more recent value during
  // compare-exchange operations.
  uint32_t tail = shared_meta()->tailptr.load(std::memory_order_acquire);
  for (;;) {
    // Acquire the current tail-pointer released by previous call to this
    // method and validate it.
    block = GetBlock(tail, 0, 0, true, false);
    if (!block) {
      SetCorrupt();
      return;
    }

    // Try to insert the block at the tail of the queue. The tail node always
    // has an existing value of kReferenceQueue; if that is somehow not the
    // existing value then another thread has acted in the meantime. A "strong"
    // exchange is necessary so the "else" block does not get executed when
    // that is not actually the case (which can happen with a "weak" exchange).
    uint32_t next = kReferenceQueue;  // Will get replaced with existing value.
    if (block->next.compare_exchange_strong(next, ref,
                                            std::memory_order_acq_rel,
                                            std::memory_order_acquire)) {
      // Update the tail pointer to the new offset. If the "else" clause did
      // not exist, then this could be a simple Release_Store to set the new
      // value but because it does, it's possible that other threads could add
      // one or more nodes at the tail before reaching this point. We don't
      // have to check the return value because it either operates correctly
      // or the exact same operation has already been done (by the "else"
      // clause) on some other thread.
      shared_meta()->tailptr.compare_exchange_strong(tail, ref,
                                                     std::memory_order_release,
                                                     std::memory_order_relaxed);
      return;
    }
    // In the unlikely case that a thread crashed or was killed between the
    // update of "next" and the update of "tailptr", it is necessary to
    // perform the operation that would have been done. There's no explicit
    // check for crash/kill which means that this operation may also happen
    // even when the other thread is in perfect working order which is what
    // necessitates the CompareAndSwap above.
    shared_meta()->tailptr.compare_exchange_strong(
        tail, next, std::memory_order_acq_rel, std::memory_order_acquire);
  }
}

// The "corrupted" state is held both locally and globally (shared). The
// shared flag can't be trusted since a malicious actor could overwrite it.
// Because corruption can be detected during read-only operations such as
// iteration, this method may be called by other "const" methods. In this
// case, it's safe to discard the constness and modify the local flag and
// maybe even the shared flag if the underlying data isn't actually read-only.
void PersistentMemoryAllocator::SetCorrupt(bool allow_write) const {
  if (!corrupt_.load(std::memory_order_relaxed) &&
      !CheckFlag(
          const_cast<volatile std::atomic<uint32_t>*>(&shared_meta()->flags),
          kFlagCorrupt)) {
    LOG(ERROR) << "Corruption detected in shared-memory segment.";
  }

  corrupt_.store(true, std::memory_order_relaxed);
  if (allow_write && access_mode_ != kReadOnly) {
    SetFlag(const_cast<volatile std::atomic<uint32_t>*>(&shared_meta()->flags),
            kFlagCorrupt);
  }
}

bool PersistentMemoryAllocator::IsCorrupt() const {
  if (corrupt_.load(std::memory_order_relaxed)) {
    return true;
  }
  if (CheckFlag(&shared_meta()->flags, kFlagCorrupt)) {
    // Set the local flag if we found the flag in the data.
    SetCorrupt(/*allow_write=*/false);
    return true;
  }
  return false;
}

bool PersistentMemoryAllocator::IsFull() const {
  return CheckFlag(&shared_meta()->flags, kFlagFull);
}

// Dereference a block |ref| and ensure that it's valid for the desired
// |type_id| and |size|. |special| indicates that we may try to access block
// headers not available to callers but still accessed by this module. By
// having internal dereferences go through this same function, the allocator
// is hardened against corruption.
const volatile PersistentMemoryAllocator::BlockHeader*
PersistentMemoryAllocator::GetBlock(Reference ref,
                                    uint32_t type_id,
                                    size_t size,
                                    bool queue_ok,
                                    bool free_ok) const {
  // Handle special cases.
  if (ref == kReferenceQueue && queue_ok)
    return reinterpret_cast<const volatile BlockHeader*>(mem_base_ + ref);

  // Validation of parameters.
  if (ref < sizeof(SharedMetadata))
    return nullptr;
  if (ref % kAllocAlignment != 0)
    return nullptr;
  size += sizeof(BlockHeader);
  uint32_t total_size;
  if (!base::CheckAdd(ref, size).AssignIfValid(&total_size)) {
    return nullptr;
  }
  if (total_size > mem_size_) {
    return nullptr;
  }

  // Validation of referenced block-header.
  if (!free_ok) {
    const volatile BlockHeader* const block =
        reinterpret_cast<volatile BlockHeader*>(mem_base_ + ref);
    if (block->cookie != kBlockCookieAllocated)
      return nullptr;
    if (block->size < size)
      return nullptr;
    uint32_t block_size;
    if (!base::CheckAdd(ref, block->size).AssignIfValid(&block_size)) {
      return nullptr;
    }
    if (block_size > mem_size_) {
      return nullptr;
    }
    if (type_id != 0 &&
        block->type_id.load(std::memory_order_relaxed) != type_id) {
      return nullptr;
    }
  }

  // Return pointer to block data.
  return reinterpret_cast<const volatile BlockHeader*>(mem_base_ + ref);
}

void PersistentMemoryAllocator::FlushPartial(size_t length, bool sync) {
  // Generally there is nothing to do as every write is done through volatile
  // memory with atomic instructions to guarantee consistency. This (virtual)
  // method exists so that derived classes can do special things, such as tell
  // the OS to write changes to disk now rather than when convenient.
}

uint32_t PersistentMemoryAllocator::freeptr() const {
  return shared_meta()->freeptr.load(std::memory_order_relaxed);
}

uint32_t PersistentMemoryAllocator::version() const {
  return shared_meta()->version;
}

const volatile void* PersistentMemoryAllocator::GetBlockData(
    Reference ref,
    uint32_t type_id,
    size_t size) const {
  DCHECK(size > 0);
  const volatile BlockHeader* block =
      GetBlock(ref, type_id, size, false, false);
  if (!block)
    return nullptr;
  return reinterpret_cast<const volatile char*>(block) + sizeof(BlockHeader);
}

void PersistentMemoryAllocator::UpdateTrackingHistograms() {
  DCHECK_NE(access_mode_, kReadOnly);
  if (used_histogram_) {
    MemoryInfo meminfo;
    GetMemoryInfo(&meminfo);
    HistogramBase::Sample used_percent = static_cast<HistogramBase::Sample>(
        ((meminfo.total - meminfo.free) * 100ULL / meminfo.total));
    used_histogram_->Add(used_percent);
  }
}


//----- LocalPersistentMemoryAllocator -----------------------------------------

LocalPersistentMemoryAllocator::LocalPersistentMemoryAllocator(
    size_t size,
    uint64_t id,
    std::string_view name)
    : PersistentMemoryAllocator(AllocateLocalMemory(size, name),
                                size,
                                0,
                                id,
                                name,
                                kReadWrite) {}

LocalPersistentMemoryAllocator::~LocalPersistentMemoryAllocator() {
  DeallocateLocalMemory(const_cast<char*>(mem_base_), mem_size_, mem_type_);
}

// static
PersistentMemoryAllocator::Memory
LocalPersistentMemoryAllocator::AllocateLocalMemory(size_t size,
                                                    std::string_view name) {
  void* address;

#if BUILDFLAG(IS_WIN)
  address =
      ::VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  if (address)
    return Memory(address, MEM_VIRTUAL);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  // MAP_ANON is deprecated on Linux but MAP_ANONYMOUS is not universal on Mac.
  // MAP_SHARED is not available on Linux <2.4 but required on Mac.
  address = ::mmap(nullptr, size, PROT_READ | PROT_WRITE,
                   MAP_ANON | MAP_SHARED, -1, 0);
  if (address != MAP_FAILED) {
#if BUILDFLAG(IS_ANDROID)
    // Allow the anonymous memory region allocated by mmap(MAP_ANON) to be
    // identified in /proc/$PID/smaps.  This helps improve visibility into
    // Chrome's memory usage on Android.
    const std::string arena_name = base::StrCat({"persistent:", name});
    prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, address, size, arena_name.c_str());
#endif
    return Memory(address, MEM_VIRTUAL);
  }
#else
#error This architecture is not (yet) supported.
#endif

  // As a last resort, just allocate the memory from the heap. This will
  // achieve the same basic result but the acquired memory has to be
  // explicitly zeroed and thus realized immediately (i.e. all pages are
  // added to the process now istead of only when first accessed).
  address = malloc(size);
  DPCHECK(address);
  memset(address, 0, size);
  return Memory(address, MEM_MALLOC);
}

// static
void LocalPersistentMemoryAllocator::DeallocateLocalMemory(void* memory,
                                                           size_t size,
                                                           MemoryType type) {
  if (type == MEM_MALLOC) {
    free(memory);
    return;
  }

  DCHECK_EQ(MEM_VIRTUAL, type);
#if BUILDFLAG(IS_WIN)
  BOOL success = ::VirtualFree(memory, 0, MEM_DECOMMIT);
  DCHECK(success);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  int result = ::munmap(memory, size);
  DCHECK_EQ(0, result);
#else
#error This architecture is not (yet) supported.
#endif
}

//----- WritableSharedPersistentMemoryAllocator --------------------------------

WritableSharedPersistentMemoryAllocator::
    WritableSharedPersistentMemoryAllocator(
        base::WritableSharedMemoryMapping memory,
        uint64_t id,
        std::string_view name)
    : PersistentMemoryAllocator(Memory(memory.memory(), MEM_SHARED),
                                memory.size(),
                                0,
                                id,
                                name,
                                kReadWrite),
      shared_memory_(std::move(memory)) {}

WritableSharedPersistentMemoryAllocator::
    ~WritableSharedPersistentMemoryAllocator() = default;

// static
bool WritableSharedPersistentMemoryAllocator::IsSharedMemoryAcceptable(
    const base::WritableSharedMemoryMapping& memory) {
  return IsMemoryAcceptable(memory.memory(), memory.size(), 0, false);
}

//----- ReadOnlySharedPersistentMemoryAllocator --------------------------------

ReadOnlySharedPersistentMemoryAllocator::
    ReadOnlySharedPersistentMemoryAllocator(
        base::ReadOnlySharedMemoryMapping memory,
        uint64_t id,
        std::string_view name)
    : PersistentMemoryAllocator(
          Memory(const_cast<void*>(memory.memory()), MEM_SHARED),
          memory.size(),
          0,
          id,
          name,
          kReadOnly),
      shared_memory_(std::move(memory)) {}

ReadOnlySharedPersistentMemoryAllocator::
    ~ReadOnlySharedPersistentMemoryAllocator() = default;

// static
bool ReadOnlySharedPersistentMemoryAllocator::IsSharedMemoryAcceptable(
    const base::ReadOnlySharedMemoryMapping& memory) {
  return IsMemoryAcceptable(memory.memory(), memory.size(), 0, true);
}

#if !BUILDFLAG(IS_NACL)
//----- FilePersistentMemoryAllocator ------------------------------------------

FilePersistentMemoryAllocator::FilePersistentMemoryAllocator(
    std::unique_ptr<MemoryMappedFile> file,
    size_t max_size,
    uint64_t id,
    std::string_view name,
    AccessMode access_mode)
    : PersistentMemoryAllocator(
          Memory(const_cast<uint8_t*>(file->data()), MEM_FILE),
          max_size != 0 ? max_size : file->length(),
          0,
          id,
          name,
          access_mode),
      mapped_file_(std::move(file)) {}

FilePersistentMemoryAllocator::~FilePersistentMemoryAllocator() = default;

// static
bool FilePersistentMemoryAllocator::IsFileAcceptable(
    const MemoryMappedFile& file,
    bool readonly) {
  return IsMemoryAcceptable(file.data(), file.length(), 0, readonly);
}

void FilePersistentMemoryAllocator::Cache() {
  // Since this method is expected to load data from permanent storage
  // into memory, blocking I/O may occur.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // Calculate begin/end addresses so that the first byte of every page
  // in that range can be read. Keep within the used space. The |volatile|
  // keyword makes it so the compiler can't make assumptions about what is
  // in a given memory location and thus possibly avoid the read.
  const volatile char* mem_end = mem_base_ + used();
  const volatile char* mem_begin = mem_base_;

  // Iterate over the memory a page at a time, reading the first byte of
  // every page. The values are added to a |total| so that the compiler
  // can't omit the read.
  int total = 0;
  for (const volatile char* memory = mem_begin; memory < mem_end;
       memory += vm_page_size_) {
    total += *memory;
  }

  // Tell the compiler that |total| is used so that it can't optimize away
  // the memory accesses above.
  debug::Alias(&total);
}

void FilePersistentMemoryAllocator::FlushPartial(size_t length, bool sync) {
  if (IsReadonly())
    return;

  std::optional<base::ScopedBlockingCall> scoped_blocking_call;
  if (sync)
    scoped_blocking_call.emplace(FROM_HERE, base::BlockingType::MAY_BLOCK);

#if BUILDFLAG(IS_WIN)
  // Windows doesn't support asynchronous flush.
  scoped_blocking_call.emplace(FROM_HERE, base::BlockingType::MAY_BLOCK);
  BOOL success = ::FlushViewOfFile(data(), length);
  DPCHECK(success);
#elif BUILDFLAG(IS_APPLE)
  // On OSX, "invalidate" removes all cached pages, forcing a re-read from
  // disk. That's not applicable to "flush" so omit it.
  int result =
      ::msync(const_cast<void*>(data()), length, sync ? MS_SYNC : MS_ASYNC);
  DCHECK_NE(EINVAL, result);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  // On POSIX, "invalidate" forces _other_ processes to recognize what has
  // been written to disk and so is applicable to "flush".
  int result = ::msync(const_cast<void*>(data()), length,
                       MS_INVALIDATE | (sync ? MS_SYNC : MS_ASYNC));
  DCHECK_NE(EINVAL, result);
#else
#error Unsupported OS.
#endif
}
#endif  // !BUILDFLAG(IS_NACL)

//----- DelayedPersistentAllocation --------------------------------------------

DelayedPersistentAllocation::DelayedPersistentAllocation(
    PersistentMemoryAllocator* allocator,
    std::atomic<Reference>* ref,
    uint32_t type,
    size_t size,
    size_t offset)
    : allocator_(allocator),
      type_(type),
      size_(checked_cast<uint32_t>(size)),
      offset_(checked_cast<uint32_t>(offset)),
      reference_(ref) {
  DCHECK(allocator_);
  DCHECK_NE(0U, type_);
  DCHECK_LT(0U, size_);
  DCHECK(reference_);
}

DelayedPersistentAllocation::~DelayedPersistentAllocation() = default;

span<uint8_t> DelayedPersistentAllocation::GetUntyped() const {
  // Relaxed operations are acceptable here because it's not protecting the
  // contents of the allocation in any way.
  Reference ref = reference_->load(std::memory_order_acquire);

#if !BUILDFLAG(IS_NACL)
  // TODO(crbug.com/40064026): Remove these. They are used to investigate
  // unexpected failures.
  bool ref_found = (ref != 0);
  bool raced = false;
#endif  // !BUILDFLAG(IS_NACL)

  if (!ref) {
    ref = allocator_->Allocate(size_, type_);
    if (!ref) {
      return span<uint8_t>();
    }

    // Store the new reference in its proper location using compare-and-swap.
    // Use a "strong" exchange to ensure no false-negatives since the operation
    // cannot be retried.
    Reference existing = 0;  // Must be mutable; receives actual value.
    if (!reference_->compare_exchange_strong(existing, ref,
                                             std::memory_order_release,
                                             std::memory_order_relaxed)) {
      // Failure indicates that something else has raced ahead, performed the
      // allocation, and stored its reference. Purge the allocation that was
      // just done and use the other one instead.
      DCHECK_EQ(type_, allocator_->GetType(existing));
      DCHECK_LE(size_, allocator_->GetAllocSize(existing));
      allocator_->ChangeType(ref, 0, type_, /*clear=*/false);
      ref = existing;
#if !BUILDFLAG(IS_NACL)
      raced = true;
#endif  // !BUILDFLAG(IS_NACL)
    }
  }

  uint8_t* mem = allocator_->GetAsArray<uint8_t>(ref, type_, size_);
  if (!mem) {
#if !BUILDFLAG(IS_NACL)
    // TODO(crbug.com/40064026): Remove these. They are used to investigate
    // unexpected failures.
    SCOPED_CRASH_KEY_BOOL("PersistentMemoryAllocator", "full",
                          allocator_->IsFull());
    SCOPED_CRASH_KEY_BOOL("PersistentMemoryAllocator", "corrupted",
                          allocator_->IsCorrupt());
    SCOPED_CRASH_KEY_NUMBER("PersistentMemoryAllocator", "freeptr",
                            allocator_->freeptr());
    // The allocator's cookie should always be `kGlobalCookie`. Add it to crash
    // keys to see if the file was corrupted externally, e.g. by a file
    // shredder. Cast to volatile to avoid compiler optimizations and ensure
    // that the actual value is read.
    SCOPED_CRASH_KEY_NUMBER(
        "PersistentMemoryAllocator", "cookie",
        static_cast<volatile PersistentMemoryAllocator::SharedMetadata*>(
            allocator_->shared_meta())
            ->cookie);
    SCOPED_CRASH_KEY_NUMBER("PersistentMemoryAllocator", "ref", ref);
    SCOPED_CRASH_KEY_BOOL("PersistentMemoryAllocator", "ref_found", ref_found);
    SCOPED_CRASH_KEY_BOOL("PersistentMemoryAllocator", "raced", raced);
    SCOPED_CRASH_KEY_NUMBER("PersistentMemoryAllocator", "type_", type_);
    SCOPED_CRASH_KEY_NUMBER("PersistentMemoryAllocator", "size_", size_);
    if (ref == 0xC8799269) {
      // There are many crash reports containing the corrupted "0xC8799269"
      // value in |ref|. This value is actually a "magic" number to indicate
      // that a certain block in persistent memory was successfully allocated,
      // so it should not appear there. Include some extra crash keys to see if
      // the surrounding values were also corrupted. If so, the value before
      // would be the size of the allocated object, and the value after would be
      // the type id of the allocated object. If they are not corrupted, these
      // would contain |ranges_checksum| and the start of |samples_metadata|
      // respectively (see PersistentHistogramData struct). We do some pointer
      // arithmetic here -- it should theoretically be safe, unless something
      // went terribly wrong...
      SCOPED_CRASH_KEY_NUMBER(
          "PersistentMemoryAllocator", "ref_before",
          (reference_ - 1)->load(std::memory_order_relaxed));
      SCOPED_CRASH_KEY_NUMBER(
          "PersistentMemoryAllocator", "ref_after",
          (reference_ + 1)->load(std::memory_order_relaxed));
      DUMP_WILL_BE_NOTREACHED();
      return span<uint8_t>();
    }
#endif  // !BUILDFLAG(IS_NACL)
    // This should never happen but be tolerant if it does as corruption from
    // the outside is something to guard against.
    DUMP_WILL_BE_NOTREACHED();
    return span<uint8_t>();
  }
  return make_span(mem + offset_, size_ - offset_);
}

}  // namespace base
