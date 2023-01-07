// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOST_ASH_WINDOW_TREE_HOST_DELEGATE_H_
#define ASH_HOST_ASH_WINDOW_TREE_HOST_DELEGATE_H_

#include <stdint.h>

#include "ash/ash_export.h"

namespace aura {
class WindowTreeHost;
}  // namespace aura

namespace display {
class Display;
}  // namespace display

namespace ash {

// A delegate for the window tree host.
class ASH_EXPORT AshWindowTreeHostDelegate {
 public:
  AshWindowTreeHostDelegate() = default;

  AshWindowTreeHostDelegate(const AshWindowTreeHostDelegate&) = delete;
  AshWindowTreeHostDelegate& operator=(const AshWindowTreeHostDelegate&) =
      delete;

  virtual ~AshWindowTreeHostDelegate() = default;

  // Returns a pointer to the display with |display_id| if found, or nullptr if
  // not found.
  virtual const display::Display* GetDisplayById(int64_t display_id) const = 0;

  // Sets the current window tree host that is the source of events which will
  // be forwarded by the unified mode event targeter to the unified host.
  virtual void SetCurrentEventTargeterSourceHost(
      aura::WindowTreeHost* targeter_src_host) = 0;
};

}  // namespace ash

#endif  // ASH_HOST_ASH_WINDOW_TREE_HOST_DELEGATE_H_
