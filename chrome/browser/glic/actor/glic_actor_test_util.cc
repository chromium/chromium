// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/actor/glic_actor_test_util.h"

#include <string>
#include <string_view>

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/actor/core/task_source_info.h"

namespace glic {

namespace {

// Helper to extract text from ContentNode subtree.
void GetSubtreeText(const optimization_guide::proto::ContentNode& node,
                    std::string* out) {
  if (node.content_attributes().has_text_data()) {
    if (!out->empty() && out->back() != ' ') {
      *out += " ";
    }
    *out += node.content_attributes().text_data().text_content();
  }
  for (const optimization_guide::proto::ContentNode& child :
       node.children_nodes()) {
    GetSubtreeText(child, out);
  }
}

// Helper to recursively find form labels and map them to DomNodes.
void FindFormLabelsRecursively(
    const optimization_guide::proto::ContentNode& node,
    const std::string& document_identifier,
    base::flat_map<std::string, ::actor::DomNode>* label_map) {
  const optimization_guide::proto::ContentAttributes& attrs =
      node.content_attributes();

  if (attrs.has_label_for_dom_node_id()) {
    std::string text;
    GetSubtreeText(node, &text);
    text = base::TrimWhitespaceASCII(text, base::TRIM_ALL);
    if (!text.empty()) {
      CHECK(!label_map->contains(text)) << "Test pages must not repeat labels";
      (*label_map)[text] =
          ::actor::DomNode{.node_id = attrs.label_for_dom_node_id(),
                           .document_identifier = document_identifier};
    }
  }

  if (attrs.attribute_type() ==
      optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME) {
    return;
  }

  for (const optimization_guide::proto::ContentNode& child :
       node.children_nodes()) {
    FindFormLabelsRecursively(child, document_identifier, label_map);
  }
}

}  // namespace

const actor::TaskSourceInfo& MockGlicTaskSourceInfo() {
  constexpr std::string_view kMockConversationId = "123456abcdef";
  static base::NoDestructor<actor::TaskSourceInfo> task_source_info(
      actor::TaskSourceInfo::Client::kGlic, std::string(kMockConversationId));
  return *task_source_info.get();
}

base::flat_map<std::string, ::actor::DomNode> BuildFormLabelsMap(
    const ::optimization_guide::proto::AnnotatedPageContent& apc) {
  base::flat_map<std::string, ::actor::DomNode> label_map;
  CHECK(apc.has_root_node());
  CHECK(apc.has_main_frame_data());
  FindFormLabelsRecursively(
      apc.root_node(),
      apc.main_frame_data().document_identifier().serialized_token(),
      &label_map);
  return label_map;
}

std::string FormLabelsDebugString(
    const base::flat_map<std::string, ::actor::DomNode>& map) {
  std::string out = "Form labels map:\n";
  for (const auto& [label, node] : map) {
    base::StringAppendF(&out, "  '%s' -> {node_id: %d, doc: %s}\n",
                        label.c_str(), node.node_id,
                        node.document_identifier.c_str());
  }
  return out;
}

}  // namespace glic
