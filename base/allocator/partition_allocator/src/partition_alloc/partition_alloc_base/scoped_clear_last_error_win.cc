// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_alloc_base/scoped_clear_last_error.h"

#include <windows.h>

namespace partition_alloc::internal::base {

ScopedClearLastError::ScopedClearLastError()
    : ScopedClearLastErrorBase(), last_system_error_(GetLastError()) {
  SetLastError(0);
}

ScopedClearLastError::~ScopedClearLastError() {
  SetLastError(last_system_error_);
}

}  // namespace partition_alloc::internal::base
