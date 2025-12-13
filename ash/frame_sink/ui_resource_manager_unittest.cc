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
#include "gpu/command_buffer/client/test_shared_image_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "ui/gfx/geometry/size.h"

namespace ash {
namespace {

constexpr UiSourceId kTestUiSourceId_1 = 1u;
constexpr UiSourceId kTestUiSourceId_2 = 2u;
constexpr gfx::Size kDefaultSize(20, 20);

class UiResourceManagerTest : public testing::Test {
 public:
  UiResourceManagerTest() = default;
  UiResourceManagerTest(const UiResourceManagerTest&) = delete;
  UiResourceManagerTest& operator=(const UiResourceManagerTest&) = delete;

 protected:
  std::unique_ptr<UiResource> MakeResource(
      const gfx::Size& resource_size = kDefaultSize,
      viz::SharedImageFormat format = viz::SinglePlaneFormat::kBGRA_8888,
      UiSourceId ui_source_id = kTestUiSourceId_1) {
    auto shared_image = sii_->CreateSharedImage(
        {format, resource_size, gfx::ColorSpace(),
         gpu::SHARED_IMAGE_USAGE_DISPLAY_READ, "FastInkRootViewFrame"},
        gpu::kNullSurfaceHandle);
    auto resource = std::make_unique<UiResource>(sii_, std::move(shared_image));
    resource->ui_source_id = ui_source_id;
    return resource;
  }

  void SetUp() override {
    sii_ = base::MakeRefCounted<gpu::TestSharedImageInterface>();
    resource_manager_ = std::make_unique<UiResourceManager>();
  }

  void TearDown() override { resource_manager_->LostExportedResources(); }

