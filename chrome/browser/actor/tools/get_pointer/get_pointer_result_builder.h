// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_GET_POINTER_GET_POINTER_RESULT_BUILDER_H_
#define CHROME_BROWSER_ACTOR_TOOLS_GET_POINTER_GET_POINTER_RESULT_BUILDER_H_

#include "base/containers/span.h"
#include "chrome/browser/actor/tools/tool_result.h"
#include "ui/gfx/geometry/point_f.h"

namespace actor {

struct PointerContext;

// A place the user pointed at.
struct PointerLocation {
  // Where it is, in the DIP space of the viewport.
  gfx::PointF point;
  // Whether the user pressed there.
  bool is_press = false;
  // Whether the pointer rested there. If not, it was only passing.
  bool is_dwell = false;
};

// What BuildGetPointerResult() reports.
struct GetPointerResultOptions {
  // Whether the element at a point is reported with its identity.
  bool include_node_identity = false;
  // Whether the user's selection is reported if they are not pointing.
  bool use_selection = false;
};

// Builds the result of a get_pointer call from `context`, what was captured of
// the page: an image of the page around where the user pointed, with a
// crosshair on each spot, and a description of each spot and of the page
// element there. `locations` are the places the user pointed at, most recent
// first, in the DIP space of the viewport that `context` is of; one outside
// the viewport is left out, since nothing can be shown or found there.
//
// The result's data is a GetPointerToolResult. Its dict, with all positions
// and sizes in the viewport's DIPs:
//
//   {
//     "status": "ok",
//     // In the order the user pointed. "index" is the number on the point's
//     // crosshair when there are several. "kind" is "press", "dwell" (the
//     // pointer rested there) or "moving" (it was only passing). "node" is
//     // the element there as DescribeNodeAtPoint() describes it, and is
//     // absent if there is none. It has the element's identity, which is the
//     // pair of its DOM node id and document id ("dom_node_id" and
//     // "document_id"), only if `options.include_node_identity`.
//     "points": [{"index": 1, "x": 412, "y": 233, "kind": "dwell",
//                 "node": {"type": "image", "label": "...", "bounds": {...}}}],
//     // The part of the viewport that the image shows.
//     "crop_region": {"x": 162, "y": 83, "width": 500, "height": 300},
//     "viewport": {"width": 1280, "height": 720}
//   }
//
// Frames whose documents have another origin than the page's are painted over
// in the image, whichever of the two it shows; see
// PaintOverCrossOriginFrames().
//
// If no location is in the viewport and `options.use_selection` is set, the
// user's selection in the page's main document is used instead. The image
// then has no crosshair, and "points" is replaced by one of:
//
//   // The start of the selected text.
//   "selection": {"text": "...", "bounds": {...}}
//   // If only whitespace is selected: the first few images and other media
//   // in the selection, each like "node" above but never with its identity.
//   "selection": {"nodes": [{"type": "image", ...}], "bounds": {...}}
//
// A selection outside the viewport, inside a text field, or with neither text
// nor media is ignored.
//
// Otherwise, if no location is in the viewport, the dict is
// {"status": "no_active_pointer"}, without an image. If the screenshot in
// `context` cannot be made into the image, the result is an error.
//
// Decoding, drawing and encoding are CPU-bound; this uses nothing bound to a
// thread, so the caller can run it on a worker thread.
ToolResult BuildGetPointerResult(const PointerContext& context,
                                 base::span<const PointerLocation> locations,
                                 const GetPointerResultOptions& options);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_GET_POINTER_GET_POINTER_RESULT_BUILDER_H_
