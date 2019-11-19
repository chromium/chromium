// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/serialize_host_descriptions.h"

#include <map>
#include <unordered_set>
#include <utility>

#include "base/strings/string_piece.h"

namespace {

// Returns the serialization of |root|. It expects |children[x]| to be the
// vector of child nodes for all descendants |x| of |root|. The serialization
// consists of taking the |representation| value of each node, starting in
// leaves, and injecting children's representations into a ListValue under the
// key |child_key| in the parent's |representation|. This is desctructive to the
// representation stored with the nodes (which gets moved out of them).
base::DictionaryValue Serialize(
    base::StringPiece child_key,
    base::DictionaryValue* root,
    const std::map<base::DictionaryValue*, std::vector<base::DictionaryValue*>>&
        children) {
  auto children_list = std::make_unique<base::ListValue>();
  auto child_it = children.find(root);
  if (child_it != children.end()) {
    for (base::DictionaryValue* child : child_it->second) {
      children_list->base::Value::Append(Serialize(child_key, child, children));
    }
  }

  if (!children_list->empty())
    root->Set(child_key, std::move(children_list));
  return std::move(*root);
}

// Takes a vector of host description and converts it into:
// |children|: a map from a host's representation to representations of its
//             children,
// |roots|: a set of representations of hosts with no parents, and
// |representations|: a vector actually storing all those representations to
//                    which the rest just points.
void CreateDictionaryForest(
    std::vector<HostDescriptionNode> hosts,
    std::map<base::DictionaryValue*, std::vector<base::DictionaryValue*>>*
        children,
    std::unordered_set<base::DictionaryValue*>* roots,
    std::vector<base::DictionaryValue>* representations) {
  representations->reserve(hosts.size());
  children->clear();
  roots->clear();
  representations->clear();

  std::map<base::StringPiece, base::DictionaryValue*> name_to_representation;

  // First move the representations and map the names to them.
  for (HostDescriptionNode& node : hosts) {
    representations->push_back(std::move(node.representation));
    // If there are multiple nodes with the same name, subsequent insertions
    // will be ignored, so only the first node with a given name will be
    // referenced by |roots| and |children|.
    name_to_representation.emplace(node.name, &representations->back());
  }

  // Now compute children.
  for (HostDescriptionNode& node : hosts) {
    base::DictionaryValue* node_rep = name_to_representation[node.name];
    base::StringPiece parent_name = node.parent_name;
    if (parent_name.empty()) {
      roots->insert(node_rep);
      continue;
    }
    auto node_it = name_to_representation.find(parent_name);
    if (node_it == name_to_representation.end()) {
      roots->insert(node_rep);
      continue;
    }
    (*children)[name_to_representation[parent_name]].push_back(node_rep);
  }
}

}  // namespace

base::ListValue SerializeHostDescriptions(
    std::vector<HostDescriptionNode> hosts,
    base::StringPiece child_key) {
  // |representations| must outlive |children| and |roots|, which contain
  // pointers to objects in |representations|.
  std::vector<base::DictionaryValue> representations;
  std::map<base::DictionaryValue*, std::vector<base::DictionaryValue*>>
      children;
  std::unordered_set<base::DictionaryValue*> roots;

  CreateDictionaryForest(std::move(hosts), &children, &roots, &representations);

  base::ListValue list_value;
  for (auto* root : roots) {
    list_value.base::Value::Append(Serialize(child_key, root, children));
  }
  return list_value;
}
