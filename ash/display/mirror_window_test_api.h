// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_MIRROR_WINDOW_TEST_API_H_
#define ASH_DISPLAY_MIRROR_WINDOW_TEST_API_H_

#include <vector>

#include "ui/base/cursor/mojom/cursor_type.mojom-forward.h"

namespace aura {
class Window;
class WindowTreeHost;
}  // namespace aura

namespace gfx {
class Point;
}

namespace ash {

class MirrorWindowTestApi {
 public:
  MirrorWindowTestApi() {}

  MirrorWindowTestApi(const MirrorWindowTestApi&) = delete;
  MirrorWindowTestApi& operator=(const MirrorWindowTestApi&) = delete;

  ~MirrorWindowTestApi() {}

  std::vector<aura::WindowTreeHost*> GetHosts() const;

  ui::mojom::CursorType GetCurrentCursorType() const;

  // Returns the position of the hot point within the cursor. This is
  // unaffected by the cursor location.
  const gfx::Point& GetCursorHotPoint() const;

  // Returns the position of the cursor hot point in root window coordinates.
  // This should be the same as the native cursor location.
  gfx::Point GetCursorHotPointLocationInRootWindow() const;

  const aura::Window* GetCursorHostWindow() const;
  gfx::Point GetCursorLocation() const;
};

}  // namespace ash

#endif  // ASH_DISPLAY_MIRROR_WINDOW_TEST_API_H_
