// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/resource_pool.h"

#include <stddef.h>

#include <limits>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_context_support.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class ResourcePoolTest : public testing::Test {
 public:
  void SetUp() override {
    auto context_support = std::make_unique<MockContextSupport>();
    context_support_ = context_support.get();
    context_provider_ =
        viz::TestContextProvider::CreateRaster(std::move(context_support));
    context_provider_->BindToCurrentSequence();
    resource_provider_ = std::make_unique<viz::ClientResourceProvider>();
    test_task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    resource_pool_ = std::make_unique<ResourcePool>(
        resource_provider_.get(), context_provider_.get(), test_task_runner_,
        ResourcePool::kDefaultExpirationDelay, false);
    resource_pool_->SetClockForTesting(test_task_runner_->GetMockTickClock());
  }

  void TearDown() override {
    resource_provider_->ShutdownAndReleaseAllResources();
  }

 protected:
  class MockContextSupport : public viz::TestContextSupport {
   public:
    MockContextSupport() = default;
    MOCK_METHOD0(FlushPendingWork, void());
  };

  class StubGpuBacking : public ResourcePool::GpuBacking {
   public:
    void OnMemoryDump(
        base::trace_event::ProcessMemoryDump* pmd,
        const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
        uint64_t tracing_process_id,
        int importance) const override {}
  };

  void SetBackingOnResource(const ResourcePool::InUsePoolResource& resource) {
    auto backing = std::make_unique<StubGpuBacking>();
    backing->shared_image = gpu::ClientSharedImage::CreateForTesting();
    backing->mailbox_sync_token.Set(
        gpu::GPU_IO, gpu::CommandBufferId::FromUnsafeValue(1), 1);
    resource.set_gpu_backing(std::move(backing));
  }

  void CheckAndReturnResource(ResourcePool::InUsePoolResource resource) {
    EXPECT_TRUE(!!resource);
    resource_pool_->ReleaseResource(std::move(resource));
  }

  scoped_refptr<viz::TestContextProvider> context_provider_;
  raw_ptr<MockContextSupport> context_support_;
  std::unique_ptr<viz::ClientResourceProvider> resource_provider_;
  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;
  std::unique_ptr<ResourcePool> resource_pool_;
};

TEST_F(ResourcePoolTest, AcquireRelease) {
  gfx::Size size(100, 100);
  viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  ResourcePool::InUsePoolResource resource =
      resource_pool_->AcquireResource(size, format, color_space);
  EXPECT_EQ(size, resource.size());
  EXPECT_EQ(format, resource.format());
  EXPECT_EQ(color_space, resource.color_space());

  resource_pool_->ReleaseResource(std::move(resource));
}

TEST_F(ResourcePoolTest, EventuallyEvictAndFlush) {
  gfx::Size size(100, 100);
  viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  ResourcePool::InUsePoolResource resource =
      resource_pool_->AcquireResource(size, format, color_space);
  resource_pool_->ReleaseResource(std::move(resource));
  EXPECT_EQ(1u, resource_pool_->GetTotalResourceCountForTesting());

  // Expect flush after eviction and flush delay.
  EXPECT_CALL(*context_support_, FlushPendingWork()).Times(testing::AtLeast(1));
  test_task_runner_->FastForwardBy(ResourcePool::kDefaultExpirationDelay +
                                   ResourcePool::kDefaultMaxFlushDelay);
  EXPECT_EQ(0u, resource_pool_->GetTotalResourceCountForTesting());
}

