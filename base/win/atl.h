// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_ATL_H_
#define BASE_WIN_ATL_H_

// Check no prior poisonous defines were made.
#include "base/win/windows_defines.inc"
// Undefine before windows header will make the poisonous defines
#include "base/win/windows_undefines.inc"

// Declare our own exception thrower (atl_throw.h includes atldef.h).
#include "base/win/atl_throw.h"

#include <atlbase.h>      // NOLINT(build/include_order)
#include <atlcom.h>       // NOLINT(build/include_order)
#include <atlctl.h>       // NOLINT(build/include_order)
#include <atlhost.h>      // NOLINT(build/include_order)
#include <atlsecurity.h>  // NOLINT(build/include_order)
#include <atlwin.h>       // NOLINT(build/include_order)

// Undefine the poisonous defines
#include "base/win/windows_undefines.inc"  // NOLINT(build/include)
// Check no poisonous defines follow this include
#include "base/win/windows_defines.inc"  // NOLINT(build/include)

#endif  // BASE_WIN_ATL_H_
