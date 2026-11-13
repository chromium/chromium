// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_GET_POINTER_RESPONSE_H_
#define CHROME_BROWSER_TTC_CORE_GET_POINTER_RESPONSE_H_

#include "base/containers/span.h"
#include "chrome/browser/ttc/core/pointer_tracker.h"
#include "components/ttc/app/public/tool_types.h"

namespace ttc {

struct PointerContext;

// Builds the answer to a get_pointer call from `context`, what was captured of
// the page: an image of the page around where the user pointed, with a
// crosshair on each spot, and a description of each spot and of the page
// element there. `locations` are the places the user pointed at, most recent
// first, in the DIP space of the viewport that `context` is of; one outside
// the viewport is left out, since nothing can be shown or found there.
//
// The result, with all positions and sizes in the viewport's DIPs:
//
//   {
//     "status": "ok",
//     // In the order the user pointed. "index" is the number on the point's
//     // crosshair when there are several. "kind" is "press", "dwell" (the
//     // pointer rested there) or "moving" (it was only passing). "node" is
//     // the element there as DescribeNodeAtPoint() describes it, less its
//     // identity, and is absent if there is none.
//     "points": [{"index": 1, "x": 412, "y": 233, "kind": "dwell",
//                 "node": {"type": "image", "label": "...", "bounds": {...}}}],
//     // The part of the viewport that the image shows.
//     "crop_region": {"x": 162, "y": 83, "width": 500, "height": 300},
//     "viewport": {"width": 1280, "height": 720}
//   }
//
// If no location is in the viewport, the result is
// {"status": "no_active_pointer"}, without an image. If the screenshot in
// `context` cannot be made into the image, the response is an error.
//
// Decoding, drawing and encoding are CPU-bound; this uses nothing bound to a
// thread, so the caller can run it on a worker thread.
ToolResponse BuildGetPointerResponse(
    const PointerContext& context,
    base::span<const PointerTracker::Location> locations);

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_GET_POINTER_RESPONSE_H_
