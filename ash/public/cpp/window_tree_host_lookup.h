// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WINDOW_TREE_HOST_LOOKUP_H_
#define ASH_PUBLIC_CPP_WINDOW_TREE_HOST_LOOKUP_H_

#include <stdint.h>

#include "ash/ash_export.h"

namespace aura {
class WindowTreeHost;
}

namespace ash {

// Returns the WindowTreeHost for a particular display id, null if there is
// no display for |display_id|.
ASH_EXPORT aura::WindowTreeHost* GetWindowTreeHostForDisplay(
    int64_t display_id);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WINDOW_TREE_HOST_LOOKUP_H_
