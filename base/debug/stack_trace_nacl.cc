// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/stack_trace.h"

namespace base {
namespace debug {

StackTrace::StackTrace() = default;
StackTrace::StackTrace(size_t count) : StackTrace() {}
StackTrace::StackTrace(span<const void* const> trace) : StackTrace() {}

void StackTrace::Print() const {}

void StackTrace::OutputToStream(std::ostream* os) const {}

std::string StackTrace::ToString() const {
  return {};
}

std::string StackTrace::ToStringWithPrefix(cstring_view prefix_string) const {
  return {};
}

std::ostream& operator<<(std::ostream& os, const StackTrace& s) {
  return os;
}

}  // namespace debug
}  // namespace base