TEST_F(ResourcePoolTest, FlushEvenIfMoreUnusedToEvict) {
  gfx::Size size(100, 100);
  viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  ResourcePool::InUsePoolResource resource1 =
      resource_pool_->AcquireResource(size, format, color_space);
  ResourcePool::InUsePoolResource resource2 =
      resource_pool_->AcquireResource(size, format, color_space);

  // Time 0: No resources evicted yet.
  EXPECT_EQ(2u, resource_pool_->GetTotalResourceCountForTesting());

  // Space the resource last_usage out so that they don't expire at the same
  // time. resource1 last used at time 0 (expires kDefaultExpirationDelay) and
  // resource2 last used at last_usage_gap (expires kDefaultExpireationDelay +
  // last_usage_gap).
  const base::TimeDelta last_usage_gap =
      ResourcePool::kDefaultMaxFlushDelay * 2;
  resource_pool_->ReleaseResource(std::move(resource1));
  test_task_runner_->FastForwardBy(last_usage_gap);
  resource_pool_->ReleaseResource(std::move(resource2));

  // Time |last_usage_gap|: No resources evicted yet.
  EXPECT_EQ(2u, resource_pool_->GetTotalResourceCountForTesting());

  // Time |kDefaultExpirationDelay|: resource1 evicted, but not resource2 yet.
  test_task_runner_->FastForwardBy(ResourcePool::kDefaultExpirationDelay -
                                   last_usage_gap);
  EXPECT_EQ(1u, resource_pool_->GetTotalResourceCountForTesting());

  // Expect at least one flush kDefaultMaxFlushDelay after an eviction.
  EXPECT_CALL(*context_support_, FlushPendingWork()).Times(testing::AtLeast(1));
  test_task_runner_->FastForwardBy(ResourcePool::kDefaultMaxFlushDelay);

  // Time |kDefaultExpirationDelay + kDefaultMaxFlushDelay|:
  // Check that flush was called and resource2 still not evicted.
  testing::Mock::VerifyAndClearExpectations(context_support_);
  EXPECT_EQ(1u, resource_pool_->GetTotalResourceCountForTesting());

  // Wait a long time and resource2 should get evicted and flushed.
  EXPECT_CALL(*context_support_, FlushPendingWork()).Times(testing::AtLeast(1));
  test_task_runner_->FastForwardBy(ResourcePool::kDefaultExpirationDelay * 100);
  EXPECT_EQ(0u, resource_pool_->GetTotalResourceCountForTesting());
}

