/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// This file is partially taken from AOSP and keeps its license.

#include "base/android/linker/ashmem.h"

#include <android/log.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <linux/ashmem.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/system_properties.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#define ASHMEM_DEVICE "/dev/ashmem"
#define LOG_E(...) \
  ((void)__android_log_print(ANDROID_LOG_ERROR, "chromium-ashmem", __VA_ARGS__))

namespace {

/* Technical note regarding reading system properties.
 *
 * Try to use the new __system_property_read_callback API that appeared in
 * Android O / API level 26 when available. Otherwise use the deprecated
 * __system_property_get function.
 *
 * For more technical details from an NDK maintainer, see:
 * https://bugs.chromium.org/p/chromium/issues/detail?id=392191#c17
 */

/* Callback used with __system_property_read_callback. */
void ReadIntProperty(void* cookie,
                     const char* name,
                     const char* value,
                     uint32_t serial) {
  *reinterpret_cast<int*>(cookie) = atoi(value);
  (void)name;
  (void)serial;
}

int SystemPropertyGetInt(const char* name) {
  int result = 0;
  if (__builtin_available(android 26, *)) {
    const prop_info* info = __system_property_find(name);
    if (info) {
      __system_property_read_callback(info, &ReadIntProperty, &result);
    }
  } else {
    char value[PROP_VALUE_MAX] = {};
    if (__system_property_get(name, value) >= 1) {
      result = atoi(value);
    }
  }
  return result;
}

int VendorApiLevel() {
  static int v_api_level = -1;
  if (v_api_level < 0) {
    v_api_level = SystemPropertyGetInt("ro.vendor.api_level");
  }
  return v_api_level;
}

enum AshmemStatus {
  ASHMEM_STATUS_INIT,
  ASHMEM_STATUS_NOT_SUPPORTED,
  ASHMEM_STATUS_SUPPORTED,
};

AshmemStatus s_ashmem_status = ASHMEM_STATUS_INIT;
dev_t s_ashmem_dev;

/* Return the dev_t of a given file path, or 0 if not available, */
dev_t FindAshmemDevice(const char* path) {
  struct stat st;
  dev_t result = 0;
  if (stat(path, &st) == 0 && S_ISCHR(st.st_mode)) {
    result = st.st_dev;
  }
  return result;
}

AshmemStatus GetAshmemSupportStatus() {
  /* NOTE: No need to make this thread-safe, assuming that
   * all threads will find the same value. */
  if (s_ashmem_status != ASHMEM_STATUS_INIT) {
    return s_ashmem_status;
  }

  s_ashmem_dev = FindAshmemDevice(ASHMEM_DEVICE);
  s_ashmem_status = (s_ashmem_dev == 0) ? ASHMEM_STATUS_NOT_SUPPORTED
                                        : ASHMEM_STATUS_SUPPORTED;
  return s_ashmem_status;
}

/* Returns true iff the ashmem device ioctl should be used for a given fd.
 * NOTE: Try not to use fstat() when possible to avoid performance issues. */
bool IsAshmemFd(int fd) {
  if (GetAshmemSupportStatus() == ASHMEM_STATUS_SUPPORTED) {
    struct stat st;
    return (fstat(fd, &st) == 0 && S_ISCHR(st.st_mode) && st.st_dev != 0 &&
            st.st_dev == s_ashmem_dev);
  }
  return false;
}

// Starting with API level 26, the following functions from
// libandroid.so should be used to create shared memory regions,
// unless the device's vendor.api_level is 202604 (Android 17)
// or newer, in which case, use memfd directly instead of
// the ASharedMemory API.
using ASharedMemory_createFunc = int (*)(const char*, size_t);
// ASharedMemory_setProtFunc() is typically invoked in conjunction with
// ASharedMemory_createFunc(), so it's okay for setProt to implicitly assume
// the type of fd it needs to work with.
using ASharedMemory_setProtFunc = int (*)(int fd, int prot);

// Function pointers to shared memory functions.
struct ASharedMemoryFuncs {
  ASharedMemory_createFunc create;
  ASharedMemory_setProtFunc setProt;
};

ASharedMemoryFuncs s_ashmem_funcs = {};
pthread_once_t s_ashmem_funcs_once = PTHREAD_ONCE_INIT;

int MemfdCreateRegion(const char* name, size_t size) {
  int fd = syscall(__NR_memfd_create, name, MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (fd < 0) {
    LOG_E("memfd_create(%s, %zd) failed: %m", name, size);
    return fd;
  }

  int ret = ftruncate(fd, size);
  if (ret < 0) {
    LOG_E("ftruncate(%s, %zd) failed: %m", name, size);
    goto error;
  }

  ret = fcntl(fd, F_ADD_SEALS, F_SEAL_GROW | F_SEAL_SHRINK);
  if (ret < 0) {
    LOG_E("memfd_create(%s, %zd) fcntl(F_ADD_SEALS) failed: %m", name, size);
    goto error;
  }

  return fd;

error:
  close(fd);
  return ret;
}

int memfd_set_prot_region(int fd, int prot) {
  int seals = fcntl(fd, F_GET_SEALS);
  if (seals == -1) {
    LOG_E("memfd_set_prot_region(%d, %d): F_GET_SEALS failed: %m", fd, prot);
    return -1;
  }

  if (prot & PROT_WRITE) {
    /*
     * Now we want the buffer to be read-write, let's check if the buffer
     * has been previously marked as read-only before, if so return error
     */
    if (seals & F_SEAL_FUTURE_WRITE) {
      LOG_E("memfd_set_prot_region(%d, %d): region is write protected", fd,
            prot);
      // Inline with ashmem error code, if already in read-only mode.
      errno = EINVAL;
      return -1;
    }

    return 0;
  }

  // We would only allow read-only for any future file operations
  if (fcntl(fd, F_ADD_SEALS, F_SEAL_FUTURE_WRITE) == -1) {
    LOG_E("memfd_set_prot_region(%d, %d): F_SEAL_FUTURE_WRITE seal failed: %m",
          fd, prot);
    return -1;
  }

  return 0;
}

int MemfdGetProtRegion(int fd) {
  int prot = PROT_READ;
  int seals = fcntl(fd, F_GET_SEALS);
  if (seals == -1) {
    LOG_E("MemfdGetProtRegion(%d): F_GET_SEALS failed: %m", fd);
  } else if (!(seals & (F_SEAL_FUTURE_WRITE | F_SEAL_WRITE))) {
    prot |= PROT_WRITE;
  }
  return prot;
}

void InitAshmemFuncs() {
  ASharedMemoryFuncs* funcs = &s_ashmem_funcs;
  /*
   * When a device conforms to the VSR for API level 202604 (Android 17),
   * ASharedMemory will allocate memfds and attempt to relabel them by using
   * fsetxattr() to workaround how SELinux handles memfds.
   *
   * fsetxattr() is not allowlisted in our seccomp filter, and allowlisting
   * it may be unsafe. Since memfds from Chromium should be accessible with
   * the existing sepolicy for appdomain_tmpfs files, just allocate memfds
   * directly if the device conforms to the VSR for API level 202604.
   */
  if (VendorApiLevel() >= 202604) {
    funcs->create = &MemfdCreateRegion;
    funcs->setProt = &memfd_set_prot_region;
  } else {
    /* Leaked intentionally! */
    void* lib = dlopen("libandroid.so", RTLD_NOW);
    funcs->create = reinterpret_cast<ASharedMemory_createFunc>(
        dlsym(lib, "ASharedMemory_create"));
    funcs->setProt = reinterpret_cast<ASharedMemory_setProtFunc>(
        dlsym(lib, "ASharedMemory_setProt"));
  }
}

const ASharedMemoryFuncs* GetAshmemFuncs() {
  pthread_once(&s_ashmem_funcs_once, &InitAshmemFuncs);
  return &s_ashmem_funcs;
}

bool IsMemfdFd(int fd) {
  if (fcntl(fd, F_GET_SEALS, 0) == -1) {
    return false;
  }
  return true;
}

}  // namespace

int SharedMemoryRegionCreate(const char* name, size_t size) {
  return GetAshmemFuncs()->create(name, size);
}

int SharedMemoryRegionSetProtectionFlags(int fd, int prot) {
  return GetAshmemFuncs()->setProt(fd, prot);
}

int SharedMemoryRegionGetProtectionFlags(int fd) {
  if (IsMemfdFd(fd)) {
    return MemfdGetProtRegion(fd);
  }

  if (IsAshmemFd(fd)) {
    return ioctl(fd, ASHMEM_GET_PROT_MASK);
  }

  return -1;
}

int AshmemPinRegion(int fd, size_t offset, size_t len) {
  if (IsAshmemFd(fd)) {
    struct ashmem_pin pin = {static_cast<__u32>(offset),
                             static_cast<__u32>(len)};
    return ioctl(fd, ASHMEM_PIN, &pin);
  }
  return ASHMEM_NOT_PURGED;
}

int AshmemUnpinRegion(int fd, size_t offset, size_t len) {
  if (IsAshmemFd(fd)) {
    struct ashmem_pin pin = {static_cast<__u32>(offset),
                             static_cast<__u32>(len)};
    return ioctl(fd, ASHMEM_UNPIN, &pin);
  }
  /* NOTE: It is not possible to use madvise() here because it requires a
   * memory address. This could be done in the caller though, instead of
   * this function. */
  return 0;
}

int AshmemDeviceIsSupported() {
  return GetAshmemSupportStatus() == ASHMEM_STATUS_SUPPORTED;
}
