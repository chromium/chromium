// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "cc/paint/paint_op_buffer.h"
#include "cc/paint/raw_memory_transfer_cache_entry.h"
#include "cc/test/transfer_cache_test_helper.h"
#include "components/viz/test/test_context_provider.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 4)
    return 0;

  scoped_refptr<viz::TestContextProvider> context_provider =
      viz::TestContextProvider::Create();
  context_provider->BindToCurrentSequence();

  cc::TransferCacheEntryType entry_type =
      static_cast<cc::TransferCacheEntryType>(data[0]);
  std::unique_ptr<cc::ServiceTransferCacheEntry> entry =
      cc::ServiceTransferCacheEntry::Create(entry_type);
  if (!entry)
    return 0;

  // Align data.
  base::span<const uint8_t> span(&data[4], size - 4);
  if (!entry->Deserialize(context_provider->GrContext(), span))
    return 0;

  // TODO(enne): consider running Serialize() here to fuzz that codepath
  // for bugs.  However, that requires setting up a real context with
  // a raster interface that supports gl operations and that has a
  // TransferCacheTestHelper in it so the innards of client and service
  // are accessible.
  return 0;
}
