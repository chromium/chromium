// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/record_replay/element_id.h"

#include <ostream>

namespace record_replay {

ElementId::ElementId(blink::LocalFrameToken frame_token, DomNodeId dom_node_id)
    : frame_token_(std::move(frame_token)), dom_node_id_(dom_node_id) {}

ElementId::ElementId(const ElementId&) = default;

ElementId& ElementId::operator=(const ElementId&) = default;

ElementId::~ElementId() = default;

bool operator==(const ElementId& lhs, const ElementId& rhs) = default;
auto operator<=>(const ElementId& lhs, const ElementId& rhs) = default;

std::ostream& operator<<(std::ostream& os, const ElementId& element_id) {
  return os << element_id.frame_token() << "_" << element_id.dom_node_id();
}

}  // namespace record_replay
