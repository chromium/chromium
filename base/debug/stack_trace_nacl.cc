// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/stack_trace.h"

namespace base {
namespace debug {

StackTrace::StackTrace() = default;
StackTrace::StackTrace(size_t count) : StackTrace() {}
StackTrace::StackTrace(const void* const* trace, size_t count) : StackTrace() {}

const void* const* StackTrace::Addresses(size_t* count) const {
  return nullptr;
}

void StackTrace::Print() const {}

void StackTrace::OutputToStream(std::ostream* os) const {}

std::string StackTrace::ToString() const {
  return "";
}

std::string StackTrace::ToStringWithPrefix(const char* prefix_string) const {
  return "";
}

std::ostream& operator<<(std::ostream& os, const StackTrace& s) {
  return os;
}

}  // namespace debug
}  // namespace base
