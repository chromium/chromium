// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_NT_STATUS_H_
#define BASE_WIN_NT_STATUS_H_

#include <windows.h>
#include <winternl.h>

#include "base/base_export.h"

namespace base {
namespace win {

// Returns the value of the most recent thread-local NTSTATUS value, i.e.
// LastStatusValue from the Thread Environment Block (TEB). This may be used,
// for example, to deduce more information about the outcome of an API call
// where the meaning of GetLastError() is ambiguous.
BASE_EXPORT NTSTATUS GetLastNtStatus();

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_NT_STATUS_H_
