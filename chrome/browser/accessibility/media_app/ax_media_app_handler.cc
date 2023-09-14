// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/media_app/ax_media_app_handler.h"

#include <utility>

namespace ash {

AXMediaAppHandler::AXMediaAppHandler() = default;
AXMediaAppHandler::~AXMediaAppHandler() = default;

bool AXMediaAppHandler::IsOcrServiceEnabled() const {
  return true;
}

bool AXMediaAppHandler::IsAccessibilityEnabled() const {
  return true;
}

void AXMediaAppHandler::DocumentUpdated(
    const std::vector<gfx::Insets>& page_locations,
    const std::vector<uint64_t>& dirty_pages) {}

void AXMediaAppHandler::ViewportUpdated(const gfx::Insets& viewport_box,
                                        float scaleFactor) {}

}  // namespace ash
