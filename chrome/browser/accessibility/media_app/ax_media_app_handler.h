// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_AX_MEDIA_APP_HANDLER_H_
#define CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_AX_MEDIA_APP_HANDLER_H_

#include <stdint.h>

#include <vector>

#include "ui/gfx/geometry/insets.h"

namespace ash {

class AXMediaAppHandler {
 public:
  AXMediaAppHandler();
  AXMediaAppHandler(const AXMediaAppHandler&) = delete;
  AXMediaAppHandler& operator=(const AXMediaAppHandler&) = delete;
  virtual ~AXMediaAppHandler();

  bool IsOcrServiceEnabled() const;
  bool IsAccessibilityEnabled() const;
  void DocumentUpdated(const std::vector<gfx::Insets>& page_locations,
                       const std::vector<uint64_t>& dirty_pages);
  void ViewportUpdated(const gfx::Insets& viewport_box, float scaleFactor);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ACCESSIBILITY_MEDIA_APP_AX_MEDIA_APP_HANDLER_H_
