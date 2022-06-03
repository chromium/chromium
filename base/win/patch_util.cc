// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/patch_util.h"

#include "base/notreached.h"

namespace base {
namespace win {
namespace internal {

DWORD ModifyCode(void* destination, const void* source, int length) {
  if ((nullptr == destination) || (nullptr == source) || (0 == length)) {
    NOTREACHED();
    return ERROR_INVALID_PARAMETER;
  }

  // Change the page protection so that we can write.
  MEMORY_BASIC_INFORMATION memory_info;
  DWORD error = NO_ERROR;
  DWORD old_page_protection = 0;

  if (!VirtualQuery(destination, &memory_info, sizeof(memory_info))) {
    error = GetLastError();
    return error;
  }

  DWORD is_executable = (PAGE_EXECUTE | PAGE_EXECUTE_READ |
                         PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY) &
                        memory_info.Protect;

  if (VirtualProtect(destination, length,
                     is_executable ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE,
                     &old_page_protection)) {
    // Write the data.
    CopyMemory(destination, source, length);

    // Restore the old page protection.
    error = ERROR_SUCCESS;
    VirtualProtect(destination, length, old_page_protection,
                   &old_page_protection);
  } else {
    error = GetLastError();
  }

  return error;
}

}  // namespace internal
}  // namespace win
}  // namespace base
