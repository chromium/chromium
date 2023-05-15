// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/bits.h"
#include "cc/paint/raw_memory_transfer_cache_entry.h"
#include "cc/test/transfer_cache_test_helper.h"
#include "components/viz/test/test_context_provider.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"

// TODO(crbug.com/1442381): Implement fuzzer with Skia Graphite backend.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Align data. ImageTransferCacheEntry requires 16-byte.
  const uint8_t* aligned_data = base::bits::AlignUp(data, 16);
  size_t alignment_gap = aligned_data - data;
  if (size < alignment_gap + 4) {
    return 0;
  }

  scoped_refptr<viz::TestContextProvider> context_provider =
      viz::TestContextProvider::Create();
  context_provider->BindToCurrentSequence();

  cc::TransferCacheEntryType entry_type =
      static_cast<cc::TransferCacheEntryType>(data[0]);
  std::unique_ptr<cc::ServiceTransferCacheEntry> entry =
      cc::ServiceTransferCacheEntry::Create(entry_type);
  if (!entry) {
    return 0;
  }

  base::span<const uint8_t> span(aligned_data, size - alignment_gap);
  if (!entry->Deserialize(context_provider->GrContext(),
                          /*graphite_recorder=*/nullptr, span)) {
    return 0;
  }

  // TODO(enne): consider running Serialize() here to fuzz that codepath
  // for bugs.  However, that requires setting up a real context with
  // a raster interface that supports gl operations and that has a
  // TransferCacheTestHelper in it so the innards of client and service
  // are accessible.
  return 0;
}
