// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_UI_RESOURCE_LAYER_TREE_HOST_IMPL_H_
#define CC_TEST_FAKE_UI_RESOURCE_LAYER_TREE_HOST_IMPL_H_

#include <unordered_map>

#include "cc/test/fake_layer_tree_host_impl.h"

namespace cc {
class TaskGraphRunner;

class FakeUIResourceLayerTreeHostImpl : public FakeLayerTreeHostImpl {
 public:
  explicit FakeUIResourceLayerTreeHostImpl(
      TaskRunnerProvider* task_runner_provider,
      TaskGraphRunner* task_graph_runner);
  ~FakeUIResourceLayerTreeHostImpl() override;

  void CreateUIResource(UIResourceId uid,
                        const UIResourceBitmap& bitmap) override;

  void DeleteUIResource(UIResourceId uid) override;

  viz::ResourceId ResourceIdForUIResource(UIResourceId uid) const override;

  bool IsUIResourceOpaque(UIResourceId uid) const override;

 private:
  std::unordered_map<UIResourceId, LayerTreeHostImpl::UIResourceData>
      fake_ui_resource_map_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_UI_RESOURCE_LAYER_TREE_HOST_IMPL_H_