TEST_F(ResourcePoolTest, AccountingSingleResource) {
  // Limits high enough to not be hit by this test.
  size_t bytes_limit = 10 * 1024 * 1024;
  size_t count_limit = 100;
  resource_pool_->SetResourceUsageLimits(bytes_limit, count_limit);

  gfx::Size size(100, 100);
  viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  size_t resource_bytes = format.EstimatedSizeInBytes(size);
  ResourcePool::InUsePoolResource resource =
      resource_pool_->AcquireResource(size, format, color_space);
  SetBackingOnResource(resource);

  EXPECT_EQ(resource_bytes, resource_pool_->GetTotalMemoryUsageForTesting());
  EXPECT_EQ(resource_bytes, resource_pool_->memory_usage_bytes());
  EXPECT_EQ(1u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(1u, resource_pool_->resource_count());
  EXPECT_EQ(0u, resource_pool_->GetBusyResourceCountForTesting());

  resource_pool_->ReleaseResource(std::move(resource));
  EXPECT_EQ(resource_bytes, resource_pool_->GetTotalMemoryUsageForTesting());
  EXPECT_EQ(0u, resource_pool_->memory_usage_bytes());
  EXPECT_EQ(1u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(0u, resource_pool_->resource_count());
  EXPECT_EQ(0u, resource_pool_->GetBusyResourceCountForTesting());

  resource_pool_->SetResourceUsageLimits(0u, 0u);
  resource_pool_->ReduceResourceUsage();
  EXPECT_EQ(0u, resource_pool_->GetTotalMemoryUsageForTesting());
  EXPECT_EQ(0u, resource_pool_->memory_usage_bytes());
  EXPECT_EQ(0u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(0u, resource_pool_->resource_count());
  EXPECT_EQ(0u, resource_pool_->GetBusyResourceCountForTesting());
}

TEST_F(ResourcePoolTest, SimpleResourceReuse) {
  // Limits high enough to not be hit by this test.
  size_t bytes_limit = 10 * 1024 * 1024;
  size_t count_limit = 100;
  resource_pool_->SetResourceUsageLimits(bytes_limit, count_limit);

  gfx::Size size(100, 100);
  viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::ColorSpace color_space1;
  gfx::ColorSpace color_space2 = gfx::ColorSpace::CreateSRGB();

  CheckAndReturnResource(
      resource_pool_->AcquireResource(size, format, color_space1));
  EXPECT_EQ(1u, resource_pool_->GetTotalResourceCountForTesting());

  // Same size/format should re-use resource.
  ResourcePool::InUsePoolResource resource =
      resource_pool_->AcquireResource(size, format, color_space1);
  EXPECT_EQ(1u, resource_pool_->GetTotalResourceCountForTesting());
  CheckAndReturnResource(std::move(resource));
  EXPECT_EQ(1u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(0u, resource_pool_->GetBusyResourceCountForTesting());

  // Different size/format should allocate new resource.
  resource = resource_pool_->AcquireResource(
      gfx::Size(50, 50), viz::SinglePlaneFormat::kLUMINANCE_8, color_space1);
  EXPECT_EQ(2u, resource_pool_->GetTotalResourceCountForTesting());
  CheckAndReturnResource(std::move(resource));
  EXPECT_EQ(2u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(0u, resource_pool_->GetBusyResourceCountForTesting());

  // Different color space should allocate new resource.
  resource = resource_pool_->AcquireResource(size, format, color_space2);
  EXPECT_EQ(3u, resource_pool_->GetTotalResourceCountForTesting());
  CheckAndReturnResource(std::move(resource));
  EXPECT_EQ(3u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(0u, resource_pool_->GetBusyResourceCountForTesting());
}

TEST_F(ResourcePoolTest, LostResource) {
  // Limits high enough to not be hit by this test.
  size_t bytes_limit = 10 * 1024 * 1024;
  size_t count_limit = 100;
  resource_pool_->SetResourceUsageLimits(bytes_limit, count_limit);

  gfx::Size size(100, 100);
  viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();

  ResourcePool::InUsePoolResource resource =
      resource_pool_->AcquireResource(size, format, color_space);

  SetBackingOnResource(resource);
  EXPECT_TRUE(resource_pool_->PrepareForExport(
      resource, viz::TransferableResource::ResourceSource::kTest));

  std::vector<viz::ResourceId> export_ids = {resource.resource_id_for_export()};
  std::vector<viz::TransferableResource> transferable_resources;
  resource_provider_->PrepareSendToParent(
      export_ids, &transferable_resources,
      static_cast<viz::RasterContextProvider*>(context_provider_.get()));
  auto returned_resources =
      viz::TransferableResource::ReturnResources(transferable_resources);
  ASSERT_EQ(1u, returned_resources.size());
  returned_resources[0].lost = true;
  resource_provider_->ReceiveReturnsFromParent(std::move(returned_resources));

  EXPECT_EQ(1u, resource_pool_->GetTotalResourceCountForTesting());
  resource_pool_->ReleaseResource(std::move(resource));
  EXPECT_EQ(0u, resource_pool_->GetTotalResourceCountForTesting());
}

TEST_F(ResourcePoolTest, BusyResourcesNotFreed) {
  // Limits high enough to not be hit by this test.
  size_t bytes_limit = 10 * 1024 * 1024;
  size_t count_limit = 100;
  resource_pool_->SetResourceUsageLimits(bytes_limit, count_limit);

  gfx::Size size(100, 100);
  viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::ColorSpace color_space;

  ResourcePool::InUsePoolResource resource =
      resource_pool_->AcquireResource(size, format, color_space);
  SetBackingOnResource(resource);

  EXPECT_EQ(40000u, resource_pool_->GetTotalMemoryUsageForTesting());
  EXPECT_EQ(1u, resource_pool_->resource_count());

  EXPECT_TRUE(resource_pool_->PrepareForExport(
      resource, viz::TransferableResource::ResourceSource::kTest));

  std::vector<viz::TransferableResource> transfers;
  resource_provider_->PrepareSendToParent(
      {resource.resource_id_for_export()}, &transfers,
      static_cast<viz::RasterContextProvider*>(context_provider_.get()));

  resource_pool_->ReleaseResource(std::move(resource));
  EXPECT_EQ(40000u, resource_pool_->GetTotalMemoryUsageForTesting());
  EXPECT_EQ(40000u, resource_pool_->memory_usage_bytes());
  EXPECT_EQ(1u, resource_pool_->GetBusyResourceCountForTesting());

  // Wait for our resource pool to evict resources. Wait 10x the expiration
  // delay.
  test_task_runner_->FastForwardBy(ResourcePool::kDefaultExpirationDelay * 10);

  // Busy resources are still held, since they may be in flight to the display
  // compositor and should not be freed.
  EXPECT_EQ(40000u, resource_pool_->GetTotalMemoryUsageForTesting());
  EXPECT_EQ(40000u, resource_pool_->memory_usage_bytes());
  EXPECT_EQ(1u, resource_pool_->GetBusyResourceCountForTesting());

  resource_provider_->ReleaseAllExportedResources(/*lose=*/false);

  EXPECT_EQ(40000u, resource_pool_->GetTotalMemoryUsageForTesting());
  EXPECT_EQ(0u, resource_pool_->memory_usage_bytes());
  EXPECT_EQ(0u, resource_pool_->GetBusyResourceCountForTesting());
}

TEST_F(ResourcePoolTest, UnusedResourcesEventuallyFreed) {
  // Limits high enough to not be hit by this test.
  size_t bytes_limit = 10 * 1024 * 1024;
  size_t count_limit = 100;
  resource_pool_->SetResourceUsageLimits(bytes_limit, count_limit);

  gfx::Size size(100, 100);
  viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::ColorSpace color_space;

  ResourcePool::InUsePoolResource resource =
      resource_pool_->AcquireResource(size, format, color_space);
  SetBackingOnResource(resource);
  EXPECT_EQ(40000u, resource_pool_->GetTotalMemoryUsageForTesting());
  EXPECT_EQ(1u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(1u, resource_pool_->resource_count());
  EXPECT_EQ(0u, resource_pool_->GetBusyResourceCountForTesting());

  // Export the resource to the display compositor.
  EXPECT_TRUE(resource_pool_->PrepareForExport(
      resource, viz::TransferableResource::ResourceSource::kTest));
  std::vector<viz::TransferableResource> transfers;
  resource_provider_->PrepareSendToParent(
      {resource.resource_id_for_export()}, &transfers,
      static_cast<viz::RasterContextProvider*>(context_provider_.get()));

  resource_pool_->ReleaseResource(std::move(resource));
  EXPECT_EQ(40000u, resource_pool_->GetTotalMemoryUsageForTesting());
  EXPECT_EQ(1u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(0u, resource_pool_->resource_count());
  EXPECT_EQ(1u, resource_pool_->GetBusyResourceCountForTesting());

  // Transfer the resource from the busy pool to the unused pool.
  resource_provider_->ReceiveReturnsFromParent(
      viz::TransferableResource::ReturnResources(transfers));
  EXPECT_EQ(40000u, resource_pool_->GetTotalMemoryUsageForTesting());
  EXPECT_EQ(1u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(0u, resource_pool_->resource_count());
  EXPECT_EQ(0u, resource_pool_->GetBusyResourceCountForTesting());

  // Wait for our resource pool to evict resources. Wait 10x the expiration
  // delay.
  test_task_runner_->FastForwardBy(ResourcePool::kDefaultExpirationDelay * 10);

  EXPECT_EQ(0u, resource_pool_->GetTotalMemoryUsageForTesting());
}

TEST_F(ResourcePoolTest, UpdateContentId) {
  gfx::Size size(100, 100);
  viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::ColorSpace color_space;
  uint64_t content_id = 42;
  uint64_t new_content_id = 43;
  gfx::Rect new_invalidated_rect(20, 20, 10, 10);

  ResourcePool::InUsePoolResource resource =
      resource_pool_->AcquireResource(size, format, color_space);
  resource_pool_->OnContentReplaced(resource, content_id);
  auto original_id = resource.unique_id_for_testing();
  resource_pool_->ReleaseResource(std::move(resource));

  // Ensure that we can retrieve the resource based on |content_id|.
  gfx::Rect invalidated_rect;
  ResourcePool::InUsePoolResource reacquired_resource =
      resource_pool_->TryAcquireResourceForPartialRaster(
          new_content_id, new_invalidated_rect, content_id, &invalidated_rect,
          color_space);
  EXPECT_EQ(original_id, reacquired_resource.unique_id_for_testing());
  EXPECT_EQ(new_invalidated_rect, invalidated_rect);
  resource_pool_->ReleaseResource(std::move(reacquired_resource));
}

TEST_F(ResourcePoolTest, UpdateContentIdAndInvalidatedRect) {
  gfx::Size size(100, 100);
  viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::ColorSpace color_space;
  uint64_t content_ids[] = {42, 43, 44};
  gfx::Rect invalidated_rect(20, 20, 10, 10);
  gfx::Rect second_invalidated_rect(25, 25, 10, 10);
  gfx::Rect expected_total_invalidated_rect(20, 20, 15, 15);

  // Acquire a new resource with the first content id.
  ResourcePool::InUsePoolResource resource =
      resource_pool_->AcquireResource(size, format, color_space);
  resource_pool_->OnContentReplaced(resource, content_ids[0]);
  auto original_id = resource.unique_id_for_testing();

  // Attempt to acquire this resource. It is in use, so its ID and invalidated
  // rect should be updated, but a new resource will be returned.
  gfx::Rect new_invalidated_rect;
  ResourcePool::InUsePoolResource reacquired_resource =
      resource_pool_->TryAcquireResourceForPartialRaster(
          content_ids[1], invalidated_rect, content_ids[0],
          &new_invalidated_rect, color_space);
  EXPECT_FALSE(!!reacquired_resource);
  EXPECT_EQ(gfx::Rect(), new_invalidated_rect);

  // Release the original resource, returning it to the unused pool.
  resource_pool_->ReleaseResource(std::move(resource));

  // Ensure that we cannot retrieve a resource based on the original content id.
  reacquired_resource = resource_pool_->TryAcquireResourceForPartialRaster(
      content_ids[1], invalidated_rect, content_ids[0], &new_invalidated_rect,
      color_space);
  EXPECT_FALSE(!!reacquired_resource);
  EXPECT_EQ(gfx::Rect(), new_invalidated_rect);

  // Ensure that we can retrieve the resource based on the second (updated)
  // content ID and that it has the expected invalidated rect.
  gfx::Rect total_invalidated_rect;
  reacquired_resource = resource_pool_->TryAcquireResourceForPartialRaster(
      content_ids[2], second_invalidated_rect, content_ids[1],
      &total_invalidated_rect, color_space);
  EXPECT_EQ(original_id, reacquired_resource.unique_id_for_testing());
  EXPECT_EQ(expected_total_invalidated_rect, total_invalidated_rect);
  resource_pool_->ReleaseResource(std::move(reacquired_resource));
}

TEST_F(ResourcePoolTest, LargeInvalidatedRect) {
  gfx::Size size(100, 100);
  viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::ColorSpace color_space;
  uint64_t content_ids[] = {42, 43, 44};
  // This rect is too large to take the area of it.
  gfx::Rect large_invalidated_rect(0, 0, std::numeric_limits<int>::max() / 2,
                                   std::numeric_limits<int>::max() / 2);

  // Acquire a resource with the first content id.
  ResourcePool::InUsePoolResource resource =
      resource_pool_->AcquireResource(size, format, color_space);
  resource_pool_->OnContentReplaced(resource, content_ids[0]);

  // Set an invalidated rect on the resource.
  gfx::Rect new_invalidated_rect;
  ResourcePool::InUsePoolResource reacquired_resource =
      resource_pool_->TryAcquireResourceForPartialRaster(
          content_ids[1], large_invalidated_rect, content_ids[0],
          &new_invalidated_rect, color_space);
  EXPECT_FALSE(!!reacquired_resource);

  // Release the original resource, returning it to the unused pool.
  resource_pool_->ReleaseResource(std::move(resource));

  // Try to get the resource again, this should work even though the area was
  // too large to compute the area for.
  resource = resource_pool_->TryAcquireResourceForPartialRaster(
      content_ids[2], large_invalidated_rect, content_ids[1],
      &new_invalidated_rect, color_space);
  EXPECT_TRUE(!!resource);
  resource_pool_->ReleaseResource(std::move(resource));
}

TEST_F(ResourcePoolTest, ReuseResource) {
  viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();

  // Create unused resource with size 100x100.
  ResourcePool::InUsePoolResource original =
      resource_pool_->AcquireResource(gfx::Size(100, 100), format, color_space);
  auto original_id = original.unique_id_for_testing();
  CheckAndReturnResource(std::move(original));

  // Try some cases that are too large, none should succeed.
  EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(101, 100), format,
                                                   color_space));
  EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(100, 101), format,
                                                   color_space));
  EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(90, 120), format,
                                                   color_space));
  EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(120, 120), format,
                                                   color_space));

  // Try some cases that are more than 2x smaller than 100x100 in area and
  // won't be re-used.
  EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(49, 100), format,
                                                   color_space));
  EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(100, 49), format,
                                                   color_space));
  EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(50, 50), format,
                                                   color_space));
  EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(70, 70), format,
                                                   color_space));

  // Try some cases that are smaller than 100x100, but within 2x area. Reuse
  // should succeed if non-exact requests are supported. Some platforms never
  // support these.
  if (resource_pool_->AllowsNonExactReUseForTesting()) {
    ResourcePool::InUsePoolResource reused = resource_pool_->AcquireResource(
        gfx::Size(50, 100), format, color_space);
    EXPECT_EQ(original_id, reused.unique_id_for_testing());
    CheckAndReturnResource(std::move(reused));
    reused = resource_pool_->AcquireResource(gfx::Size(100, 50), format,
                                             color_space);
    EXPECT_EQ(original_id, reused.unique_id_for_testing());
    CheckAndReturnResource(std::move(reused));
    reused =
        resource_pool_->AcquireResource(gfx::Size(71, 71), format, color_space);
    EXPECT_EQ(original_id, reused.unique_id_for_testing());
    CheckAndReturnResource(std::move(reused));
  } else {
    EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(50, 100), format,
                                                     color_space));
    EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(100, 50), format,
                                                     color_space));
    EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(71, 71), format,
                                                     color_space));
  }

  // 100x100 is an exact match and should succeed. A subsequent request for
  // the same size should fail (the resource is already in use).
  ResourcePool::InUsePoolResource resource =
      resource_pool_->AcquireResource(gfx::Size(100, 100), format, color_space);
  EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(100, 100), format,
                                                   color_space));
  CheckAndReturnResource(std::move(resource));
}

