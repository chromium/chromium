// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/get_pointer_response.h"

#include <stdint.h>

#include <optional>
#include <utility>
#include <vector>

#include "base/containers/adapters.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/actor/tools/get_pointer/pointer_crop_geometry.h"
#include "chrome/browser/actor/tools/get_pointer/pointer_frame_redaction.h"
#include "chrome/browser/actor/tools/get_pointer/pointer_hit_test.h"
#include "chrome/browser/actor/tools/get_pointer/pointer_image_annotator.h"
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ttc/core/pointer_context_capturer.h"
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

using actor::AnnotatePointerImage;
using actor::ComputePointerCropRect;
using actor::DescribeNode;
using actor::DescribeNodeAtPoint;
using actor::PaintOverCrossOriginFrames;
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

// Returns `jpeg`, the bytes of a JPEG, as the image of a response.
VisualContent JpegContent(std::vector<uint8_t> jpeg) {
  return VisualContent{.image_data = std::move(jpeg),
                       .mime_type = "image/jpeg"};
}

// The most selected things described in a result; enough to tell the model
// what is selected, and the image shows the rest.
constexpr size_t kMaxSelectedNodes = 5;

// Returns the nodes from `start_node_id` to `end_node_id` of the main document
// of the page under `root`: the start node, everything after it up to the end
// node, and all of the end node, in document order. The ids are DOM node ids:
// the page content gives them for the two ends of a selection, and for each of
// its own nodes as `common_ancestor_dom_node_id`. Empty if either node is not
// in the page content, as for a selection inside a text field, or if the end
// node comes before the start node.
std::vector<const ContentNode*> FindNodeRange(
    const ContentNode& root,
    blink::DOMNodeIdType start_node_id,
    blink::DOMNodeIdType end_node_id) {
  // Nodes without an id report 0, the null id, which must not pass for a
  // match.
  if (start_node_id.is_null() || end_node_id.is_null()) {
    return {};
  }
  std::vector<const ContentNode*> range;
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
        return range;
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
    if (is_in_range) {
      range.push_back(node);
    }
    for (const ContentNode& child : base::Reversed(node->children_nodes())) {
      stack.emplace_back(&child, depth + 1);
    }
  }
  // Without both nodes, in order, the range is not known.
  if (!is_in_range || !end_depth) {
    range.clear();
  }
  return range;
}

// Where `node` is in the viewport, in device pixels.
gfx::Rect VisibleBox(const ContentNode& node) {
  const auto& box = node.content_attributes().geometry().visible_bounding_box();
  return gfx::Rect(box.x(), box.y(), box.width(), box.height());
}

// Whether `node` is content that shows something other than text.
bool IsMedia(const ContentNode& node) {
  switch (node.content_attributes().attribute_type()) {
    case optimization_guide::proto::CONTENT_ATTRIBUTE_IMAGE:
    case optimization_guide::proto::CONTENT_ATTRIBUTE_SVG_ROOT:
    case optimization_guide::proto::CONTENT_ATTRIBUTE_CANVAS:
    case optimization_guide::proto::CONTENT_ATTRIBUTE_VIDEO:
      return true;
    default:
      return false;
  }
}

