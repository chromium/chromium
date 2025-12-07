// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/dom_node_geometry.h"

#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"

namespace actor::ui {
namespace {
using base::UmaHistogramEnumeration;
using optimization_guide::proto::AnnotatedPageContent;
using optimization_guide::proto::ContentAttributes;
using optimization_guide::proto::ContentNode;
using optimization_guide::proto::DocumentIdentifier;
using NodeGeomMap = DomNodeGeometry::NodeGeomMap;

constexpr std::string_view kDomNodeResultHistogram =
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

std::unique_ptr<DomNodeGeometry> DomNodeGeometry::InitFromApc(
    const AnnotatedPageContent& apc) {
  TRACE_EVENT("actor", "DomNodeGeometry::InitFromApc");
  if (!apc.has_main_frame_data() ||
      !apc.main_frame_data().has_document_identifier()) {
    return base::WrapUnique(
        new DomNodeGeometry(GetDomNodeResult::kNoApcMainFrameData));
  }
  auto map = BuildNodeMap(apc.main_frame_data().document_identifier(),
                          apc.root_node());
  return base::WrapUnique(new DomNodeGeometry(std::move(map)));
}

base::expected<gfx::Point, GetDomNodeResult> DomNodeGeometry::GetDomNode(
    const DomNode& node) const {
  TRACE_EVENT("actor", "DomNodeGeometry::GetDomNode");
  auto result = InternalGetDomNode(node);
  UmaHistogramEnumeration(kDomNodeResultHistogram,
                          result.error_or(GetDomNodeResult::kSuccess));
  return result;
}

base::expected<gfx::Point, GetDomNodeResult>
DomNodeGeometry::InternalGetDomNode(const DomNode& node) const {
  if (init_error_.has_value()) {
    return base::unexpected(init_error_.value());
  }
  auto it = node_map_.find(node);
  if (it == node_map_.end()) {
    return base::unexpected(GetDomNodeResult::kNodeNotFoundInApc);
  }
  const ContentAttributes& attr = it->second;
  if (!attr.has_geometry()) {
    return base::unexpected(GetDomNodeResult::kNoGeometry);
  }
  const auto& geom = attr.geometry();
  if (!geom.has_visible_bounding_box()) {
    return base::unexpected(GetDomNodeResult::kOffScreen);
  }
  const auto& rect = geom.visible_bounding_box();
  const int x = rect.x() + rect.width() / 2;
  const int y = rect.y() + rect.height() / 2;
  return gfx::Point(x, y);
}

DomNodeGeometry::DomNodeGeometry(GetDomNodeResult init_error)
    : init_error_(init_error), node_map_() {}

DomNodeGeometry::DomNodeGeometry(NodeGeomMap node_map)
    : init_error_(std::nullopt), node_map_(std::move(node_map)) {}

DomNodeGeometry::~DomNodeGeometry() = default;

}  // namespace actor::ui
