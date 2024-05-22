// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame_sink/ui_resource_manager.h"

#include <memory>
#include <vector>

#include "ash/frame_sink/ui_resource.h"
#include "base/test/gtest_util.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "ui/gfx/geometry/size.h"

namespace ash {
namespace {

constexpr UiSourceId kTestUiSourceId_1 = 1u;
constexpr UiSourceId kTestUiSourceId_2 = 2u;

std::unique_ptr<UiResource> MakeResource(const gfx::Size& resource_size,
                                         viz::SharedImageFormat format,
                                         uint32_t ui_source_id) {
  auto resource = std::make_unique<UiResource>();
  resource->ui_source_id = ui_source_id;
  resource->format = format;
  resource->resource_size = resource_size;
  resource->SetExternallyOwnedMailbox(gpu::Mailbox::Generate());
  return resource;
}

class UiResourceManagerTest : public testing::Test {
 public:
  UiResourceManagerTest() = default;
  UiResourceManagerTest(const UiResourceManagerTest&) = delete;
  UiResourceManagerTest& operator=(const UiResourceManagerTest&) = delete;

 protected:
  void SetUp() override {
    resource_manager_ = std::make_unique<UiResourceManager>();
  }

  void TearDown() override { resource_manager_->LostExportedResources(); }

