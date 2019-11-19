// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_list_iterator.h"

#include <memory>

#include "base/containers/adapters.h"
#include "cc/animation/animation_host.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/test_task_graph_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

// Layer version unit tests

TEST(LayerListIteratorTest, VerifyTraversalOrder) {
  // Unfortunate preamble.
  FakeLayerTreeHostClient client;
  TestTaskGraphRunner task_graph_runner;
  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  std::unique_ptr<FakeLayerTreeHost> host_ptr = FakeLayerTreeHost::Create(
      &client, &task_graph_runner, animation_host.get());
  FakeLayerTreeHost* host = host_ptr.get();

  // This test constructs the following tree.
  // 1
  // +-2
  // | +-3
  // | +-4
  // + 5
  //   +-6
  //   +-7
  // We expect to visit all seven layers in that order.
  scoped_refptr<Layer> layer1 = Layer::Create();
  scoped_refptr<Layer> layer2 = Layer::Create();
  scoped_refptr<Layer> layer3 = Layer::Create();
  scoped_refptr<Layer> layer4 = Layer::Create();
  scoped_refptr<Layer> layer5 = Layer::Create();
  scoped_refptr<Layer> layer6 = Layer::Create();
  scoped_refptr<Layer> layer7 = Layer::Create();

  std::unordered_map<int, int> layer_id_to_order;
  layer_id_to_order[layer1->id()] = 1;
  layer_id_to_order[layer2->id()] = 2;
  layer_id_to_order[layer3->id()] = 3;
  layer_id_to_order[layer4->id()] = 4;
  layer_id_to_order[layer5->id()] = 5;
  layer_id_to_order[layer6->id()] = 6;
  layer_id_to_order[layer7->id()] = 7;

  layer2->AddChild(std::move(layer3));
  layer2->AddChild(std::move(layer4));

  layer5->AddChild(std::move(layer6));
  layer5->AddChild(std::move(layer7));

  layer1->AddChild(std::move(layer2));
  layer1->AddChild(std::move(layer5));

  host->SetRootLayer(std::move(layer1));

  int i = 1;
  for (auto* layer : *host) {
    EXPECT_EQ(i++, layer_id_to_order[layer->id()]);
  }
  EXPECT_EQ(8, i);
}

TEST(LayerListIteratorTest, VerifySingleLayer) {
  // Unfortunate preamble.
  FakeLayerTreeHostClient client;
  TestTaskGraphRunner task_graph_runner;
  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  std::unique_ptr<FakeLayerTreeHost> host_ptr = FakeLayerTreeHost::Create(
      &client, &task_graph_runner, animation_host.get());
  FakeLayerTreeHost* host = host_ptr.get();

  // This test constructs a tree consisting of a single layer.
  scoped_refptr<Layer> layer1 = Layer::Create();
  std::unordered_map<int, int> layer_id_to_order;
  layer_id_to_order[layer1->id()] = 1;
  host->SetRootLayer(std::move(layer1));

  int i = 1;
  for (auto* layer : *host) {
    EXPECT_EQ(i++, layer_id_to_order[layer->id()]);
  }
  EXPECT_EQ(2, i);
}

TEST(LayerListIteratorTest, VerifyNullFirstLayer) {
  // Ensures that if an iterator is constructed with a nullptr, that it can be
  // iterated without issue and that it remains equal to any other
  // null-initialized iterator.
  LayerListIterator it(nullptr);
  LayerListIterator end(nullptr);

  EXPECT_EQ(it, end);
  ++it;
  EXPECT_EQ(it, end);
}

TEST(LayerListReverseIteratorTest, VerifyTraversalOrder) {
  // Unfortunate preamble.
  FakeLayerTreeHostClient client;
  TestTaskGraphRunner task_graph_runner;
  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  std::unique_ptr<FakeLayerTreeHost> host_ptr = FakeLayerTreeHost::Create(
      &client, &task_graph_runner, animation_host.get());
  FakeLayerTreeHost* host = host_ptr.get();

  // This test constructs the following tree.
  // 1
  // +-2
  // | +-3
  // | +-4
  // + 5
  //   +-6
  //   +-7
  // We expect to visit all seven layers in reverse order.
  scoped_refptr<Layer> layer1 = Layer::Create();
  scoped_refptr<Layer> layer2 = Layer::Create();
  scoped_refptr<Layer> layer3 = Layer::Create();
  scoped_refptr<Layer> layer4 = Layer::Create();
  scoped_refptr<Layer> layer5 = Layer::Create();
  scoped_refptr<Layer> layer6 = Layer::Create();
  scoped_refptr<Layer> layer7 = Layer::Create();

  std::unordered_map<int, int> layer_id_to_order;
  layer_id_to_order[layer1->id()] = 1;
  layer_id_to_order[layer2->id()] = 2;
  layer_id_to_order[layer3->id()] = 3;
  layer_id_to_order[layer4->id()] = 4;
  layer_id_to_order[layer5->id()] = 5;
  layer_id_to_order[layer6->id()] = 6;
  layer_id_to_order[layer7->id()] = 7;

  layer2->AddChild(std::move(layer3));
  layer2->AddChild(std::move(layer4));

  layer5->AddChild(std::move(layer6));
  layer5->AddChild(std::move(layer7));

  layer1->AddChild(std::move(layer2));
  layer1->AddChild(std::move(layer5));

  host->SetRootLayer(std::move(layer1));

  int i = 7;

  for (auto* layer : base::Reversed(*host)) {
    EXPECT_EQ(i--, layer_id_to_order[layer->id()]);
  }

  EXPECT_EQ(0, i);
}

TEST(LayerListReverseIteratorTest, VerifySingleLayer) {
  // Unfortunate preamble.
  FakeLayerTreeHostClient client;
  TestTaskGraphRunner task_graph_runner;
  auto animation_host = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
  std::unique_ptr<FakeLayerTreeHost> host_ptr = FakeLayerTreeHost::Create(
      &client, &task_graph_runner, animation_host.get());
  FakeLayerTreeHost* host = host_ptr.get();

  // This test constructs a tree consisting of a single layer.
  scoped_refptr<Layer> layer1 = Layer::Create();
  std::unordered_map<int, int> layer_id_to_order;
  layer_id_to_order[layer1->id()] = 1;
  host->SetRootLayer(std::move(layer1));

  int i = 1;
  for (auto* layer : base::Reversed(*host)) {
    EXPECT_EQ(i--, layer_id_to_order[layer->id()]);
  }
  EXPECT_EQ(0, i);
}

TEST(LayerListReverseIteratorTest, VerifyNullFirstLayer) {
  // Ensures that if an iterator is constructed with a nullptr, that it can be
  // iterated without issue and that it remains equal to any other
  // null-initialized iterator.
  LayerListReverseIterator it(nullptr);
  LayerListReverseIterator end(nullptr);

  EXPECT_EQ(it, end);
  ++it;
  EXPECT_EQ(it, end);
}

}  // namespace
}  // namespace cc
