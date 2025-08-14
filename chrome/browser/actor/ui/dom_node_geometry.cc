// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/dom_node_geometry.h"

#include <absl/container/flat_hash_map.h>

#include "base/metrics/histogram_functions.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"

namespace actor::ui {
namespace {
using base::UmaHistogramEnumeration;
using optimization_guide::proto::ContentAttributes;
using optimization_guide::proto::ContentNode;
using optimization_guide::proto::DocumentIdentifier;

using NodeGeomMap = absl::flat_hash_map<DomNode, ContentAttributes>;

constexpr char kDomNodeResultHistogram[] =
    "Actor.DomNodeGeometry.GetDomNodeResult";

void BuildNodeMapInternal(const DocumentIdentifier& doc_id,
                          const ContentNode& root,
                          NodeGeomMap& map) {
  const auto& content_attributes = root.content_attributes();
  if (content_attributes.has_common_ancestor_dom_node_id()) {
    DomNode key{.node_id = content_attributes.common_ancestor_dom_node_id(),
                .document_identifier = doc_id.serialized_token()};
    map.emplace(std::move(key), root.content_attributes());
  }

  // If the node is an iframe, then use the iframe's doc_id for its children.
  // Otherwise use the same doc_id.
  const DocumentIdentifier& child_doc_id =
      (content_attributes.has_iframe_data() &&
       content_attributes.iframe_data().has_frame_data())
          ? content_attributes.iframe_data().frame_data().document_identifier()
          : doc_id;
  for (const auto& child : root.children_nodes()) {
    BuildNodeMapInternal(child_doc_id, child, map);
  }
}

NodeGeomMap BuildNodeMap(const DocumentIdentifier& doc_id,
                         const ContentNode& root) {
  NodeGeomMap map;
  BuildNodeMapInternal(doc_id, root, map);
  return map;
}
}  // namespace

std::optional<gfx::Point> GetDomNodePointFromApc(
    const optimization_guide::proto::AnnotatedPageContent& apc,
    const DomNode& node) {
  if (!apc.has_main_frame_data() ||
      !apc.main_frame_data().has_document_identifier()) {
    UmaHistogramEnumeration(kDomNodeResultHistogram,
                            GetDomNodeResult::kNoApcMainFrameData);
    return std::nullopt;
  }
  NodeGeomMap node_map = BuildNodeMap(
      apc.main_frame_data().document_identifier(), apc.root_node());
  auto it = node_map.find(node);
  if (it == node_map.end()) {
    UmaHistogramEnumeration(kDomNodeResultHistogram,
                            GetDomNodeResult::kNodeNotFoundInApc);
    return std::nullopt;
  }
  const ContentAttributes& attr = it->second;
  if (!attr.has_geometry()) {
    UmaHistogramEnumeration(kDomNodeResultHistogram,
                            GetDomNodeResult::kNoGeometry);
    return std::nullopt;
  }
  const auto& geom = attr.geometry();
  if (!geom.has_visible_bounding_box()) {
    UmaHistogramEnumeration(kDomNodeResultHistogram,
                            GetDomNodeResult::kOffScreen);
    return std::nullopt;
  }
  const auto& rect = geom.visible_bounding_box();
  const int x = rect.x() + rect.width() / 2;
  const int y = rect.y() + rect.height() / 2;
  UmaHistogramEnumeration(kDomNodeResultHistogram, GetDomNodeResult::kSuccess);
  return gfx::Point(x, y);
}

}  // namespace actor::ui
