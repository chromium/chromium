// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RECORD_REPLAY_ELEMENT_ID_H_
#define CHROME_BROWSER_RECORD_REPLAY_ELEMENT_ID_H_

#include <ostream>
#include <string>

#include "chrome/common/record_replay/aliases.h"

namespace record_replay {

// Uniquely identifies a DOM element in the browser process.
class ElementId {
 public:
  // Note: We use std::unique_ptr<ElementId> in collections to avoid slicing
  // when using derived classes like ContentElementId.
  explicit ElementId(DomNodeId dom_node_id);
  ElementId(const ElementId&);
  ElementId& operator=(const ElementId&);
  virtual ~ElementId();

  DomNodeId dom_node_id() const { return dom_node_id_; }

  virtual std::string ToString() const;

  friend bool operator==(const ElementId& lhs, const ElementId& rhs);
  friend auto operator<=>(const ElementId& lhs, const ElementId& rhs);

 private:
  DomNodeId dom_node_id_{};
};

std::ostream& operator<<(std::ostream& os, const ElementId& element_id);

}  // namespace record_replay

#endif  // CHROME_BROWSER_RECORD_REPLAY_ELEMENT_ID_H_
