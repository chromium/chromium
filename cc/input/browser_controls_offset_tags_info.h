// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_BROWSER_CONTROLS_OFFSET_TAGS_INFO_H_
#define CC_INPUT_BROWSER_CONTROLS_OFFSET_TAGS_INFO_H_

#include "cc/cc_export.h"
#include "components/viz/common/quads/offset_tag.h"

namespace cc {

// A group of OffsetTags and constraints. Right now this only contains data for
// top controls, but more can be added here in the future when more UI elements
// are moved by viz without browser involvement.
struct CC_EXPORT BrowserControlsOffsetTagsInfo {
  viz::OffsetTag top_controls_offset_tag;
  int top_controls_height;
};

}  // namespace cc

#endif  // CC_INPUT_BROWSER_CONTROLS_OFFSET_TAGS_INFO_H_
