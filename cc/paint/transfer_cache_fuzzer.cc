// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include "base/bits.h"
#include "cc/paint/raw_memory_transfer_cache_entry.h"
#include "cc/test/transfer_cache_test_helper.h"
#include "components/viz/test/test_context_provider.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"

struct Environment {
  Environment() { logging::SetMinLogLevel(logging::LOGGING_FATAL); }
};

// TODO(crbug.com/40266937): Implement fuzzer with Skia Graphite backend.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  // Size required for fuzzing metadata.
  if (size < 2) {
    return 0;
  }

  scoped_refptr<viz::TestContextProvider> context_provider =
      viz::TestContextProvider::CreateRaster();
  context_provider->BindToCurrentSequence();

  cc::TransferCacheEntryType entry_type =
      static_cast<cc::TransferCacheEntryType>(data[0]);
  std::unique_ptr<cc::ServiceTransferCacheEntry> entry =
      cc::ServiceTransferCacheEntry::Create(entry_type);
  if (!entry) {
    return 0;
  }

#if DCHECK_IS_ON()
  // Align data on debug builds. ImageTransferCacheEntry requires 16-byte
  // alignment on debug builds. Note: Consume one byte becauase the first
  // byte was used for `TransferCacheEntryType`
  const uint8_t* aligned_data = base::bits::AlignUp(&data[1], 16);
  size_t alignment_gap = aligned_data - data;
  if (size < alignment_gap) {
    return 0;
  }
  base::span<const uint8_t> span(aligned_data, size - alignment_gap);
#else
  // Support memory backing to discover bugs in release builds that require
  // unaligned memory.
  size_t offset = data[1] % 16;
  const uint8_t* unaligned_data = &data[2] + offset;
  size_t unaligned_gap = unaligned_data - data;
  if (size < unaligned_gap) {
    return 0;
  }
  base::span<const uint8_t> span(unaligned_data, size - unaligned_gap);
#endif

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
