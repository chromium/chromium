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

/* Returns true if the ashmem device is supported on this device.
 * Note that even if the device is not supported,
 * ashmem_{create,set_prot,get_prot,get_size}_region() will still work
 * because they will use the ASharedMemory functions from libandroid.so
 * instead. But ashmem_{pin,unpin}_region() will be no-ops. Starting with API
 * level 26, memfd regions are used under the scenes, also working as no-op
 * for pin/unpin.
 */
int ashmem_device_is_supported();

int ashmem_create_region(const char *name, size_t size);
int ashmem_set_prot_region(int fd, int prot);
int ashmem_get_prot_region(int fd);
int ashmem_pin_region(int fd, size_t offset, size_t len);
int ashmem_unpin_region(int fd, size_t offset, size_t len);
int ashmem_get_size_region(int fd);

#endif  // BASE_ANDROID_LINKER_ASHMEM_H_
