// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr_asan_event.h"

#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)

#include <sanitizer/allocator_interface.h>
#include <sanitizer/asan_interface.h>

#include <sstream>

#include "base/compiler_specific.h"
#include "base/debug/asan_service.h"
#include "base/memory/raw_ptr_asan_allocator.h"
#include "base/memory/raw_ptr_asan_service.h"

namespace base::internal {

namespace {

template <typename T>
struct InternalDeleter {
  constexpr InternalDeleter() = default;

  void operator()(T* ptr) const { RawPtrAsanAllocator<T>().deallocate(ptr, 1); }
};

}  // namespace

void RawPtrAsanEvent::PrintEventStack() const {
  using BufferType = std::array<char, 4096>;
  std::unique_ptr<BufferType, InternalDeleter<BufferType>> buffer(
      RawPtrAsanAllocator<BufferType>().allocate(1));
  size_t frame_index = 0;

  for (size_t i = 0; i < stack.size(); ++i) {
    if (!stack[i]) {
      break;
    }
    void* frame_ptr = const_cast<void*>(stack[i]);
    __sanitizer_symbolize_pc(frame_ptr, "%p %F %L", buffer->data(),
                             buffer->size());
    for (const char* ptr = buffer->data(); *ptr != '\0';
         UNSAFE_BUFFERS(ptr += strlen(ptr))) {
      debug::AsanService::GetInstance()->Log("    #%i %s", frame_index++, ptr);
    }
  }

  debug::AsanService::GetInstance()->Log("");
}

void RawPtrAsanEvent::PrintEvent(bool print_stack = false) const {
  const char* type_string = "quarantine-entry";
  if (type == RawPtrAsanEvent::Type::kQuarantineAssignment) {
    type_string = "quarantine-assignment";
  } else if (type == RawPtrAsanEvent::Type::kQuarantineRead) {
    type_string = "quarantine-read";
  } else if (type == RawPtrAsanEvent::Type::kQuarantineWrite) {
    type_string = "quarantine-write";
  } else if (type == RawPtrAsanEvent::Type::kQuarantineExit) {
    type_string = "quarantine-exit";
  } else if (type == RawPtrAsanEvent::Type::kFreeAssignment) {
    type_string = "free-assignment";
  }

  std::stringstream ss_for_thread_id;
  ss_for_thread_id << thread_id;
  debug::AsanService::GetInstance()->Log("[0x%zx:%zu] (%s) %s", address, size,
                                         ss_for_thread_id.str().c_str(),
                                         type_string);

  if (print_stack) {
    PrintEventStack();
  }
}

RawPtrAsanEventLog::RawPtrAsanEventLog() = default;

RawPtrAsanEventLog::~RawPtrAsanEventLog() = default;

NO_SANITIZE("address")
void RawPtrAsanEventLog::Add(RawPtrAsanEvent&& event) {
  using Type = RawPtrAsanEvent::Type;
  internal::PartitionAutoLock event_lock(GetLock());

  // Check if we have previous logged accesses to this allocation; this will
  // tell us if we need to store it, and whether we need to synthesize the
  // corresponding kQuarantineEntry event.
  bool has_entry = false;
  for (auto iter = events_.rbegin(); iter != events_.rend(); ++iter) {
    if (event.IsSameAllocation(*iter)) {
      // If the last matching event that we find is a quarantine-exit event,
      // then this event refers to a reused allocation that doesn't have any
      // logged events.
      if (iter->type != Type::kQuarantineExit) {
        has_entry = true;
      }
      break;
    }
  }

  if (event.type == Type::kQuarantineEntry) {
    // We don't store kQuarantineEntry events directly, since we can synthesize
    // them later when they are needed.
    return;
  }
  if (event.type == Type::kQuarantineExit) {
    // kQuarantineExit without any other accesses is uninteresting, we can just
    // discard this event as long as the threads match.
    RawPtrAsanThreadId free_thread_id =
        RawPtrAsanService::GetInstance().GetFreeThreadIdOfAllocation(
            event.address);
    if (!has_entry && event.thread_id == free_thread_id) {
      return;
    }
  }

  if (!has_entry && (event.type == Type::kQuarantineAssignment ||
                     event.type == Type::kQuarantineRead ||
                     event.type == Type::kQuarantineWrite ||
                     event.type == Type::kQuarantineExit)) {
    // If we reach here, then this allocation in quarantine just became
    // interesting, so we should synthesize a kQuarantineEntry event for it.
    RawPtrAsanThreadId free_thread_id =
        RawPtrAsanService::GetInstance().GetFreeThreadIdOfAllocation(
            event.address);
    uintptr_t allocation_start_address =
        RawPtrAsanService::GetInstance().GetAllocationStart(event.address);
    RawPtrAsanEvent entry_event;
    entry_event.type = Type::kQuarantineEntry;
    entry_event.thread_id = free_thread_id;
    entry_event.address = allocation_start_address;
    if (allocation_start_address) {
      void* allocation_start_ptr =
          reinterpret_cast<void*>(allocation_start_address);
      entry_event.size = __sanitizer_get_allocated_size(allocation_start_ptr);
      __asan_get_alloc_stack(allocation_start_ptr,
                             const_cast<void**>(entry_event.stack.data()),
                             entry_event.stack.size(), nullptr);
    } else {
      // `ptr` does not point to ASAN allocated memory.
      entry_event.size = 0;
    }
    events_.emplace_back(std::move(entry_event));
  }

  events_.emplace_back(std::move(event));
}

void RawPtrAsanEventLog::Print(bool print_stack = false) {
  internal::PartitionAutoLock lock(GetLock());
  for (const auto& event : events_) {
    event.PrintEvent(print_stack);
  }
}

}  // namespace base::internal

#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)
