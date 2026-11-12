// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_POINTER_CROP_GEOMETRY_H_
#define CHROME_BROWSER_TTC_CORE_POINTER_CROP_GEOMETRY_H_

#include "base/containers/span.h"

namespace gfx {
class PointF;
class Rect;
class Size;
}  // namespace gfx

namespace ttc {

// Returns the part of the viewport, in DIPs, to show the model around
// `points`, which are in the viewport's DIP space. The rectangle is the
// points' bounding box plus a 150 DIP margin on every side, widened to at
// least 500 DIP so a single spot comes with enough context to tell what it is
// part of; for one spot that is 500x300. It is centered on the points, then
// shifted to stay inside `viewport_dip` (so a spot near an edge gets less
// margin on that side) and only shrinks if the viewport is smaller than that.
// Returns an empty rectangle if there are no points or no viewport.
//
// Points outside the viewport count as being on its nearest edge, so that
// there is always an answer, but nothing can be shown or found at such a point.
// TODO(crbug.com/571268337): Have the get_pointer tool drop points outside the
// viewport before it crops, marks and hit-tests, so that the three agree.
gfx::Rect ComputePointerCropRect(base::span<const gfx::PointF> points,
                                 const gfx::Size& viewport_dip);

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_POINTER_CROP_GEOMETRY_H_
