// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/starscan/write_protector.h"

#include <mutex>
#include <thread>

#include "base/allocator/partition_allocator/address_pool_manager.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"

#if defined(PA_STARSCAN_UFFD_WRITE_PROTECTOR_SUPPORTED)
#include <fcntl.h>
#include <linux/userfaultfd.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#endif  // defined(PA_STARSCAN_UFFD_WRITE_PROTECTOR_SUPPORTED)

namespace base {
namespace internal {

PCScan::ClearType NoWriteProtector::SupportedClearType() const {
  return PCScan::ClearType::kLazy;
}

#if defined(PA_STARSCAN_UFFD_WRITE_PROTECTOR_SUPPORTED)
namespace {
void UserFaultFDThread(int uffd) {
  PA_DCHECK(-1 != uffd);

  static constexpr char kThreadName[] = "PCScanPFHandler";
  base::PlatformThread::SetName(kThreadName);

  while (true) {
    // Pool on the uffd descriptor for page fault events.
    pollfd pollfd{.fd = uffd, .events = POLLIN};
    const int nready = HANDLE_EINTR(poll(&pollfd, 1, -1));
    PA_CHECK(-1 != nready);

    // Get page fault info.
    uffd_msg msg;
    const int nread = HANDLE_EINTR(read(uffd, &msg, sizeof(msg)));
    PA_CHECK(0 != nread);

    // We only expect page faults.
    PA_DCHECK(UFFD_EVENT_PAGEFAULT == msg.event);
    // We have subscribed only to wp-fault events.
    PA_DCHECK(UFFD_PAGEFAULT_FLAG_WP & msg.arg.pagefault.flags);

    // Enter the safepoint. Concurrent faulted writes will wait until safepoint
    // finishes.
    PCScan::JoinScanIfNeeded();
  }
}
}  // namespace

UserFaultFDWriteProtector::UserFaultFDWriteProtector()
    : uffd_(syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK)) {
  if (uffd_ == -1) {
    LOG(WARNING) << "userfaultfd is not supported by the current kernel";
    return;
  }

  PA_PCHECK(-1 != uffd_);

  uffdio_api uffdio_api;
  uffdio_api.api = UFFD_API;
  uffdio_api.features = 0;
  PA_CHECK(-1 != ioctl(uffd_, UFFDIO_API, &uffdio_api));
  PA_CHECK(UFFD_API == uffdio_api.api);

  // Register the giga-cage to listen uffd events.
  struct uffdio_register uffdio_register;
  uffdio_register.range.start = PartitionAddressSpace::BRPPoolBase();
  uffdio_register.range.len = kPoolMaxSize;
  uffdio_register.mode = UFFDIO_REGISTER_MODE_WP;
  PA_CHECK(-1 != ioctl(uffd_, UFFDIO_REGISTER, &uffdio_register));

  // Start uffd thread.
  std::thread(UserFaultFDThread, uffd_).detach();
}

namespace {
enum class UserFaultFDWPMode {
  kProtect,
  kUnprotect,
};

void UserFaultFDWPSet(int uffd,
                      uintptr_t begin,
                      size_t length,
                      UserFaultFDWPMode mode) {
  PA_DCHECK(0 == (begin % SystemPageSize()));
  PA_DCHECK(0 == (length % SystemPageSize()));

  uffdio_writeprotect wp;
  wp.range.start = begin;
  wp.range.len = length;
  wp.mode =
      (mode == UserFaultFDWPMode::kProtect) ? UFFDIO_WRITEPROTECT_MODE_WP : 0;
  PA_PCHECK(-1 != ioctl(uffd, UFFDIO_WRITEPROTECT, &wp));
}
}  // namespace

void UserFaultFDWriteProtector::ProtectPages(uintptr_t begin, size_t length) {
  if (IsSupported())
    UserFaultFDWPSet(uffd_, begin, length, UserFaultFDWPMode::kProtect);
}

void UserFaultFDWriteProtector::UnprotectPages(uintptr_t begin, size_t length) {
  if (IsSupported())
    UserFaultFDWPSet(uffd_, begin, length, UserFaultFDWPMode::kUnprotect);
}

PCScan::ClearType UserFaultFDWriteProtector::SupportedClearType() const {
  return IsSupported() ? PCScan::ClearType::kEager : PCScan::ClearType::kLazy;
}

bool UserFaultFDWriteProtector::IsSupported() const {
  return uffd_ != -1;
}

#endif  // defined(PA_STARSCAN_UFFD_WRITE_PROTECTOR_SUPPORTED)

}  // namespace internal
}  // namespace base