  std::unique_ptr<UiResourceManager> resource_manager_;
};

TEST_F(UiResourceManagerTest, ReuseResource_NoResources) {
  viz::ResourceId resource_id = resource_manager_->FindResourceToReuse(
      gfx::Size(100, 100), viz::SinglePlaneFormat::kBGRA_8888,
      kTestUiSourceId_1);

  EXPECT_EQ(resource_id, viz::kInvalidResourceId);
}

TEST_F(UiResourceManagerTest, ReuseResource) {
  resource_manager_->OfferResource(
      MakeResource(gfx::Size(10, 10), viz::SinglePlaneFormat::kBGRA_8888,
                   kTestUiSourceId_1));

  resource_manager_->OfferResource(
      MakeResource(gfx::Size(20, 20), viz::SinglePlaneFormat::kBGRA_8888,
                   kTestUiSourceId_1));

  resource_manager_->OfferResource(
      MakeResource(gfx::Size(10, 20), viz::SinglePlaneFormat::kBGRA_8888,
                   kTestUiSourceId_2));

  resource_manager_->OfferResource(
      MakeResource(gfx::Size(10, 20), viz::SinglePlaneFormat::kBGRA_8888,
                   kTestUiSourceId_2));

  EXPECT_EQ(resource_manager_->available_resources_count(), 4u);

  // When we have no match in the currently available resources.
  viz::ResourceId resource_id = resource_manager_->FindResourceToReuse(
      gfx::Size(100, 100), viz::SinglePlaneFormat::kBGRA_8888,
      kTestUiSourceId_1);

  EXPECT_EQ(resource_id, viz::kInvalidResourceId);

  // When we have the requested resource.
  resource_id = resource_manager_->FindResourceToReuse(
      gfx::Size(10, 10), viz::SinglePlaneFormat::kBGRA_8888, kTestUiSourceId_1);

  EXPECT_NE(resource_id, viz::kInvalidResourceId);

  auto* found_resource = resource_manager_->PeekAvailableResource(resource_id);
  EXPECT_EQ(found_resource->ui_source_id, kTestUiSourceId_1);
  EXPECT_EQ(found_resource->format, viz::SinglePlaneFormat::kBGRA_8888);
  EXPECT_EQ(found_resource->resource_size, gfx::Size(10, 10));

  // When we have multiple matching resources, return any matching resource.
  resource_id = resource_manager_->FindResourceToReuse(
      gfx::Size(10, 20), viz::SinglePlaneFormat::kBGRA_8888, kTestUiSourceId_2);

  EXPECT_NE(resource_id, viz::kInvalidResourceId);
  found_resource = resource_manager_->PeekAvailableResource(resource_id);

  EXPECT_EQ(found_resource->ui_source_id, kTestUiSourceId_2);
  EXPECT_EQ(found_resource->format, viz::SinglePlaneFormat::kBGRA_8888);
  EXPECT_EQ(found_resource->resource_size, gfx::Size(10, 20));
}

TEST_F(UiResourceManagerTest, OfferResource) {
  resource_manager_->OfferResource(std::make_unique<UiResource>());
  resource_manager_->OfferResource(std::make_unique<UiResource>());

  // As soon as we offer a resource, it is available to be used.
  EXPECT_EQ(resource_manager_->available_resources_count(), 2u);
}

using UiResourceManagerDeathTest = UiResourceManagerTest;
TEST_F(UiResourceManagerDeathTest,
       NeedToClearAllExportedResourceBeforeDeletingManager) {
  viz::ResourceId to_be_exported_resource_id =
      resource_manager_->OfferResource(std::make_unique<UiResource>());
  resource_manager_->OfferResource(std::make_unique<UiResource>());

  resource_manager_->PrepareResourceForExport(to_be_exported_resource_id);

  // The manager cannot be deleted as we still have a exported resource.
  EXPECT_DCHECK_DEATH({ resource_manager_.reset(); });
}

TEST_F(UiResourceManagerTest, PrepareResourceForExporting_InvalidIds) {
  viz::ResourceId to_be_released_resource =
      resource_manager_->OfferResource(std::make_unique<UiResource>());
  resource_manager_->OfferResource(std::make_unique<UiResource>());
  {
    // We cannot export a resource that we do not manage.
    auto transferable_resource =
        resource_manager_->PrepareResourceForExport(viz::ResourceId(20));
    EXPECT_TRUE(transferable_resource.is_empty());
    EXPECT_EQ(resource_manager_->exported_resources_count(), 0u);

    resource_manager_->ReleaseAvailableResource(to_be_released_resource);
  }
  {
    // We cannot export a resource that was released for the manager.
    resource_manager_->ReleaseAvailableResource(to_be_released_resource);

    auto transferable_resource =
        resource_manager_->PrepareResourceForExport(to_be_released_resource);
    EXPECT_TRUE(transferable_resource.is_empty());
    EXPECT_EQ(resource_manager_->exported_resources_count(), 0u);
  }
}

TEST_F(UiResourceManagerTest, PrepareResourceForExporting) {
  viz::ResourceId to_be_exported_resource_id =
      resource_manager_->OfferResource(std::make_unique<UiResource>());
  resource_manager_->OfferResource(std::make_unique<UiResource>());
  resource_manager_->OfferResource(std::make_unique<UiResource>());

  EXPECT_EQ(resource_manager_->exported_resources_count(), 0u);
  EXPECT_EQ(resource_manager_->available_resources_count(), 3u);

  // The resource in now in the exported_pool.
  viz::TransferableResource transferable_resource =
      resource_manager_->PrepareResourceForExport(to_be_exported_resource_id);

  // We exported one resources leaving two resources as available.
  EXPECT_EQ(resource_manager_->exported_resources_count(), 1u);
  EXPECT_EQ(resource_manager_->available_resources_count(), 2u);

  EXPECT_EQ(transferable_resource.id, to_be_exported_resource_id);
}

TEST_F(UiResourceManagerTest, CannotExportAlreadyExportedResource) {
  viz::ResourceId to_be_exported_resource_id =
      resource_manager_->OfferResource(std::make_unique<UiResource>());
  resource_manager_->OfferResource(std::make_unique<UiResource>());
  resource_manager_->OfferResource(std::make_unique<UiResource>());

  resource_manager_->PrepareResourceForExport(to_be_exported_resource_id);

  auto transferable_resource =
      resource_manager_->PrepareResourceForExport(to_be_exported_resource_id);
  EXPECT_TRUE(transferable_resource.is_empty());
}

TEST_F(UiResourceManagerTest, ReleaseResource_InvalidIds) {
  // We can only release a resource that we currently manage.
  const auto released_resource =
      resource_manager_->ReleaseAvailableResource(viz::ResourceId(20));
  EXPECT_FALSE(released_resource);
}

TEST_F(UiResourceManagerTest, ReleaseResource) {
  viz::ResourceId to_be_released_resource =
      resource_manager_->OfferResource(std::make_unique<UiResource>());
  resource_manager_->OfferResource(std::make_unique<UiResource>());

  EXPECT_EQ(resource_manager_->available_resources_count(), 2u);

  auto released_resource =
      resource_manager_->ReleaseAvailableResource(to_be_released_resource);

  EXPECT_EQ(resource_manager_->available_resources_count(), 1u);
  EXPECT_EQ(released_resource->resource_id, to_be_released_resource);
}

TEST_F(UiResourceManagerTest, CannotReleaseExportedResourcesTillReclaimed) {
  viz::ResourceId to_be_exported_resource =
      resource_manager_->OfferResource(std::make_unique<UiResource>());
  resource_manager_->OfferResource(std::make_unique<UiResource>());

  EXPECT_EQ(resource_manager_->available_resources_count(), 2u);

  resource_manager_->PrepareResourceForExport(to_be_exported_resource);

  // We cannot release an exported resource until the resource is reclaimed.
  auto released_resource =
      resource_manager_->ReleaseAvailableResource(to_be_exported_resource);

  EXPECT_FALSE(released_resource);
  EXPECT_EQ(resource_manager_->exported_resources_count(), 1u);

  std::vector<viz::ReturnedResource> returned;
  returned.emplace_back();
  returned.back().id = to_be_exported_resource;
  returned.back().count = 1;
  returned.back().lost = false;

  resource_manager_->ReclaimResources(returned);

  viz::ResourceId to_be_released_resource = to_be_exported_resource;

  // Now that we reclaimed the exported resource, we can now release it.
  released_resource =
      resource_manager_->ReleaseAvailableResource(to_be_released_resource);

  EXPECT_EQ(to_be_released_resource, released_resource->resource_id);
}

TEST_F(UiResourceManagerTest, ExportedResourcesAreLost) {
  viz::ResourceId to_be_exported_resource_1 =
      resource_manager_->OfferResource(std::make_unique<UiResource>());
  viz::ResourceId to_be_exported_resource_2 =
      resource_manager_->OfferResource(std::make_unique<UiResource>());
  resource_manager_->OfferResource(std::make_unique<UiResource>());
  resource_manager_->OfferResource(std::make_unique<UiResource>());
  resource_manager_->OfferResource(std::make_unique<UiResource>());

  resource_manager_->PrepareResourceForExport(to_be_exported_resource_1);
  resource_manager_->PrepareResourceForExport(to_be_exported_resource_2);

  EXPECT_EQ(resource_manager_->available_resources_count(), 3u);
  EXPECT_EQ(resource_manager_->exported_resources_count(), 2u);

  // If there is no chance to reclaim back the resources we need to clear the
  // exported pool.
  resource_manager_->LostExportedResources();

  EXPECT_EQ(resource_manager_->available_resources_count(), 3u);
  EXPECT_EQ(resource_manager_->exported_resources_count(), 0u);
}

TEST_F(UiResourceManagerTest, ReclaimResources) {
  viz::ResourceId to_be_exported_resource_1 =
      resource_manager_->OfferResource(std::make_unique<UiResource>());
  viz::ResourceId to_be_exported_resource_2 =
      resource_manager_->OfferResource(std::make_unique<UiResource>());

  resource_manager_->PrepareResourceForExport(to_be_exported_resource_1);
  resource_manager_->PrepareResourceForExport(to_be_exported_resource_2);
  {
    // Returning a non-lost resource.
    std::vector<viz::ReturnedResource> returned;
    returned.emplace_back();
    returned.back().id = to_be_exported_resource_2;
    returned.back().count = 1;
    returned.back().lost = false;

    EXPECT_EQ(resource_manager_->exported_resources_count(), 2u);
    resource_manager_->ReclaimResources(returned);
    EXPECT_EQ(resource_manager_->exported_resources_count(), 1u);

    // Reclaimed resource is now available to be reused.
    EXPECT_EQ(resource_manager_->available_resources_count(), 1u);
  }

  {
    // Returning a lost resource.
    std::vector<viz::ReturnedResource> returned;
    returned.emplace_back();
    returned.back().id = to_be_exported_resource_1;
    returned.back().count = 1;
    returned.back().lost = true;

    EXPECT_EQ(resource_manager_->exported_resources_count(), 1u);

    // We have one available resource already.
    EXPECT_EQ(resource_manager_->available_resources_count(), 1u);
    resource_manager_->ReclaimResources(returned);

    // We have received the resource so it is no more exported.
    EXPECT_EQ(resource_manager_->exported_resources_count(), 0u);

    // This reclaimed resource is lost so it cannot be reused again.
    EXPECT_EQ(resource_manager_->available_resources_count(), 1u);
  }

  viz::ResourceId to_be_exported_resource_3 =
      resource_manager_->OfferResource(std::make_unique<UiResource>());

  viz::ResourceId to_be_exported_resource_4 =
      resource_manager_->OfferResource(std::make_unique<UiResource>());

  // Exporting more resources.
  resource_manager_->PrepareResourceForExport(to_be_exported_resource_3);
  resource_manager_->PrepareResourceForExport(to_be_exported_resource_4);

  {
    // Returning multiple resources.
    std::vector<viz::ReturnedResource> returned;
    returned.emplace_back();
    returned.back().id = to_be_exported_resource_3;
    returned.back().count = 1;
    returned.back().lost = true;

    returned.emplace_back();
    returned.back().id = to_be_exported_resource_4;
    returned.back().count = 1;
    returned.back().lost = false;

    EXPECT_EQ(resource_manager_->exported_resources_count(), 2u);

    // We have one available resource that was reclaimed before in the test.
    EXPECT_EQ(resource_manager_->available_resources_count(), 1u);
    resource_manager_->ReclaimResources(returned);

    // We have reclaimed all the resources.
    EXPECT_EQ(resource_manager_->exported_resources_count(), 0u);

    // One of the two exported resources was lost.
    EXPECT_EQ(resource_manager_->available_resources_count(), 2u);
  }
}

}  // namespace
}  // namespace ash
