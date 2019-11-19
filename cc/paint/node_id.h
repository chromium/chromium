// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_NODE_ID_H_
#define CC_PAINT_NODE_ID_H_

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

}  // namespace cc

#endif  // CC_PAINT_NODE_ID_H_