TEST_F(ResourcePoolTest, PurgedMemory) {
  // Limits high enough to not be hit by this test.
  size_t bytes_limit = 10 * 1024 * 1024;
  size_t count_limit = 100;
  resource_pool_->SetResourceUsageLimits(bytes_limit, count_limit);

  gfx::Size size(100, 100);
  viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  ResourcePool::InUsePoolResource resource =
      resource_pool_->AcquireResource(size, format, color_space);
  SetBackingOnResource(resource);
  EXPECT_TRUE(resource_pool_->PrepareForExport(
      resource, viz::TransferableResource::ResourceSource::kTest));

  EXPECT_EQ(1u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(0u, resource_pool_->GetBusyResourceCountForTesting());

  // Purging and suspending should not impact an in-use resource.
  resource_pool_->OnMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_EQ(1u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(0u, resource_pool_->GetBusyResourceCountForTesting());

  // Export the resource to the display compositor, so it will be busy once
  // released.
  std::vector<viz::TransferableResource> transfers;
  resource_provider_->PrepareSendToParent(
      {resource.resource_id_for_export()}, &transfers,
      static_cast<viz::RasterContextProvider*>(context_provider_.get()));

  // Release the resource making it busy.
  resource_pool_->ReleaseResource(std::move(resource));
  EXPECT_EQ(1u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(1u, resource_pool_->GetBusyResourceCountForTesting());

  // Purging and suspending should not impact a busy resource either.
  resource_pool_->OnMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_EQ(1u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(1u, resource_pool_->GetBusyResourceCountForTesting());

  // The resource moves from busy to available.
  resource_provider_->ReceiveReturnsFromParent(
      viz::TransferableResource::ReturnResources(transfers));
  EXPECT_EQ(1u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(0u, resource_pool_->GetBusyResourceCountForTesting());

  // Purging and suspending should drop unused resources.
  resource_pool_->OnMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_EQ(0u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(0u, resource_pool_->GetBusyResourceCountForTesting());
}

TEST_F(ResourcePoolTest, InvalidateResources) {
  // Limits high enough to not be hit by this test.
  size_t bytes_limit = 10 * 1024 * 1024;
  size_t count_limit = 100;
  resource_pool_->SetResourceUsageLimits(bytes_limit, count_limit);

  gfx::Size size(100, 100);
  viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  ResourcePool::InUsePoolResource busy_resource =
      resource_pool_->AcquireResource(size, format, color_space);
  SetBackingOnResource(busy_resource);
  EXPECT_TRUE(resource_pool_->PrepareForExport(
      busy_resource, viz::TransferableResource::ResourceSource::kTest));
  EXPECT_EQ(1u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(0u, resource_pool_->GetBusyResourceCountForTesting());
  EXPECT_EQ(1u, resource_pool_->resource_count());

  // Make a 2nd resource which will be left available in the pool.
  ResourcePool::InUsePoolResource avail_resource =
      resource_pool_->AcquireResource(size, format, color_space);
  EXPECT_EQ(2u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(0u, resource_pool_->GetBusyResourceCountForTesting());
  EXPECT_EQ(2u, resource_pool_->resource_count());

  // Make a 3nd resource which will be kept in use.
  ResourcePool::InUsePoolResource in_use_resource =
      resource_pool_->AcquireResource(size, format, color_space);
  EXPECT_EQ(3u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(0u, resource_pool_->GetBusyResourceCountForTesting());
  EXPECT_EQ(3u, resource_pool_->resource_count());

  // Mark this one as available.
  resource_pool_->ReleaseResource(std::move(avail_resource));
  EXPECT_EQ(3u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(0u, resource_pool_->GetBusyResourceCountForTesting());
  EXPECT_EQ(2u, resource_pool_->resource_count());

  // Export the first resource to the display compositor, so it will be busy
  // once released.
  std::vector<viz::TransferableResource> transfers;
  resource_provider_->PrepareSendToParent(
      {busy_resource.resource_id_for_export()}, &transfers,
      static_cast<viz::RasterContextProvider*>(context_provider_.get()));

  // Release the resource making it busy.
  resource_pool_->ReleaseResource(std::move(busy_resource));
  EXPECT_EQ(3u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(1u, resource_pool_->GetBusyResourceCountForTesting());
  EXPECT_EQ(1u, resource_pool_->resource_count());

  // Invalidating resources should prevent reuse of any resource.
  resource_pool_->InvalidateResources();

  // The available resource is just dropped immediately.
  EXPECT_EQ(2u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(1u, resource_pool_->GetBusyResourceCountForTesting());
  EXPECT_EQ(1u, resource_pool_->resource_count());

  // The resource moves from busy to available, but since we invalidated,
  // it is not kept.
  resource_provider_->ReceiveReturnsFromParent(
      viz::TransferableResource::ReturnResources(transfers));
  EXPECT_EQ(1u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(0u, resource_pool_->GetBusyResourceCountForTesting());
  EXPECT_EQ(1u, resource_pool_->resource_count());

  // The last resource was in use, when it is released it will not become able
  // to be reused.
  resource_pool_->ReleaseResource(std::move(in_use_resource));
  EXPECT_EQ(0u, resource_pool_->GetTotalResourceCountForTesting());
  EXPECT_EQ(0u, resource_pool_->GetBusyResourceCountForTesting());
}

TEST_F(ResourcePoolTest, ExactRequestsRespected) {
  viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();

  resource_pool_ = std::make_unique<ResourcePool>(
      resource_provider_.get(), context_provider_.get(), test_task_runner_,
      ResourcePool::kDefaultExpirationDelay, true);

  // Create unused resource with size 100x100.
  CheckAndReturnResource(resource_pool_->AcquireResource(gfx::Size(100, 100),
                                                         format, color_space));

  // Try some cases that are smaller than 100x100, but within 2x area which
  // would typically allow reuse. Reuse should fail.
  EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(50, 100), format,
                                                   color_space));
  EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(100, 50), format,
                                                   color_space));
  EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(71, 71), format,
                                                   color_space));

  // 100x100 is an exact match and should succeed. A subsequent request for
  // the same size should fail (the resource is already in use).
  ResourcePool::InUsePoolResource resource =
      resource_pool_->AcquireResource(gfx::Size(100, 100), format, color_space);
  EXPECT_EQ(nullptr, resource_pool_->ReuseResource(gfx::Size(100, 100), format,
                                                   color_space));
  CheckAndReturnResource(std::move(resource));
}

TEST_F(ResourcePoolTest, MetadataSentToDisplayCompositor) {
  // Limits high enough to not be hit by this test.
  size_t bytes_limit = 10 * 1024 * 1024;
  size_t count_limit = 100;
  resource_pool_->SetResourceUsageLimits(bytes_limit, count_limit);

  // These values are all non-default values so we can tell they are propagated.
  gfx::Size size(100, 101);
  viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_4444;
  EXPECT_NE(gfx::BufferFormat::RGBA_8888,
            viz::SinglePlaneSharedImageFormatToBufferFormat(format));
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  gpu::SyncToken sync_token(gpu::CommandBufferNamespace::GPU_IO,
                            gpu::CommandBufferId::FromUnsafeValue(0x123), 7);

  ResourcePool::InUsePoolResource resource =
      resource_pool_->AcquireResource(size, format, color_space);
  SetBackingOnResource(resource);

  // More non-default values.
  resource.gpu_backing()->shared_image =
      gpu::ClientSharedImage::CreateForTesting();
  resource.gpu_backing()->mailbox_sync_token = sync_token;
  resource.gpu_backing()->wait_on_fence_required = true;
  resource.gpu_backing()->overlay_candidate = true;

  EXPECT_TRUE(resource_pool_->PrepareForExport(
      resource, viz::TransferableResource::ResourceSource::kTest));

  std::vector<viz::TransferableResource> transfer;
  resource_provider_->PrepareSendToParent(
      {resource.resource_id_for_export()}, &transfer,
      static_cast<viz::RasterContextProvider*>(context_provider_.get()));

  // The verified_flush flag will be set by the ResourceProvider when it exports
  // the resource.
  sync_token.SetVerifyFlush();

  ASSERT_EQ(transfer.size(), 1u);
  EXPECT_EQ(transfer[0].id, resource.resource_id_for_export());
  EXPECT_EQ(transfer[0].mailbox(),
            resource.gpu_backing()->shared_image->mailbox());
  EXPECT_EQ(transfer[0].sync_token(), sync_token);
  EXPECT_EQ(transfer[0].texture_target(),
            resource.gpu_backing()->shared_image->GetTextureTarget());
  EXPECT_EQ(transfer[0].format, format);
  EXPECT_EQ(
      transfer[0].synchronization_type,
      viz::TransferableResource::SynchronizationType::kGpuCommandsCompleted);
  EXPECT_TRUE(transfer[0].is_overlay_candidate);

  resource_pool_->ReleaseResource(std::move(resource));
}

TEST_F(ResourcePoolTest, InvalidResource) {
  // Limits high enough to not be hit by this test.
  size_t bytes_limit = 10 * 1024 * 1024;
  size_t count_limit = 100;
  resource_pool_->SetResourceUsageLimits(bytes_limit, count_limit);

  // These values are all non-default values so we can tell they are propagated.
  gfx::Size size(100, 101);
  viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_4444;
  EXPECT_NE(gfx::BufferFormat::RGBA_8888,
            viz::SinglePlaneSharedImageFormatToBufferFormat(format));
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();

  ResourcePool::InUsePoolResource resource =
      resource_pool_->AcquireResource(size, format, color_space);

  // Keep a zero mailbox
  auto backing = std::make_unique<StubGpuBacking>();
  backing->wait_on_fence_required = true;
  backing->overlay_candidate = true;
  resource.set_gpu_backing(std::move(backing));

  EXPECT_FALSE(resource_pool_->PrepareForExport(
      resource, viz::TransferableResource::ResourceSource::kTest));

  resource_pool_->ReleaseResource(std::move(resource));

  // Acquire another resource. The resource should not be reused.
  resource = resource_pool_->AcquireResource(size, format, color_space);
  EXPECT_FALSE(resource.gpu_backing());
  resource_pool_->ReleaseResource(std::move(resource));
}

}  // namespace cc