// Returns get_pointer's answer for a user who is not pointing but has made a
// selection in the page's main document, or nullopt if they have not, or it
// cannot be shown. `screenshot` shows `viewport`, which is in DIPs.
std::optional<ToolResponse> BuildSelectionResponse(
    const PointerContext& context,
    const SkBitmap& screenshot,
    const gfx::Size& viewport) {
  const auto& selection = context.page_content.main_frame_data()
                              .frame_interaction_info()
                              .selection();
  const std::vector<const ContentNode*> range =
      FindNodeRange(context.page_content.root_node(),
                    blink::DOMNodeIdType(selection.start_node_id()),
                    blink::DOMNodeIdType(selection.end_node_id()));

  // If the selected text is only whitespace, the media in the range are what
  // is selected, and its text nodes are ignored. This happens when the user
  // drags from the end of one paragraph, over an image, to the start of the
  // next paragraph: the text is then just the line breaks between the blocks.
  // Whitespace here is any Unicode whitespace character, not only ASCII's,
  // since pages often put no-break spaces (U+00A0) around images and in empty
  // paragraphs.
  //
  // TODO(crbug.com/585958593): Handle a selection of just an image with no
  // text around it, e.g. <div><img></div>. Its text is empty, and Blink only
  // adds a selection to the page content if its text is not empty, so there
  // is no selection here and get_pointer answers no_active_pointer.
  const bool has_text = !base::ContainsOnlyChars(
      base::UTF8ToUTF16(selection.selected_text()), base::kWhitespaceUTF16);
  base::DictValue description;
  gfx::Rect bounds_in_pixels;
  if (has_text) {
    description.Set("text",
                    base::TruncateUTF8ToByteSize(selection.selected_text(),
                                                 kMaxSelectedTextBytes));
    // Where the content from the start node to the end of the end node is.
    for (const ContentNode* node : range) {
      if (node->children_nodes().empty()) {
        bounds_in_pixels.Union(VisibleBox(*node));
      }
    }
  } else {
    // Described without their identity, whatever
    // kTtcGetPointerIncludeNodeIdentity says: the model must not be given a DOM
    // node id that the server has not mapped to one of the node ids it knows,
    // and the server maps those of the elements pointed at only.
    base::ListValue nodes;
    for (const ContentNode* node : range) {
      const gfx::Rect box = VisibleBox(*node);
      if (!IsMedia(*node) || box.IsEmpty()) {
        continue;
      }
      bounds_in_pixels.Union(box);
      if (nodes.size() < kMaxSelectedNodes) {
        nodes.Append(
            DescribeNode(*node, context.page_content, context.scale_factor));
      }
    }
    description.Set("nodes", std::move(nodes));
  }

  gfx::Rect bounds =
      gfx::ScaleToEnclosingRect(bounds_in_pixels, 1 / context.scale_factor);
  bounds.Intersect(gfx::Rect(viewport));
  if (bounds.IsEmpty()) {
    return std::nullopt;
  }

  const gfx::PointF corners[] = {gfx::PointF(bounds.origin()),
                                 gfx::PointF(bounds.bottom_right())};
  const gfx::Rect crop = ComputePointerCropRect(corners, viewport);
  // No crosshair: the page itself shows what is selected.
  std::optional<std::vector<uint8_t>> jpeg =
      AnnotatePointerImage(screenshot, crop, {}, context.scale_factor);
  if (!jpeg) {
    return std::nullopt;
  }
  description.Set("bounds", RectToDict(bounds));
  return ToolResponse::Success(
      base::DictValue()
          .Set("status", "ok")
          .Set("selection", std::move(description))
          .Set("crop_region", RectToDict(crop))
          .Set("viewport", base::DictValue()
                               .Set("width", viewport.width())
                               .Set("height", viewport.height())),
      JpegContent(std::move(*jpeg)));
}

}  // namespace

ToolResponse BuildGetPointerResponse(
    const PointerContext& context,
    base::span<const PointerTracker::Location> locations,
    bool include_node_identity) {
  // The viewport is what the screenshot shows, so that the points, the image
  // and the hit test all agree on what is in it. There is none if the
  // screenshot is not an image or the scale factor is not a usable number.
  SkBitmap screenshot = gfx::JPEGCodec::Decode(context.screenshot);
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

  // The model is not given the content of frames of other origins than the
  // page's, so they are not for it to see either, whether the image is of
  // where the user points or of what they have selected.
  PaintOverCrossOriginFrames(context.page_content, screenshot);

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
  std::optional<std::vector<uint8_t>> jpeg =
      AnnotatePointerImage(screenshot, crop, points, context.scale_factor);
  if (!jpeg) {
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
      JpegContent(std::move(*jpeg)));
}

}  // namespace ttc
