// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/serialize_host_descriptions.h"

#include <map>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace {

// Returns the serialization of |root|. It expects |children[x]| to be the
// vector of child nodes for all descendants |x| of |root|. The serialization
// consists of taking the |representation| value of each node, starting in
// leaves, and injecting children's representations into a list under the
// key |child_key| in the parent's |representation|. This is destructive to the
// representation stored with the nodes (which gets moved out of them).
base::Value Serialize(
    std::string_view child_key,
    base::Value* root,
    const std::map<base::Value*, std::vector<base::Value*>>& children) {
  base::Value::List children_list;
  auto child_it = children.find(root);
  if (child_it != children.end()) {
    for (base::Value* child : child_it->second) {
      children_list.Append(Serialize(child_key, child, children));
    }
  }

  if (!children_list.empty())
    root->GetDict().Set(child_key, std::move(children_list));
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
    std::map<base::Value*, std::vector<base::Value*>>* children,
    std::unordered_set<base::Value*>* roots,
    base::Value::List* representations) {
  representations->reserve(hosts.size());

  std::map<std::string_view, base::Value*> name_to_representation;

  // First move the representations and map the names to them.
  for (HostDescriptionNode& node : hosts) {
    representations->Append(std::move(node.representation));
    // If there are multiple nodes with the same name, subsequent insertions
    // will be ignored, so only the first node with a given name will be
    // referenced by |roots| and |children|.
    name_to_representation.emplace(node.name, &representations->back());
  }

  // Now compute children.
  for (HostDescriptionNode& node : hosts) {
    base::Value* node_rep = name_to_representation[node.name];
    std::string_view parent_name = node.parent_name;
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

base::Value::List SerializeHostDescriptions(
    std::vector<HostDescriptionNode> hosts,
    std::string_view child_key) {
  // |representations| must outlive |children| and |roots|, which contain
  // pointers to objects in |representations|.
  base::Value::List representations;
  std::map<base::Value*, std::vector<base::Value*>> children;
  std::unordered_set<base::Value*> roots;

  CreateDictionaryForest(std::move(hosts), &children, &roots, &representations);

  base::Value::List result;
  result.reserve(roots.size());
  for (auto* root : roots) {
    result.Append(Serialize(child_key, root, children));
  }
  return result;
}
