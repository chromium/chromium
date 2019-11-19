// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_TRANSFER_CACHE_ENTRY_H_
#define CC_PAINT_TRANSFER_CACHE_ENTRY_H_

#include <memory>

#include "base/containers/span.h"
#include "cc/paint/paint_export.h"

class GrContext;

namespace cc {

// To add a new transfer cache entry type:
//  - Add a type name to the TransferCacheEntryType enum.
//  - Implement a ClientTransferCacheEntry and ServiceTransferCacheEntry for
//    your new type.
//  - Update ServiceTransferCacheEntry::Create in transfer_cache_entry.cc.
enum class TransferCacheEntryType : uint32_t {
  kRawMemory,
  kImage,
  kShader,
  // Add new entries above this line, make sure to update kLast.
  kLast = kShader,
};

// An interface used on the client to serialize a transfer cache entry
// into raw bytes that can be sent to the service.
class CC_PAINT_EXPORT ClientTransferCacheEntry {
 public:
  virtual ~ClientTransferCacheEntry() {}

  // Returns the type of this entry. Combined with id, it should form a unique
  // identifier.
  virtual TransferCacheEntryType Type() const = 0;

  // Returns the id of this entry. Combined with type, it should form a unique
  // identifier.
  virtual uint32_t Id() const = 0;

  // Returns the serialized sized of this entry in bytes. This function will be
  // used to determine how much memory is going to be allocated and passed to
  // the Serialize() call.
  virtual uint32_t SerializedSize() const = 0;

  // Serializes the entry into the given span of memory. The size of the span is
  // guaranteed to be at least SerializedSize() bytes. Returns true on success
  // and false otherwise.
  virtual bool Serialize(base::span<uint8_t> data) const = 0;

  // Returns the same value as Type() but as a uint32_t to use via
  // ContextSupport.
  uint32_t UnsafeType() const { return static_cast<uint32_t>(Type()); }
};

// An interface which receives the raw data sent by the client and
// deserializes it into the appropriate service-side object.
class CC_PAINT_EXPORT ServiceTransferCacheEntry {
 public:
  static std::unique_ptr<ServiceTransferCacheEntry> Create(
      TransferCacheEntryType type);

  // Checks that |raw_type| represents a valid TransferCacheEntryType and
  // populates |type|. If |raw_type| is not valid, the function returns false
  // and |type| is not modified.
  static bool SafeConvertToType(uint32_t raw_type,
                                TransferCacheEntryType* type);

  // Returns true if the entry needs a GrContext during deserialization.
  static bool UsesGrContext(TransferCacheEntryType type);

  virtual ~ServiceTransferCacheEntry() {}

  // Returns the type of this entry.
  virtual TransferCacheEntryType Type() const = 0;

  // Returns the cached size of this entry. This value is used for memory
  // bookkeeping and to determine whether an unlocked cache entry will be
  // evicted.
  virtual size_t CachedSize() const = 0;

  // Deserialize the cache entry from the given span of memory with the given
  // context.
  virtual bool Deserialize(GrContext* context,
                           base::span<const uint8_t> data) = 0;
};

// Helpers to simplify subclassing.
template <class Base, TransferCacheEntryType EntryType>
class TransferCacheEntryBase : public Base {
 public:
  static constexpr TransferCacheEntryType kType = EntryType;
  TransferCacheEntryType Type() const final { return kType; }
};

template <TransferCacheEntryType EntryType>
using ClientTransferCacheEntryBase =
    TransferCacheEntryBase<ClientTransferCacheEntry, EntryType>;

template <TransferCacheEntryType EntryType>
using ServiceTransferCacheEntryBase =
    TransferCacheEntryBase<ServiceTransferCacheEntry, EntryType>;

}  // namespace cc

#endif  // CC_PAINT_TRANSFER_CACHE_ENTRY_H_
