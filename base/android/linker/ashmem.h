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

#ifndef BASE_ANDROID_LINKER_ASHMEM_H_
#define BASE_ANDROID_LINKER_ASHMEM_H_

#include <cstddef>

// Management of shared memory regions changed across OS releases. These minimal
// low level wrappers abstract away those release-specific differences.

// On new OS releases returns false.
//
// On old systems returns true when the "/dev/ashmem" device is present on the
// system and looks 'functional'.
//
// Without /dev/ashmem the functions below will continue working, but may
// use different mechanisms and system library calls under the scenes.
//
// The functions can be disabled system-wide without removing /dev/ashmem. This
// should be rare, and the rest of the functions declared below will behave just
// like /dev/ashmem does not exist.
int AshmemDeviceIsSupported();

// Implements ASharedMemory_create(), as described in NDK docs (API level 26).
int SharedMemoryRegionCreate(const char* name, size_t size);

// Implements ASharedMemory_setProt(), as described in NDK docs (API level 26).
int SharedMemoryRegionSetProtectionFlags(int fd, int prot);

// Returns the memory protection flags for the region (PROT_READ, PROT_WRITE or
// both).
int SharedMemoryRegionGetProtectionFlags(int fd);

// Pins the region to memory on old systems.
//
// Behaves as no-op when the region is not served by the ashmem device
// (irrelevant FDs).
//
// Note: Even if AshmemDeviceIsSupported() is true, pinning may still be
// disabled.
// Note: This functionality was never officially provided by Android NDK, and
// the underlying mechanisms are being removed.
int AshmemPinRegion(int fd, size_t offset, size_t len);

// Unpins the region from memory on old systems, allowing the OS to free it
// without asking the userspace.
//
// Just like the AshmemPinRegion() above, behaves as no-op when the OS support
// is removed. Also it is a no-op for irrelevant FDs.
int AshmemUnpinRegion(int fd, size_t offset, size_t len);

#endif  // BASE_ANDROID_LINKER_ASHMEM_H_
