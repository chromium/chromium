// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/ipc/cc_param_traits_macros.h"
#include "ipc/param_traits_write_macros.h"

namespace IPC {
#undef CC_IPC_CC_PARAM_TRAITS_MACROS_H_
#include "cc/ipc/cc_param_traits_macros.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef CC_IPC_CC_PARAM_TRAITS_MACROS_H_
#include "cc/ipc/cc_param_traits_macros.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef CC_IPC_CC_PARAM_TRAITS_MACROS_H_
#include "cc/ipc/cc_param_traits_macros.h"
}  // namespace IPC
