// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"

#include <fcntl.h>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/posix/eintr_wrapper.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "third_party/boringssl/src/include/openssl/rand.h"

#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/40641285): Waiting for this header to appear in the iOS SDK.
// (See below.)
#include <sys/random.h>
#endif

namespace base {

namespace {

#if BUILDFLAG(IS_AIX)
// AIX has no 64-bit support for O_CLOEXEC.
static constexpr int kOpenFlags = O_RDONLY;
#else
static constexpr int kOpenFlags = O_RDONLY | O_CLOEXEC;
#endif

int OpenUrandomFd() {
  int ret = HANDLE_EINTR(open("/dev/urandom", kOpenFlags));
  CHECK(ret >= 0) << "Cannot open /dev/urandom";
  return ret;
}

double RandomUint64ToRandomDouble(uint64_t value) {
  // This transformation is explained in rand_util.cc.
  return (value >> 11) * 0x1.0p-53;
}

}  // namespace

namespace internal {

double RandDoubleAvoidAllocation() {
  uint64_t number;
  // TODO(crbug.com/40641285): For Linux and Android use getrandom(2)
  // after Cronet deprecates Android N. Android O would guarantee
  // kernel version >= 3.18, which is sufficient for getrandom().
#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/40641285): Enable this on iOS too, when sys/random.h arrives
  // in its SDK.
  if (getentropy(&number, sizeof(number)) == 0) {
    return RandomUint64ToRandomDouble(number);
  }
#endif
  // Reading from /dev/urandom is the most portable way to avoid allocation.
  CHECK(ReadFromFD(GetUrandomFD(), as_writable_chars(span_from_ref(number))));
  return RandomUint64ToRandomDouble(number);
}

}  // namespace internal

void RandBytes(span<uint8_t> output) {
  // BoringSSL's RAND_bytes always returns 1. Any error aborts the program.
  (void)RAND_bytes(output.data(), output.size());
}

int GetUrandomFD() {
  static const int fd = OpenUrandomFd();
  return fd;
}

}  // namespace base
