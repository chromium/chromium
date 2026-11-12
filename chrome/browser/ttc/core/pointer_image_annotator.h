// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_POINTER_IMAGE_ANNOTATOR_H_
#define CHROME_BROWSER_TTC_CORE_POINTER_IMAGE_ANNOTATOR_H_

#include <optional>

#include "base/containers/span.h"
#include "components/ttc/app/public/tool_types.h"

class SkBitmap;

namespace gfx {
class PointF;
class Rect;
}  // namespace gfx

namespace ttc {

// Returns the part `crop_dip` of `viewport`, a screenshot of the viewport in
// device pixels, as a JPEG with a crosshair on each of `points`, for the model
// to see what the user pointed at. `crop_dip` and `points` are in the
// viewport's DIP space; `scale_factor` is the viewport's device pixels per DIP.
// When there are several points each crosshair gets a badge with its 1-based
// index, so the model can tell them apart. Returns nullopt if there is no
// viewport, `scale_factor` is not a plausible scale factor, the crop lies
// outside the viewport or the image cannot be encoded.
//
// Drawing and encoding are CPU-bound; this uses only Skia, so the caller can
// run it on a worker thread.
std::optional<VisualContent> AnnotatePointerImage(
    const SkBitmap& viewport,
    const gfx::Rect& crop_dip,
    base::span<const gfx::PointF> points,
    float scale_factor);

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_POINTER_IMAGE_ANNOTATOR_H_
