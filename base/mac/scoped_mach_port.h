// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_SCOPED_MACH_PORT_H_
#define BASE_MAC_SCOPED_MACH_PORT_H_

#include "base/apple/scoped_mach_port.h"

// This is a forwarding header so that Crashpad can continue to build correctly
// until mini_chromium and then it are updated and rolled.

// TODO(https://crbug.com/1444927): Update mini_chromium, update Crashpad, roll
// Crashpad, and then delete this forwarding header.

namespace base::mac {

using ScopedMachSendRight = base::apple::ScopedMachSendRight;
using ScopedMachReceiveRight = base::apple::ScopedMachReceiveRight;
using ScopedMachPortSet = base::apple::ScopedMachPortSet;

}  // namespace base::mac

#endif  // BASE_MAC_SCOPED_MACH_PORT_H_
