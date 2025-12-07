// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/shared_types.h"

#include <sstream>

#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace actor {

std::string DebugString(const MouseClickType& t) {
  std::ostringstream ss;
  ss << t;
  return ss.str();
}

std::string DebugString(const MouseClickCount& c) {
  std::ostringstream ss;
  ss << c;
  return ss.str();
}

std::string DebugString(const PageTarget& t) {
  if (std::holds_alternative<gfx::Point>(t)) {
    return std::get<gfx::Point>(t).ToString();
  } else if (std::holds_alternative<DomNode>(t)) {
    const DomNode& d = std::get<DomNode>(t);
    return absl::StrFormat("DomNode[id=%d doc_id=%s]", d.node_id,
                           d.document_identifier);
  }
  NOTREACHED();
}

std::ostream& operator<<(std::ostream& os, const PageTarget& t) {
  os << DebugString(t);
  return os;
}

}  // namespace actor
