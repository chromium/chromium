// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/unguessable_token.h"
#include "cc/layers/deadline_policy.h"
#include "cc/resources/ui_resource_manager.h"
#include "cc/slim/filter.h"
#include "cc/slim/layer.h"
#include "cc/slim/layer_tree.h"
#include "cc/slim/nine_patch_layer.h"
#include "cc/slim/solid_color_layer.h"
#include "cc/slim/surface_layer.h"
#include "cc/slim/test_layer_tree_client.h"
#include "cc/slim/ui_resource_layer.h"
#include "components/viz/common/quads/offset_tag.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"

namespace cc::slim {

TEST(SlimLayerTest, LayerTreeManipulation) {
  scoped_refptr<Layer> layer1 = Layer::Create();
  scoped_refptr<Layer> layer2 = Layer::Create();
  scoped_refptr<Layer> layer3 = Layer::Create();
  scoped_refptr<Layer> layer4 = Layer::Create();
  scoped_refptr<Layer> layer5 = Layer::Create();

  EXPECT_FALSE(layer1->parent());
  EXPECT_EQ(layer1->RootLayer(), layer1.get());
  EXPECT_TRUE(layer1->children().empty());

  layer1->AddChild(layer2);
  EXPECT_EQ(layer2->parent(), layer1.get());
  EXPECT_EQ(layer1->RootLayer(), layer1.get());
  EXPECT_EQ(layer2->RootLayer(), layer1.get());
  EXPECT_EQ(layer1->children().size(), 1u);
  EXPECT_EQ(layer1->children()[0].get(), layer2.get());
  EXPECT_TRUE(layer2->HasAncestor(layer1.get()));
  EXPECT_FALSE(layer1->HasAncestor(layer2.get()));

  layer1->InsertChild(layer3, /*position=*/0u);
  EXPECT_EQ(layer3->parent(), layer1.get());
  EXPECT_EQ(layer3->RootLayer(), layer1.get());
  EXPECT_EQ(layer1->children().size(), 2u);
  EXPECT_EQ(layer1->children()[0].get(), layer3.get());
  EXPECT_TRUE(layer3->HasAncestor(layer1.get()));
  EXPECT_FALSE(layer1->HasAncestor(layer3.get()));

  layer1->ReplaceChild(layer2.get(), layer4);
  EXPECT_EQ(layer2->parent(), nullptr);
  EXPECT_TRUE(layer2->HasOneRef());
  EXPECT_EQ(layer4->parent(), layer1.get());
  EXPECT_EQ(layer4->RootLayer(), layer1.get());
  EXPECT_EQ(layer1->children().size(), 2u);
  EXPECT_EQ(layer1->children()[1].get(), layer4.get());
  EXPECT_TRUE(layer4->HasAncestor(layer1.get()));

  layer4->AddChild(layer5);
  EXPECT_EQ(layer5->parent(), layer4.get());
  EXPECT_EQ(layer5->RootLayer(), layer1.get());
  EXPECT_TRUE(layer5->HasAncestor(layer1.get()));
  EXPECT_EQ(layer4->children().size(), 1u);
  EXPECT_EQ(layer4->children()[0].get(), layer5.get());

  layer5->RemoveFromParent();
  EXPECT_TRUE(layer5->HasOneRef());
  EXPECT_TRUE(layer4->children().empty());

  layer1->RemoveAllChildren();
  EXPECT_TRUE(layer1->children().empty());
  EXPECT_TRUE(layer1->HasOneRef());
  EXPECT_TRUE(layer2->HasOneRef());
  EXPECT_TRUE(layer3->HasOneRef());
  EXPECT_TRUE(layer4->HasOneRef());
}

TEST(SlimLayerTest, LayerProperties) {
  scoped_refptr<Layer> layer = Layer::Create();

  layer->SetPosition(gfx::PointF(1.f, 2.f));
  EXPECT_EQ(layer->position(), gfx::PointF(1.f, 2.f));

  layer->SetBounds(gfx::Size(1, 2));
  EXPECT_EQ(layer->bounds(), gfx::Size(1, 2));

  layer->SetTransform(gfx::Transform::MakeTranslation(1.f, 2.f));
  EXPECT_EQ(layer->transform(), gfx::Transform::MakeTranslation(1.f, 2.f));

  layer->SetTransformOrigin(gfx::PointF(1.f, 2.f));
  EXPECT_EQ(layer->transform_origin(), gfx::PointF(1.f, 2.f));

  layer->SetIsDrawable(true);
  EXPECT_TRUE(layer->draws_content());
  layer->SetIsDrawable(false);
  EXPECT_FALSE(layer->draws_content());

  layer->SetBackgroundColor(SkColors::kGray);
  EXPECT_EQ(layer->background_color(), SkColors::kGray);

  layer->SetContentsOpaque(true);
  EXPECT_TRUE(layer->contents_opaque());
  layer->SetContentsOpaque(false);
  EXPECT_FALSE(layer->contents_opaque());

  layer->SetOpacity(0.5f);
  EXPECT_EQ(layer->opacity(), 0.5f);

  layer->SetHideLayerAndSubtree(true);
  EXPECT_TRUE(layer->hide_layer_and_subtree());
  layer->SetHideLayerAndSubtree(false);
  EXPECT_FALSE(layer->hide_layer_and_subtree());

  layer->SetMasksToBounds(true);
  EXPECT_TRUE(layer->masks_to_bounds());
  layer->SetMasksToBounds(false);
  EXPECT_FALSE(layer->masks_to_bounds());

  std::vector<Filter> filters;
  filters.push_back(Filter::CreateBrightness(0.5f));
  layer->SetFilters(std::move(filters));

  EXPECT_FALSE(layer->HasNonTrivialMaskFilterInfo());
  layer->SetRoundedCorner(gfx::RoundedCornersF(50));
  EXPECT_EQ(layer->corner_radii(), gfx::RoundedCornersF(50));
  EXPECT_TRUE(layer->HasNonTrivialMaskFilterInfo());
  layer->SetRoundedCorner(gfx::RoundedCornersF());
  EXPECT_FALSE(layer->HasNonTrivialMaskFilterInfo());

  gfx::LinearGradient gradient;
  gradient.AddStep(0.0f, 0);
  gradient.AddStep(1.0f, 255);
  EXPECT_FALSE(layer->HasNonTrivialMaskFilterInfo());
  layer->SetGradientMask(gradient);
  EXPECT_EQ(layer->gradient_mask(), gradient);
  EXPECT_TRUE(layer->HasNonTrivialMaskFilterInfo());
  layer->SetGradientMask(gfx::LinearGradient());
  EXPECT_FALSE(layer->HasNonTrivialMaskFilterInfo());
}

TEST(SlimLayerTest, SurfaceLayerProperties) {
  scoped_refptr<SurfaceLayer> layer = SurfaceLayer::Create();

  layer->SetStretchContentToFillBounds(true);
  EXPECT_TRUE(layer->stretch_content_to_fill_bounds());
  layer->SetStretchContentToFillBounds(false);
  EXPECT_FALSE(layer->stretch_content_to_fill_bounds());

  base::UnguessableToken token = base::UnguessableToken::Create();
  viz::SurfaceId start(viz::FrameSinkId(1u, 2u),
                       viz::LocalSurfaceId(3u, 4u, token));
  viz::SurfaceId end(viz::FrameSinkId(1u, 2u),
                     viz::LocalSurfaceId(5u, 6u, token));

  EXPECT_EQ(layer->oldest_acceptable_fallback(), std::nullopt);
  layer->SetOldestAcceptableFallback(start);
  EXPECT_EQ(layer->oldest_acceptable_fallback(), start);
  layer->SetSurfaceId(end, cc::DeadlinePolicy::UseDefaultDeadline());
  EXPECT_EQ(layer->surface_id(), end);
}

TEST(SlimLayerTest, UIResourceLayerProperties) {
  scoped_refptr<UIResourceLayer> layer = UIResourceLayer::Create();

  layer->SetUIResourceId(1);
  layer->SetUIResourceId(0);

  auto image_info =
      SkImageInfo::Make(1, 1, kN32_SkColorType, kPremul_SkAlphaType);
  SkBitmap bitmap;
  bitmap.allocPixels(image_info);
  bitmap.setImmutable();
  layer->SetBitmap(bitmap);

  layer->SetUV(gfx::PointF(0.25f, 0.25f), gfx::PointF(0.75f, 0.75f));

  TestLayerTreeClient client;
  auto layer_tree = LayerTree::Create(&client);
  layer_tree->SetRoot(layer);

  EXPECT_NE(layer->resource_id(), 0);
  EXPECT_EQ(layer_tree->GetUIResourceManager()
                ->owned_shared_resources_size_for_test(),
            1u);
}

TEST(SlimLayerTest, NinePatchLayerProperties) {
  scoped_refptr<NinePatchLayer> layer = NinePatchLayer::Create();

  auto image_info =
      SkImageInfo::Make(10, 10, kN32_SkColorType, kPremul_SkAlphaType);
  SkBitmap bitmap;
  bitmap.allocPixels(image_info);
  bitmap.setImmutable();
  layer->SetBitmap(bitmap);

  layer->SetBorder(gfx::Rect(1, 1, 8, 8));
  layer->SetAperture(gfx::Rect(4, 4, 2, 2));
  layer->SetFillCenter(true);
  layer->SetNearestNeighbor(true);

  TestLayerTreeClient client;
  auto layer_tree = LayerTree::Create(&client);
  layer_tree->SetRoot(layer);

  EXPECT_EQ(layer_tree->GetUIResourceManager()
                ->owned_shared_resources_size_for_test(),
            1u);
}

TEST(SlimLayerTest, NumDescendantsThatDrawContent) {
  auto layer0 = Layer::Create();
  auto layer1 = Layer::Create();
  auto layer2 = Layer::Create();

  layer0->AddChild(layer1);
  layer0->AddChild(layer2);

  EXPECT_EQ(layer0->NumDescendantsThatDrawContent(), 0);
  EXPECT_EQ(layer1->NumDescendantsThatDrawContent(), 0);
  EXPECT_EQ(layer2->NumDescendantsThatDrawContent(), 0);

  auto drawing_layer0 = SolidColorLayer::Create();
  auto drawing_layer1 = SolidColorLayer::Create();
  auto drawing_layer2 = SolidColorLayer::Create();
  drawing_layer0->SetIsDrawable(true);
  drawing_layer1->SetIsDrawable(true);
  drawing_layer2->SetIsDrawable(true);
  EXPECT_TRUE(drawing_layer0->draws_content());
  EXPECT_TRUE(drawing_layer1->draws_content());
  EXPECT_TRUE(drawing_layer2->draws_content());

  layer1->AddChild(drawing_layer0);
  EXPECT_EQ(layer0->NumDescendantsThatDrawContent(), 1);
  EXPECT_EQ(layer1->NumDescendantsThatDrawContent(), 1);
  EXPECT_EQ(layer2->NumDescendantsThatDrawContent(), 0);

  drawing_layer0->AddChild(drawing_layer1);
  EXPECT_EQ(layer0->NumDescendantsThatDrawContent(), 2);
  EXPECT_EQ(layer1->NumDescendantsThatDrawContent(), 2);
  EXPECT_EQ(layer2->NumDescendantsThatDrawContent(), 0);
  EXPECT_EQ(drawing_layer0->NumDescendantsThatDrawContent(), 1);

  layer1->AddChild(drawing_layer2);
  EXPECT_EQ(layer0->NumDescendantsThatDrawContent(), 3);
  EXPECT_EQ(layer1->NumDescendantsThatDrawContent(), 3);
  EXPECT_EQ(layer2->NumDescendantsThatDrawContent(), 0);

  drawing_layer0->RemoveFromParent();
  EXPECT_EQ(layer0->NumDescendantsThatDrawContent(), 1);
  EXPECT_EQ(layer1->NumDescendantsThatDrawContent(), 1);
  EXPECT_EQ(layer2->NumDescendantsThatDrawContent(), 0);

  layer2->AddChild(drawing_layer0);
  EXPECT_EQ(layer0->NumDescendantsThatDrawContent(), 3);
  EXPECT_EQ(layer1->NumDescendantsThatDrawContent(), 1);
  EXPECT_EQ(layer2->NumDescendantsThatDrawContent(), 2);
}

TEST(SlimLayerTest, OffsetTag) {
  auto layer0 = Layer::Create();
  auto layer1 = SurfaceLayer::Create();
  auto layer2 = Layer::Create();

  layer0->AddChild(layer1);
  layer0->AddChild(layer2);

  const auto offset_tag = viz::OffsetTag::CreateRandom();
  const viz::OffsetTagConstraints constraints(0, 0, -10.0f, 0);
  layer1->RegisterOffsetTag(offset_tag, constraints);

  layer0->SetOffsetTag(offset_tag);
  EXPECT_EQ(layer0->offset_tag(), offset_tag);

  // Unregister the OffsetTag and set zero tag on parent layer.
  layer1->UnregisterOffsetTag(offset_tag);
  layer0->SetOffsetTag({});
}

}  // namespace cc::slim
