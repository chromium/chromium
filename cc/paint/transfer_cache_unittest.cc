// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <vector>

#include "base/command_line.h"
#include "build/build_config.h"
#include "cc/paint/image_transfer_cache_entry.h"
#include "cc/paint/raw_memory_transfer_cache_entry.h"
#include "cc/paint/transfer_cache_entry.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "components/viz/test/test_in_process_context_provider.h"
#include "gpu/command_buffer/client/client_transfer_cache.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/client/test_gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/service/service_transfer_cache.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/raster_in_process_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gl/gl_implementation.h"

namespace cc {
namespace {

class TransferCacheTest : public testing::Test {
 public:
  TransferCacheTest() : test_client_entry_(std::vector<uint8_t>(100)) {}

  void SetUp() override {
    gpu::ContextCreationAttribs attribs;
    attribs.fail_if_major_perf_caveat = false;
    attribs.bind_generates_resource = false;
    // Enable OOP rasterization.
    attribs.enable_oop_rasterization = true;
    attribs.enable_raster_interface = true;
    attribs.enable_gles2_interface = false;

    context_ = std::make_unique<gpu::RasterInProcessContext>();
    auto result = context_->Initialize(
        viz::TestGpuServiceHolder::GetInstance()->task_executor(), attribs,
        gpu::SharedMemoryLimits(), nullptr, nullptr);

    ASSERT_EQ(result, gpu::ContextResult::kSuccess);
    ASSERT_TRUE(context_->GetCapabilities().gpu_rasterization);
  }

  void TearDown() override { context_.reset(); }

  gpu::ServiceTransferCache* ServiceTransferCache() {
    return context_->GetTransferCacheForTest();
  }

  int decoder_id() { return context_->GetRasterDecoderIdForTest(); }

  gpu::raster::RasterInterface* ri() { return context_->GetImplementation(); }

  gpu::ContextSupport* ContextSupport() {
    return context_->GetContextSupport();
  }

  const ClientRawMemoryTransferCacheEntry& test_client_entry() const {
    return test_client_entry_;
  }
  void CreateEntry(const ClientTransferCacheEntry& entry) {
    auto* context_support = ContextSupport();
    uint32_t size = entry.SerializedSize();
    void* data = context_support->MapTransferCacheEntry(size);
    ASSERT_TRUE(data);
    entry.Serialize(base::make_span(static_cast<uint8_t*>(data), size));
    context_support->UnmapAndCreateTransferCacheEntry(entry.UnsafeType(),
                                                      entry.Id());
  }

