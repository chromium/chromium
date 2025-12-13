// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_SHARED_TYPES_H_
#define CHROME_BROWSER_ACTOR_SHARED_TYPES_H_

#include <variant>

#include "chrome/common/actor.mojom-data-view.h"
#include "ui/gfx/geometry/point.h"

namespace actor {

using MouseClickType = mojom::ClickAction_Type;
using MouseClickCount = mojom::ClickAction_Count;

// PageTarget specifies a target in the page. This must be one of (mutually
// exclusive):
//   * A main-frame relative coordinate
//   * A specific node, specified by DOMNodeId and document identifier pair.
//     DOMNodeId can be the kRootElementDomNodeId special value to target the
//     viewport.
struct DomNode {
  int node_id;
  std::string document_identifier;

  bool operator==(const DomNode& other) const = default;

  template <typename H>
  friend H AbslHashValue(H h, const DomNode& node) {
    return H::combine(std::move(h), node.node_id, node.document_identifier);
  }
};

using PageTarget = std::variant<gfx::Point, DomNode>;

std::string DebugString(const MouseClickType& t);
std::string DebugString(const MouseClickCount& c);
std::string DebugString(const PageTarget& t);

std::ostream& operator<<(std::ostream& os, const PageTarget& t);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_SHARED_TYPES_H_
