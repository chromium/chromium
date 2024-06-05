// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_allocator_dump_guid.h"

#include "base/format_macros.h"
#include "base/hash/sha1.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/stringprintf.h"

namespace base {
namespace trace_event {

namespace {

uint64_t HashString(const std::string& str) {
  SHA1Digest digest = SHA1Hash(base::as_byte_span(str));
  return base::U64FromLittleEndian(base::span(digest).first<8u>());
}

}  // namespace

MemoryAllocatorDumpGuid::MemoryAllocatorDumpGuid(uint64_t guid) : guid_(guid) {}

MemoryAllocatorDumpGuid::MemoryAllocatorDumpGuid()
    : MemoryAllocatorDumpGuid(0u) {
}

MemoryAllocatorDumpGuid::MemoryAllocatorDumpGuid(const std::string& guid_str)
    : MemoryAllocatorDumpGuid(HashString(guid_str)) {
}

std::string MemoryAllocatorDumpGuid::ToString() const {
  return StringPrintf("%" PRIx64, guid_);
}

}  // namespace trace_event
}  // namespace base