 private:
  gpu::TestGpuMemoryBufferManager gpu_memory_buffer_manager_;
  std::unique_ptr<gpu::RasterInProcessContext> context_;
  gl::DisableNullDrawGLBindings enable_pixel_output_;
  ClientRawMemoryTransferCacheEntry test_client_entry_;
};

TEST_F(TransferCacheTest, Basic) {
  auto* service_cache = ServiceTransferCache();
  auto* context_support = ContextSupport();

  // Create an entry.
  const auto& entry = test_client_entry();
  CreateEntry(entry);
  ri()->Finish();

  // Validate service-side state.
  EXPECT_NE(nullptr,
            service_cache->GetEntry(gpu::ServiceTransferCache::EntryKey(
                decoder_id(), entry.Type(), entry.Id())));

  // Unlock on client side and flush to service.
  context_support->UnlockTransferCacheEntries(
      {{entry.UnsafeType(), entry.Id()}});
  ri()->Finish();

  // Re-lock on client side and validate state. No need to flush as lock is
  // local.
  EXPECT_TRUE(context_support->ThreadsafeLockTransferCacheEntry(
      entry.UnsafeType(), entry.Id()));

  // Delete on client side, flush, and validate that deletion reaches service.
  context_support->DeleteTransferCacheEntry(entry.UnsafeType(), entry.Id());
  ri()->Finish();
  EXPECT_EQ(nullptr,
            service_cache->GetEntry(gpu::ServiceTransferCache::EntryKey(
                decoder_id(), entry.Type(), entry.Id())));
}

TEST_F(TransferCacheTest, MemoryEviction) {
  auto* service_cache = ServiceTransferCache();
  auto* context_support = ContextSupport();

  const auto& entry = test_client_entry();
  // Create an entry.
  CreateEntry(entry);
  ri()->Finish();

  // Validate service-side state.
  EXPECT_NE(nullptr,
            service_cache->GetEntry(gpu::ServiceTransferCache::EntryKey(
                decoder_id(), entry.Type(), entry.Id())));

  // Unlock on client side and flush to service.
  context_support->UnlockTransferCacheEntries(
      {{entry.UnsafeType(), entry.Id()}});
  ri()->Finish();

  // Evict on the service side.
  service_cache->SetCacheSizeLimitForTesting(0);
  EXPECT_EQ(nullptr,
            service_cache->GetEntry(gpu::ServiceTransferCache::EntryKey(
                decoder_id(), entry.Type(), entry.Id())));

  // Try to re-lock on the client side. This should fail.
  EXPECT_FALSE(context_support->ThreadsafeLockTransferCacheEntry(
      entry.UnsafeType(), entry.Id()));
}

TEST_F(TransferCacheTest, CountEviction) {
  auto* service_cache = ServiceTransferCache();
  auto* context_support = ContextSupport();

  // Create 10 entries and leave them all unlocked.
  std::vector<std::unique_ptr<ClientRawMemoryTransferCacheEntry>> entries;
  for (int i = 0; i < 10; ++i) {
    entries.emplace_back(std::make_unique<ClientRawMemoryTransferCacheEntry>(
        std::vector<uint8_t>(4)));
    CreateEntry(*entries[i]);
    context_support->UnlockTransferCacheEntries(
        {{entries[i]->UnsafeType(), entries[i]->Id()}});
    ri()->Finish();
  }

  // These entries should be using up space.
  EXPECT_EQ(service_cache->cache_size_for_testing(), 40u);

  // Evict on the service side.
  service_cache->SetMaxCacheEntriesForTesting(5);

  // Half the entries should be evicted.
  EXPECT_EQ(service_cache->cache_size_for_testing(), 20u);
}

// This tests a size that is small enough that the transfer buffer is used
// inside of RasterImplementation::MapTransferCacheEntry.
TEST_F(TransferCacheTest, RawMemoryTransferSmall) {
  auto* service_cache = ServiceTransferCache();

  // Create an entry with some initialized data.
  std::vector<uint8_t> data;
  data.resize(100);
  for (size_t i = 0; i < data.size(); ++i) {
    data[i] = i;
  }

  // Add the entry to the transfer cache
  ClientRawMemoryTransferCacheEntry client_entry(data);
  CreateEntry(client_entry);
  ri()->Finish();

  // Validate service-side data matches.
  ServiceTransferCacheEntry* service_entry =
      service_cache->GetEntry(gpu::ServiceTransferCache::EntryKey(
          decoder_id(), client_entry.Type(), client_entry.Id()));
  EXPECT_EQ(service_entry->Type(), client_entry.Type());
  const std::vector<uint8_t> service_data =
      static_cast<ServiceRawMemoryTransferCacheEntry*>(service_entry)->data();
  EXPECT_EQ(data, service_data);
}

// This tests a size that is large enough that mapped memory is used inside
// of RasterImplementation::MapTransferCacheEntry.
TEST_F(TransferCacheTest, RawMemoryTransferLarge) {
  auto* service_cache = ServiceTransferCache();

  // Create an entry with some initialized data.
  std::vector<uint8_t> data;
  data.resize(1500);
  for (size_t i = 0; i < data.size(); ++i) {
    data[i] = i;
  }

  // Add the entry to the transfer cache
  ClientRawMemoryTransferCacheEntry client_entry(data);
  CreateEntry(client_entry);
  ri()->Finish();

  // Validate service-side data matches.
  ServiceTransferCacheEntry* service_entry =
      service_cache->GetEntry(gpu::ServiceTransferCache::EntryKey(
          decoder_id(), client_entry.Type(), client_entry.Id()));
  EXPECT_EQ(service_entry->Type(), client_entry.Type());
  const std::vector<uint8_t> service_data =
      static_cast<ServiceRawMemoryTransferCacheEntry*>(service_entry)->data();
  EXPECT_EQ(data, service_data);
}

}  // namespace
}  // namespace cc
