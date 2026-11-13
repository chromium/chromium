// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/get_pointer_response.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/containers/adapters.h"
#include "base/numerics/safe_conversions.h"
#include "base/values.h"
#include "chrome/browser/ttc/core/pointer_context_capturer.h"
#include "chrome/browser/ttc/core/pointer_crop_geometry.h"
#include "chrome/browser/ttc/core/pointer_hit_test.h"
#include "chrome/browser/ttc/core/pointer_image_annotator.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

namespace ttc {

ToolResponse BuildGetPointerResponse(
    const PointerContext& context,
    base::span<const PointerTracker::Location> locations) {
  // The viewport is what the screenshot shows, so that the points, the image
  // and the hit test all agree on what is in it. There is none if the
  // screenshot is not an image or the scale factor is not a usable number.
  const SkBitmap screenshot = gfx::JPEGCodec::Decode(context.screenshot);
  const gfx::Size viewport =
      context.scale_factor > 0
          ? gfx::ScaleToFlooredSize(
                gfx::Size(screenshot.width(), screenshot.height()),
                1 / context.scale_factor)
          : gfx::Size();
  if (viewport.IsEmpty()) {
    return ToolResponse::Error(
        actor::mojom::ActionResultCode::kScreenshotCaptureFailed,
        "The screenshot could not be read");
  }

  // In the order the user pointed. Contains() is false for a point that is
  // not a number, so those are left out too.
  std::vector<const PointerTracker::Location*> shown;
  std::vector<gfx::PointF> points;
  for (const PointerTracker::Location& location : base::Reversed(locations)) {
    if (gfx::RectF(gfx::SizeF(viewport)).Contains(location.point)) {
      shown.push_back(&location);
      points.push_back(location.point);
    }
  }
  if (points.empty()) {
    return ToolResponse::Success(
        base::DictValue().Set("status", "no_active_pointer"));
  }

  const gfx::Rect crop = ComputePointerCropRect(points, viewport);
  std::optional<VisualContent> image =
      AnnotatePointerImage(screenshot, crop, points, context.scale_factor);
  if (!image) {
    return ToolResponse::Error(
        actor::mojom::ActionResultCode::kScreenshotCaptureFailed,
        "The image of the pointer could not be made");
  }

  base::ListValue point_list;
  for (size_t i = 0; i < shown.size(); ++i) {
    const gfx::Point position = gfx::ToRoundedPoint(points[i]);
    const char* const kind = shown[i]->is_press   ? "press"
                             : shown[i]->is_dwell ? "dwell"
                                                  : "moving";
    base::DictValue point = base::DictValue()
                                .Set("index", base::checked_cast<int>(i + 1))
                                .Set("x", position.x())
                                .Set("y", position.y())
                                .Set("kind", kind);
    if (std::optional<base::DictValue> node = DescribeNodeAtPoint(
            context.page_content, points[i], context.scale_factor)) {
      // TODO(crbug.com/571268337): Report the node's identity once the server
      // maps it to the node ids the model knows. Until then the model would
      // mistake a DOM node id for one of those and act on another element.
      node->Remove("dom_node_id");
      node->Remove("document_id");
      point.Set("node", std::move(*node));
    }
    point_list.Append(std::move(point));
  }

  return ToolResponse::Success(
      base::DictValue()
          .Set("status", "ok")
          .Set("points", std::move(point_list))
          .Set("crop_region", base::DictValue()
                                  .Set("x", crop.x())
                                  .Set("y", crop.y())
                                  .Set("width", crop.width())
                                  .Set("height", crop.height()))
          .Set("viewport", base::DictValue()
                               .Set("width", viewport.width())
                               .Set("height", viewport.height())),
      std::move(*image));
}

}  // namespace ttc
