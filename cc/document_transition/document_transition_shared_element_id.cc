// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/document_transition/document_transition_shared_element_id.h"

#include <sstream>
#include <string>

#include "base/check_op.h"
#include "base/containers/flat_set.h"

namespace cc {

DocumentTransitionSharedElementId::DocumentTransitionSharedElementId() =
    default;

DocumentTransitionSharedElementId::DocumentTransitionSharedElementId(
    uint32_t document_tag)
    : document_tag_(document_tag) {}

DocumentTransitionSharedElementId::DocumentTransitionSharedElementId(
    DocumentTransitionSharedElementId&&) = default;

DocumentTransitionSharedElementId::DocumentTransitionSharedElementId(
    const DocumentTransitionSharedElementId&) = default;

DocumentTransitionSharedElementId::~DocumentTransitionSharedElementId() =
    default;

void DocumentTransitionSharedElementId::AddIndex(uint32_t index) {
  DCHECK_NE(document_tag_, 0u);
  element_indices_.insert(index);
}

bool DocumentTransitionSharedElementId::Matches(uint32_t document_tag,
                                                uint32_t index) const {
  return document_tag_ == document_tag && element_indices_.count(index) != 0;
}

std::string DocumentTransitionSharedElementId::ToString() const {
  std::ostringstream str;
  str << "DocumentTransitionSharedElementId{ document_tag: " << document_tag_
      << " element_indices: [";
  std::string separator = "";
  for (auto index : element_indices_) {
    str << separator << index;
    separator = ", ";
  }
  str << "]}";
  return str.str();
}

}  // namespace cc
