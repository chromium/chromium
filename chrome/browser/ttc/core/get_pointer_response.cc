// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/get_pointer_response.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/containers/adapters.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ttc/core/pointer_context_capturer.h"
#include "chrome/browser/ttc/core/pointer_crop_geometry.h"
#include "chrome/browser/ttc/core/pointer_hit_test.h"
#include "chrome/browser/ttc/core/pointer_image_annotator.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "third_party/blink/public/common/dom/dom_node_id.h"
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

namespace {

using optimization_guide::proto::ContentNode;

// Enough to recognize what is selected; the page content has the rest.
constexpr size_t kMaxSelectedTextBytes = 1024;

base::DictValue RectToDict(const gfx::Rect& rect) {
  return base::DictValue()
      .Set("x", rect.x())
      .Set("y", rect.y())
      .Set("width", rect.width())
      .Set("height", rect.height());
}

// Returns where the nodes from `start_node_id` to `end_node_id` of the main
// document of the page under `root` are in the viewport, in device pixels:
// the union of the boxes of the content from the start node to the end of the
// end node, in document order. The ids are DOM node ids: the page content
// gives them for the two ends of a selection, and for each of its own nodes
// as `common_ancestor_dom_node_id`. Empty if either node is not in the page
// content, as for a selection inside a text field, if the end node comes
// before the start node, or if none of the content is in the viewport.
gfx::Rect FindNodeRangeBounds(const ContentNode& root,
                              blink::DOMNodeIdType start_node_id,
                              blink::DOMNodeIdType end_node_id) {
  // Nodes without an id report 0, the null id, which must not pass for a
  // match.
  if (start_node_id.is_null() || end_node_id.is_null()) {
    return gfx::Rect();
  }
  gfx::Rect bounds;
  bool is_in_range = false;
  // The depth of the end node while in its subtree, with which the range ends.
  // The end node can be reached before the start node, which it may contain:
  // a selection from inside <b> to the end of the <p> around it ends at the
  // <p>.
  std::optional<size_t> end_depth;
  // Iterative because the tree comes from a renderer and may be deep.
  std::vector<std::pair<const ContentNode*, size_t>> stack = {{&root, 0}};
  while (!stack.empty()) {
    const auto [node, depth] = stack.back();
    stack.pop_back();
    if (end_depth && depth <= *end_depth) {
      if (is_in_range) {
        return bounds;
      }
      // The end node did not contain the start node.
      end_depth.reset();
    }
    const auto& attributes = node->content_attributes();
    const blink::DOMNodeIdType node_id(
        attributes.common_ancestor_dom_node_id());
    is_in_range |= node_id == start_node_id;
    if (!end_depth && node_id == end_node_id) {
      end_depth = depth;
    }
    if (attributes.attribute_type() ==
        optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME) {
      // Node ids only mean something within their own document.
      continue;
    }
    if (is_in_range && node->children_nodes().empty()) {
      const auto& box = attributes.geometry().visible_bounding_box();
      bounds.Union(gfx::Rect(box.x(), box.y(), box.width(), box.height()));
    }
    for (const ContentNode& child : base::Reversed(node->children_nodes())) {
      stack.emplace_back(&child, depth + 1);
    }
  }
  // Without both nodes, in order, the range is not known.
  return is_in_range && end_depth ? bounds : gfx::Rect();
}

// Returns get_pointer's answer for a user who is not pointing but has selected
// text in the page's main document, or nullopt if they have not, or it cannot
// be shown. `screenshot` shows `viewport`, which is in DIPs.
std::optional<ToolResponse> BuildSelectionResponse(
    const PointerContext& context,
    const SkBitmap& screenshot,
    const gfx::Size& viewport) {
  const auto& selection = context.page_content.main_frame_data()
                              .frame_interaction_info()
                              .selection();
  // A selection without text, such as one of only an image, does not count.
  // TODO(crbug.com/585958593): Support selections without text.
  if (selection.selected_text().empty()) {
    return std::nullopt;
  }
  gfx::Rect bounds = gfx::ScaleToEnclosingRect(
      FindNodeRangeBounds(context.page_content.root_node(),
                          blink::DOMNodeIdType(selection.start_node_id()),
                          blink::DOMNodeIdType(selection.end_node_id())),
      1 / context.scale_factor);
  bounds.Intersect(gfx::Rect(viewport));
  if (bounds.IsEmpty()) {
    return std::nullopt;
  }

  const gfx::PointF corners[] = {gfx::PointF(bounds.origin()),
                                 gfx::PointF(bounds.bottom_right())};
  const gfx::Rect crop = ComputePointerCropRect(corners, viewport);
  // No crosshair: the page itself shows what is selected.
  std::optional<VisualContent> image =
      AnnotatePointerImage(screenshot, crop, {}, context.scale_factor);
  if (!image) {
    return std::nullopt;
  }
  return ToolResponse::Success(
      base::DictValue()
          .Set("status", "ok")
          .Set("selection", base::DictValue()
                                .Set("text", base::TruncateUTF8ToByteSize(
                                                 selection.selected_text(),
                                                 kMaxSelectedTextBytes))
                                .Set("bounds", RectToDict(bounds)))
          .Set("crop_region", RectToDict(crop))
          .Set("viewport", base::DictValue()
                               .Set("width", viewport.width())
                               .Set("height", viewport.height())),
      std::move(*image));
}

}  // namespace

ToolResponse BuildGetPointerResponse(
    const PointerContext& context,
    base::span<const PointerTracker::Location> locations,
    bool include_node_identity) {
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
    if (kTtcGetPointerUseSelection.Get()) {
      if (std::optional<ToolResponse> response =
              BuildSelectionResponse(context, screenshot, viewport)) {
        return std::move(*response);
      }
    }
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
      if (!include_node_identity) {
        node->Remove("dom_node_id");
        node->Remove("document_id");
      }
      point.Set("node", std::move(*node));
    }
    point_list.Append(std::move(point));
  }

  return ToolResponse::Success(
      base::DictValue()
          .Set("status", "ok")
          .Set("points", std::move(point_list))
          .Set("crop_region", RectToDict(crop))
          .Set("viewport", base::DictValue()
                               .Set("width", viewport.width())
                               .Set("height", viewport.height())),
      std::move(*image));
}

}  // namespace ttc
