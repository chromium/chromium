// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_ANDROID_INFO_H_
#define BASE_ANDROID_ANDROID_INFO_H_
namespace base::android::android_info {

const char* device();

const char* manufacturer();

const char* model();

const char* brand();

const char* android_build_id();

const char* build_type();

const char* board();

const char* android_build_fp();

int sdk_int();

bool is_debug_android();

const char* version_incremental();

const char* hardware();

bool is_at_least_u();

const char* codename();

// Available only on android S+. For S-, this method returns empty string.
const char* soc_manufacturer();

bool is_at_least_t();

const char* abi_name();

}  // namespace base::android::android_info

#endif  // BASE_ANDROID_ANDROID_INFO_H_
