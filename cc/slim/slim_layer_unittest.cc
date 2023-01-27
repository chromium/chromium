// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "cc/slim/layer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc::slim {

namespace {

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

}  // namespace

}  // namespace cc::slim
