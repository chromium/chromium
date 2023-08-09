// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"

#include <fcntl.h>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"

#if !BUILDFLAG(IS_NACL)
#include "third_party/boringssl/src/include/openssl/crypto.h"
#include "third_party/boringssl/src/include/openssl/rand.h"
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

}  // namespace

namespace internal {

double RandDoubleAvoidAllocation() {
  uint64_t number;
  // Reading from /dev/urandom is the most portable way to avoid allocation.
  CHECK(ReadFromFD(GetUrandomFD(), reinterpret_cast<char*>(&number),
                   sizeof(number)));
  // This transformation is explained in rand_util.cc.
  return (number >> 11) * 0x1.0p-53;
}

}  // namespace internal

void RandBytes(void* output, size_t output_length) {
#if !BUILDFLAG(IS_NACL)
  // Ensure BoringSSL is initialized so it can use things like RDRAND.
  CRYPTO_library_init();
  // BoringSSL's RAND_bytes always returns 1. Any error aborts the program.
  (void)RAND_bytes(static_cast<uint8_t*>(output), output_length);
#endif
}

int GetUrandomFD() {
  static const int fd = OpenUrandomFd();
  return fd;
}

}  // namespace base
