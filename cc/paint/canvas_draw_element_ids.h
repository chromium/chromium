// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_CANVAS_DRAW_ELEMENT_IDS_H_
#define CC_PAINT_CANVAS_DRAW_ELEMENT_IDS_H_

#include <map>
#include <string>

#include "cc/paint/element_id.h"

namespace cc {

// Ids for the elements of a single canvas element which can be drawn with
// html-in-canvas. The key is the ElementId of the canvas child element, and the
// value is the dom node id.
using CanvasDrawElementIds = std::map<ElementId, std::string>;

// Map of the ElementId for a canvas element to the `CanvasDrawElementIds` for
// that canvas element.
using AllCanvasDrawElementIds = std::map<ElementId, CanvasDrawElementIds>;

}  // namespace cc

#endif  // CC_PAINT_CANVAS_DRAW_ELEMENT_IDS_H_
