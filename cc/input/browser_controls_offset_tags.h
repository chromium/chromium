// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_BROWSER_CONTROLS_OFFSET_TAGS_H_
#define CC_INPUT_BROWSER_CONTROLS_OFFSET_TAGS_H_

#include "cc/cc_export.h"
#include "components/viz/common/quads/offset_tag.h"

namespace cc {

// Contains all OffsetTags for browser controls. Tagging browser controls with
// OffsetTags allow the renderer and viz to move them during scrolls/animations
// without needing browser compositor frames.
struct CC_EXPORT BrowserControlsOffsetTags {
  viz::OffsetTag top_controls_offset_tag;
  viz::OffsetTag content_offset_tag;
  viz::OffsetTag bottom_controls_offset_tag;
};

}  // namespace cc

#endif  // CC_INPUT_BROWSER_CONTROLS_OFFSET_TAGS_H_
