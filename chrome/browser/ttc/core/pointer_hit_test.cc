// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/pointer_hit_test.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/adapters.h"
#include "base/strings/string_util.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"

namespace ttc {

namespace {

using optimization_guide::proto::ContentAttributes;
using optimization_guide::proto::ContentNode;

// Enough to recognize the element by; the page content has the rest.
constexpr size_t kMaxTextBytes = 128;

// A plain container that covers more than this fraction of the viewport is
// the page's background. Pointing at one that big is pointing at empty space
// in a wrapper for the page's layout, and describing the wrapper would pass
// off the start of the page's text as the thing pointed at. A smaller one,
// e.g. a card or a sidebar, can be what the user means. Half is a judgment,
// not a measurement: what fills most of the viewport is the page rather than
// a thing on it. To be tuned once there are evals of the tool.
constexpr double kBackgroundViewportFraction = 0.5;

// Returns the start of the text of `root` and its descendants, in document
// order. Iterative because the tree comes from a renderer and may be deep.
std::string CollectText(const ContentNode& root) {
  std::string text;
  std::vector<const ContentNode*> stack = {&root};
  while (!stack.empty() && text.size() < kMaxTextBytes) {
    const ContentNode* node = stack.back();
    stack.pop_back();
    const ContentAttributes& attributes = node->content_attributes();
    std::string_view raw_piece;
    if (attributes.has_text_data()) {
      raw_piece = attributes.text_data().text_content();
    } else if (attributes.has_image_data()) {
      raw_piece = attributes.image_data().image_caption();
    }
    // Text nodes come with the whitespace around them, which must not count
    // towards the limit.
    const std::string piece = base::CollapseWhitespaceASCII(
        raw_piece, /*trim_sequences_with_line_breaks=*/false);
    if (!piece.empty()) {
      if (!text.empty()) {
        text += ' ';
      }
      text += piece;
    }
    for (const ContentNode& child : base::Reversed(node->children_nodes())) {
      stack.push_back(&child);
    }
  }
  return std::string(base::TruncateUTF8ToByteSize(text, kMaxTextBytes));
}

// Returns the link or form control that `target` is part of, if any, and
// `target` otherwise: the hit test finds the innermost node, e.g. a button's
// text, but the control is what the user means and what tools act on. Looks
// no further than `target`'s own document.
const ContentNode& FindControlOrSelf(const ContentNode& root,
                                     const ContentNode& target) {
  // Depth-first, keeping the path from `root` to the node being visited.
  std::vector<const ContentNode*> path;
  std::vector<std::pair<const ContentNode*, size_t>> stack = {{&root, 0}};
  while (!stack.empty()) {
    const auto [node, depth] = stack.back();
    stack.pop_back();
    path.resize(depth);
    path.push_back(node);
    if (node != &target) {
      for (const ContentNode& child : node->children_nodes()) {
        stack.emplace_back(&child, depth + 1);
      }
      continue;
    }
    for (const ContentNode* ancestor : base::Reversed(path)) {
      switch (ancestor->content_attributes().attribute_type()) {
        case optimization_guide::proto::CONTENT_ATTRIBUTE_ANCHOR:
        case optimization_guide::proto::CONTENT_ATTRIBUTE_FORM_CONTROL:
          return *ancestor;
        case optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME:
          // The rest of the path is in other documents.
          return target;
        default:
          break;
      }
    }
    break;
  }
  return target;
}

// Whether a node is the page's background rather than something on it: a
// document's root, or a plain container that spans most of the viewport, such
// as the body.
bool IsBackground(const ContentAttributes& attributes,
                  const ContentAttributes& viewport) {
  switch (attributes.attribute_type()) {
    case optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT:
      return true;
    case optimization_guide::proto::CONTENT_ATTRIBUTE_CONTAINER: {
      const auto& box = attributes.geometry().visible_bounding_box();
      const auto& viewport_box = viewport.geometry().visible_bounding_box();
      const double area = 1.0 * box.width() * box.height();
      const double viewport_area =
          1.0 * viewport_box.width() * viewport_box.height();
      return attributes.label().empty() &&
             area > kBackgroundViewportFraction * viewport_area;
    }
    default:
      return false;
  }
}

// E.g. "image" for CONTENT_ATTRIBUTE_IMAGE.
std::string TypeName(const ContentAttributes& attributes) {
  const std::string& name =
      optimization_guide::proto::ContentAttributeType_Name(
          attributes.attribute_type());
  return base::ToLowerASCII(
      base::RemovePrefix(name, "CONTENT_ATTRIBUTE_").value_or(name));
}

}  // namespace

std::optional<base::DictValue> DescribeNodeAtPoint(
    const optimization_guide::proto::AnnotatedPageContent& page_content,
    const gfx::PointF& point,
    float scale_factor) {
  if (scale_factor <= 0) {
    return std::nullopt;
  }
  // The page content's geometry is in device pixels relative to the viewport.
  std::optional<optimization_guide::TargetNodeInfo> target =
      optimization_guide::FindNodeAtPoint(
          page_content,
          gfx::ToFlooredPoint(gfx::ScalePoint(point, scale_factor)));
  if (!target) {
    return std::nullopt;
  }
  const ContentNode& node =
      FindControlOrSelf(page_content.root_node(), *target->node);
  const ContentAttributes& attributes = node.content_attributes();
  if (IsBackground(attributes, page_content.root_node().content_attributes())) {
    return std::nullopt;
  }

  base::DictValue description;
  if (attributes.has_common_ancestor_dom_node_id()) {
    description.Set("dom_node_id", attributes.common_ancestor_dom_node_id());
  }
  description.Set("document_id",
                  target->document_identifier.serialized_token());
  description.Set("type", TypeName(attributes));
  if (!attributes.label().empty()) {
    description.Set("label", attributes.label());
  }
  if (std::string text = CollectText(node); !text.empty()) {
    description.Set("text", std::move(text));
  }
  const auto& box = attributes.geometry().visible_bounding_box();
  const gfx::Rect bounds = gfx::ScaleToEnclosingRect(
      gfx::Rect(box.x(), box.y(), box.width(), box.height()), 1 / scale_factor);
  description.Set("bounds", base::DictValue()
                                .Set("x", bounds.x())
                                .Set("y", bounds.y())
                                .Set("width", bounds.width())
                                .Set("height", bounds.height()));
  return description;
}

}  // namespace ttc
