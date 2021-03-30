// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_SHARED_ELEMENT_ID_H_
#define CC_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_SHARED_ELEMENT_ID_H_

#include <stdint.h>

#include <tuple>

namespace cc {

struct DocumentTransitionSharedElementId {
  uint32_t document_tag = 0u;
  uint32_t element_index = 0u;

  bool operator==(const DocumentTransitionSharedElementId& other) const {
    return element_index == other.element_index &&
           document_tag == other.document_tag;
  }

  bool operator!=(const DocumentTransitionSharedElementId& other) const {
    return !(*this == other);
  }

  bool operator<(const DocumentTransitionSharedElementId& other) const {
    return std::tie(document_tag, element_index) <
           std::tie(other.document_tag, other.element_index);
  }

  bool valid() const { return document_tag != 0u; }
};

}  // namespace cc

#endif  // CC_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_SHARED_ELEMENT_ID_H_
