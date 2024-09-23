// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted_memory.h"

#include <utility>

#include "base/check_op.h"
#include "base/memory/read_only_shared_memory_region.h"

namespace base {

bool RefCountedMemory::Equals(
    const scoped_refptr<RefCountedMemory>& other) const {
  return other && AsSpan() == other->AsSpan();
}

RefCountedMemory::RefCountedMemory() = default;
RefCountedMemory::~RefCountedMemory() = default;

RefCountedStaticMemory::RefCountedStaticMemory() = default;
RefCountedStaticMemory::~RefCountedStaticMemory() = default;

RefCountedStaticMemory::RefCountedStaticMemory(base::span<const uint8_t> bytes)
    : bytes_(bytes) {}

base::span<const uint8_t> RefCountedStaticMemory::AsSpan() const {
  return bytes_;
}

RefCountedBytes::RefCountedBytes() = default;
RefCountedBytes::~RefCountedBytes() = default;

RefCountedBytes::RefCountedBytes(std::vector<uint8_t> initializer)
    : bytes_(std::move(initializer)) {}

RefCountedBytes::RefCountedBytes(base::span<const uint8_t> initializer)
    : bytes_(initializer.begin(), initializer.end()) {}

RefCountedBytes::RefCountedBytes(size_t size) : bytes_(size, 0u) {}

scoped_refptr<RefCountedBytes> RefCountedBytes::TakeVector(
    std::vector<uint8_t>* to_destroy) {
  auto bytes = MakeRefCounted<RefCountedBytes>();
  bytes->bytes_.swap(*to_destroy);
  return bytes;
}

base::span<const uint8_t> RefCountedBytes::AsSpan() const {
  return bytes_;
}

RefCountedString::RefCountedString() = default;
RefCountedString::~RefCountedString() = default;

RefCountedString::RefCountedString(std::string str) : string_(std::move(str)) {}

base::span<const uint8_t> RefCountedString::AsSpan() const {
  return base::as_byte_span(string_);
}

RefCountedString16::RefCountedString16() = default;
RefCountedString16::~RefCountedString16() = default;

RefCountedString16::RefCountedString16(std::u16string str)
    : string_(std::move(str)) {}

base::span<const uint8_t> RefCountedString16::AsSpan() const {
  return base::as_byte_span(string_);
}

RefCountedSharedMemoryMapping::RefCountedSharedMemoryMapping(
    ReadOnlySharedMemoryMapping mapping)
    : mapping_(std::move(mapping)) {
  DCHECK_GT(mapping_.size(), 0u);
}

RefCountedSharedMemoryMapping::~RefCountedSharedMemoryMapping() = default;

base::span<const uint8_t> RefCountedSharedMemoryMapping::AsSpan() const {
  return mapping_.GetMemoryAsSpan<const uint8_t>();
}

// static
scoped_refptr<RefCountedSharedMemoryMapping>
RefCountedSharedMemoryMapping::CreateFromWholeRegion(
    const ReadOnlySharedMemoryRegion& region) {
  ReadOnlySharedMemoryMapping mapping = region.Map();
  if (!mapping.IsValid())
    return nullptr;
  return MakeRefCounted<RefCountedSharedMemoryMapping>(std::move(mapping));
}

}  //  namespace base
