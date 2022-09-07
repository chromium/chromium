// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/thread_instruction_count.h"

#include "base/check_op.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/threading/thread_local_storage.h"

#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace base {
namespace android {

namespace {

constexpr int kPerfFdOpenFailed = -1;

ThreadLocalStorage::Slot& InstructionCounterFdSlot() {
  static NoDestructor<ThreadLocalStorage::Slot> fd_slot([](void* fd_ptr) {
    int fd = checked_cast<int>(reinterpret_cast<intptr_t>(fd_ptr));
    if (fd > 0)
      close(fd);
  });
  return *fd_slot;
}

// Opens a new file descriptor that emits the value of
// PERF_COUNT_HW_INSTRUCTIONS in userspace (excluding kernel and hypervisor
// instructions) for the given |thread_id|, or 0 for the calling thread.
//
// Returns kPerfFdOpenFailed if opening the file descriptor failed.
int OpenInstructionCounterFdForThread(int thread_id) {
  struct perf_event_attr pe = {0};
  pe.type = PERF_TYPE_HARDWARE;
  pe.size = sizeof(struct perf_event_attr);
  pe.config = PERF_COUNT_HW_INSTRUCTIONS;
  pe.exclude_kernel = 1;
  pe.exclude_hv = 1;

  long fd = syscall(__NR_perf_event_open, &pe, thread_id, /* cpu */ -1,
                    /* group_fd */ -1, /* flags */ 0);
  if (fd < 0) {
    PLOG(ERROR) << "perf_event_open: omitting instruction counters";
    return kPerfFdOpenFailed;
  }
  return checked_cast<int>(fd);
}

// Retrieves the active perf counter FD for the current thread, performing
// lazy-initialization if necessary.
int InstructionCounterFdForCurrentThread() {
  auto& slot = InstructionCounterFdSlot();
  int fd = checked_cast<int>(reinterpret_cast<intptr_t>(slot.Get()));
  if (fd == 0) {
    fd = OpenInstructionCounterFdForThread(0);
    slot.Set(reinterpret_cast<void*>(fd));
  }
  return fd;
}

}  // namespace

// static
bool ThreadInstructionCount::IsSupported() {
  return InstructionCounterFdForCurrentThread() > 0;
}

// static
ThreadInstructionCount ThreadInstructionCount::Now() {
  DCHECK(IsSupported());
  int fd = InstructionCounterFdForCurrentThread();
  if (fd <= 0)
    return ThreadInstructionCount();

  uint64_t instructions = 0;
  ssize_t bytes_read = read(fd, &instructions, sizeof(instructions));
  CHECK_EQ(bytes_read, static_cast<ssize_t>(sizeof(instructions)))
      << "Short reads of small size from kernel memory is not expected. If "
         "this fails, use HANDLE_EINTR.";
  return ThreadInstructionCount(instructions);
}

}  // namespace android
}  // namespace base
