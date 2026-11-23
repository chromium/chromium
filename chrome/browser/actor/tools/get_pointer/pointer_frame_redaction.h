// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_GET_POINTER_POINTER_FRAME_REDACTION_H_
#define CHROME_BROWSER_ACTOR_TOOLS_GET_POINTER_POINTER_FRAME_REDACTION_H_

class SkBitmap;

namespace optimization_guide::proto {
class AnnotatedPageContent;
}  // namespace optimization_guide::proto

namespace actor {

// Paints over, in `screenshot`, the frames of `page_content` whose documents
// have another origin than the page's, so that the model does not see in the
// image what it is not given as page content: the frames that
// IsCrossOriginFrame() in pointer_hit_test.h is true of, which the hit test
// leaves out too. `screenshot` is of the viewport that `page_content` is of,
// in the device pixels its geometry is in.
//
// A frame is painted over both where the page content has it and where the
// screenshot does, if that was recorded: the two are captured at the same
// time but not in the same instant. An open popup, e.g. the list of a
// <select>, is painted over too unless the page's main document opened it.
void PaintOverCrossOriginFrames(
    const optimization_guide::proto::AnnotatedPageContent& page_content,
    SkBitmap& screenshot);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_GET_POINTER_POINTER_FRAME_REDACTION_H_