  scoped_refptr<gpu::SharedImageInterface> sii_;
  std::unique_ptr<UiResourceManager> resource_manager_;
};

TEST_F(UiResourceManagerTest, ReuseResource_NoResources) {
  auto resource = resource_manager_->GetResourceToReuse(
      gfx::Size(100, 100), viz::SinglePlaneFormat::kBGRA_8888,
      kTestUiSourceId_1);

  EXPECT_FALSE(resource);
}

TEST_F(UiResourceManagerTest, ReuseResource) {
  resource_manager_->OfferResourceForTesting(
      MakeResource(gfx::Size(10, 10), viz::SinglePlaneFormat::kBGRA_8888,
                   kTestUiSourceId_1));

  resource_manager_->OfferResourceForTesting(MakeResource(
      kDefaultSize, viz::SinglePlaneFormat::kBGRA_8888, kTestUiSourceId_1));

  resource_manager_->OfferResourceForTesting(
      MakeResource(gfx::Size(10, 20), viz::SinglePlaneFormat::kBGRA_8888,
                   kTestUiSourceId_2));

  resource_manager_->OfferResourceForTesting(
      MakeResource(gfx::Size(10, 20), viz::SinglePlaneFormat::kBGRA_8888,
                   kTestUiSourceId_2));

  EXPECT_EQ(resource_manager_->available_resources_count(), 4u);

  // When we have no match in the currently available resources.
  auto resource = resource_manager_->GetResourceToReuse(
      gfx::Size(100, 100), viz::SinglePlaneFormat::kBGRA_8888,
      kTestUiSourceId_1);

  EXPECT_FALSE(resource);

  // When we have the requested resource.
  resource = resource_manager_->GetResourceToReuse(
      gfx::Size(10, 10), viz::SinglePlaneFormat::kBGRA_8888, kTestUiSourceId_1);

  EXPECT_TRUE(resource);

  EXPECT_EQ(resource->ui_source_id, kTestUiSourceId_1);
  EXPECT_EQ(resource->client_shared_image()->format(),
            viz::SinglePlaneFormat::kBGRA_8888);
  EXPECT_EQ(resource->client_shared_image()->size(), gfx::Size(10, 10));

  // When we have multiple matching resources, return any matching resource.
  resource = resource_manager_->GetResourceToReuse(
      gfx::Size(10, 20), viz::SinglePlaneFormat::kBGRA_8888, kTestUiSourceId_2);

  EXPECT_TRUE(resource);

  EXPECT_EQ(resource->ui_source_id, kTestUiSourceId_2);
  EXPECT_EQ(resource->client_shared_image()->format(),
            viz::SinglePlaneFormat::kBGRA_8888);
  EXPECT_EQ(resource->client_shared_image()->size(), gfx::Size(10, 20));
}

TEST_F(UiResourceManagerTest, OfferResource) {
  resource_manager_->OfferResourceForTesting(MakeResource());
  resource_manager_->OfferResourceForTesting(MakeResource());

  // As soon as we offer a resource, it is available to be used.
  EXPECT_EQ(resource_manager_->available_resources_count(), 2u);
}

using UiResourceManagerDeathTest = UiResourceManagerTest;
TEST_F(UiResourceManagerDeathTest,
       NeedToClearAllExportedResourceBeforeDeletingManager) {
  resource_manager_->OfferResourceForTesting(MakeResource());
  resource_manager_->OfferAndPrepareResourceForExport(
      MakeResource(kDefaultSize));

  // The manager cannot be deleted as we still have a exported resource.
  EXPECT_DCHECK_DEATH({ resource_manager_.reset(); });
}

TEST_F(UiResourceManagerTest, PrepareResourceForExporting) {
  resource_manager_->OfferResourceForTesting(MakeResource());
  resource_manager_->OfferResourceForTesting(MakeResource());

  EXPECT_EQ(resource_manager_->exported_resources_count(), 0u);
  EXPECT_EQ(resource_manager_->available_resources_count(), 2u);

  viz::TransferableResource transferable_resource =
      resource_manager_->OfferAndPrepareResourceForExport(
          MakeResource(kDefaultSize));

  // We exported one resources leaving two resources as available.
  EXPECT_EQ(resource_manager_->exported_resources_count(), 1u);
  EXPECT_EQ(resource_manager_->available_resources_count(), 2u);
}

TEST_F(UiResourceManagerTest, CannotReuseExportedResourcesTillReclaimed) {
  const gfx::Size kDefaultSize2(10, 10);
  resource_manager_->OfferResourceForTesting(MakeResource(kDefaultSize2));
  EXPECT_EQ(resource_manager_->available_resources_count(), 1u);

  viz::TransferableResource transferable_resource =
      resource_manager_->OfferAndPrepareResourceForExport(
          MakeResource(kDefaultSize));

  // We cannot release an exported resource until the resource is reclaimed.
  auto released_resource = resource_manager_->GetResourceToReuse(
      kDefaultSize, viz::SinglePlaneFormat::kBGRA_8888, kTestUiSourceId_1);

  EXPECT_FALSE(released_resource);
  EXPECT_EQ(resource_manager_->exported_resources_count(), 1u);

  std::vector<viz::ReturnedResource> returned;
  returned.emplace_back();
  returned.back().id = transferable_resource.id;
  returned.back().count = 1;
  returned.back().lost = false;

  resource_manager_->ReclaimResources(returned);

  EXPECT_EQ(resource_manager_->exported_resources_count(), 0u);
  EXPECT_EQ(resource_manager_->available_resources_count(), 2u);

  // Now that we reclaimed the exported resource, we can now reuse it.
  released_resource = resource_manager_->GetResourceToReuse(
      kDefaultSize, viz::SinglePlaneFormat::kBGRA_8888, kTestUiSourceId_1);
  EXPECT_TRUE(released_resource);
  EXPECT_EQ(resource_manager_->available_resources_count(), 1u);
}

TEST_F(UiResourceManagerTest, ExportedResourcesAreLost) {
  resource_manager_->OfferResourceForTesting(MakeResource());
  resource_manager_->OfferResourceForTesting(MakeResource());
  resource_manager_->OfferResourceForTesting(MakeResource());

  resource_manager_->OfferAndPrepareResourceForExport(
      MakeResource(kDefaultSize));
  resource_manager_->OfferAndPrepareResourceForExport(
      MakeResource(kDefaultSize));

  EXPECT_EQ(resource_manager_->available_resources_count(), 3u);
  EXPECT_EQ(resource_manager_->exported_resources_count(), 2u);

  // If there is no chance to reclaim back the resources we need to clear the
  // exported pool.
  resource_manager_->LostExportedResources();

  EXPECT_EQ(resource_manager_->available_resources_count(), 3u);
  EXPECT_EQ(resource_manager_->exported_resources_count(), 0u);
}

TEST_F(UiResourceManagerTest, ReclaimResources) {
  auto resource1 = resource_manager_->OfferAndPrepareResourceForExport(
      MakeResource(kDefaultSize));
  auto resource2 = resource_manager_->OfferAndPrepareResourceForExport(
      MakeResource(kDefaultSize));
  {
    // Returning a non-lost resource.
    std::vector<viz::ReturnedResource> returned;
    returned.emplace_back();
    returned.back().id = resource2.id;
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
    returned.back().id = resource1.id;
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

  auto resource3 = resource_manager_->OfferAndPrepareResourceForExport(
      MakeResource(kDefaultSize));
  auto resource4 = resource_manager_->OfferAndPrepareResourceForExport(
      MakeResource(kDefaultSize));

  {
    // Returning multiple resources.
    std::vector<viz::ReturnedResource> returned;
    returned.emplace_back();
    returned.back().id = resource3.id;
    returned.back().count = 1;
    returned.back().lost = true;

    returned.emplace_back();
    returned.back().id = resource4.id;
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
