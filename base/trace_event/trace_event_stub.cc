// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>

#include "base/trace_event/trace_event_stub.h"

namespace base {
namespace trace_event {

ConvertableToTraceFormat::~ConvertableToTraceFormat() = default;

void TracedValue::AppendAsTraceFormat(std::string* out) const {}

MemoryDumpProvider::~MemoryDumpProvider() = default;

// static
constexpr const char* const MemoryDumpManager::kTraceCategory;

}  // namespace trace_event
}  // namespace base

namespace perfetto {

TracedDictionary TracedValue::WriteDictionary() && {
  return TracedDictionary();
}

TracedArray TracedValue::WriteArray() && {
  return TracedArray();
}

TracedArray TracedDictionary::AddArray(StaticString) {
  return TracedArray();
}

TracedArray TracedDictionary::AddArray(DynamicString) {
  return TracedArray();
}

TracedDictionary TracedDictionary::AddDictionary(StaticString) {
  return TracedDictionary();
}

TracedDictionary TracedDictionary::AddDictionary(DynamicString) {
  return TracedDictionary();
}

TracedArray TracedArray::AppendArray() {
  return TracedArray();
}

TracedDictionary TracedArray::AppendDictionary() {
  return TracedDictionary();
}

}  // namespace perfetto
