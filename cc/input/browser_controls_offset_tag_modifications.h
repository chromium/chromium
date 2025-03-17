// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_BROWSER_CONTROLS_OFFSET_TAG_MODIFICATIONS_H_
#define CC_INPUT_BROWSER_CONTROLS_OFFSET_TAG_MODIFICATIONS_H_

#include "cc/cc_export.h"
#include "cc/input/browser_controls_offset_tags.h"

namespace cc {

// Sent from browser to renderer.
// Contains the OffsetTags for browser controls and the additional heights that
// the controls must be offset by, so visual effects that overlap the web
// contents will disappear when controls are moved completely off screen.
struct CC_EXPORT BrowserControlsOffsetTagModifications {
  BrowserControlsOffsetTags tags;
  int top_controls_additional_height = 0;
  int bottom_controls_additional_height = 0;
};

}  // namespace cc

#endif  // CC_INPUT_BROWSER_CONTROLS_OFFSET_TAG_MODIFICATIONS_H_
