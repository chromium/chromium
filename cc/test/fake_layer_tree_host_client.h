// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_LAYER_TREE_HOST_CLIENT_H_
#define CC_TEST_FAKE_LAYER_TREE_HOST_CLIENT_H_

#include "cc/test/stub_layer_tree_host_client.h"

namespace cc {
class LayerTreeHost;

class FakeLayerTreeHostClient : public StubLayerTreeHostClient {
 public:
  FakeLayerTreeHostClient();
  ~FakeLayerTreeHostClient() override;

  // Caller responsible for unsetting this and maintaining the host's lifetime.
  void SetLayerTreeHost(LayerTreeHost* host) { host_ = host; }

  // StubLayerTreeHostClient overrides.
  void RequestNewLayerTreeFrameSink() override;
  void DidFailToInitializeLayerTreeFrameSink() override;

  void SetUseSoftwareCompositing(bool sw) { software_comp_ = sw; }

 private:
  bool software_comp_ = true;
  LayerTreeHost* host_ = nullptr;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_LAYER_TREE_HOST_CLIENT_H_
