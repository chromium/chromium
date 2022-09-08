// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_NODE_ID_H_
#define CC_PAINT_NODE_ID_H_

#include "cc/paint/paint_export.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {
// The NodeId is used to associate the DOM node with PaintOp, its peer in
// blink is DOMNodeId.
//
// This is used for ContentCapture in blink to capture on-screen content, the
// NodeId shall be set to DrawTextBlobOp when blink paints the main text
// content of page; for ContentCapture, please refer to
// third_party/blink/renderer/core/content_capture/README.md
using NodeId = int;

static const NodeId kInvalidNodeId = 0;

struct CC_PAINT_EXPORT NodeInfo {
  NodeInfo(NodeId node_id, const gfx::Rect& visual_rect)
      : node_id(node_id), visual_rect(visual_rect) {}

  NodeId node_id;
  gfx::Rect visual_rect;
};

}  // namespace cc

#endif  // CC_PAINT_NODE_ID_H_
