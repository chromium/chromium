// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_LAYER_TREE_HOST_DELEGATE_H_
#define CC_TEST_FAKE_LAYER_TREE_HOST_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "cc/test/stub_layer_tree_host_delegate.h"

namespace cc {
class LayerTreeHost;

class FakeLayerTreeHostDelegate : public StubLayerTreeHostDelegate {
 public:
  FakeLayerTreeHostDelegate();
  ~FakeLayerTreeHostDelegate() override;

  // Caller responsible for unsetting this and maintaining the host's lifetime.
  void SetLayerTreeHost(LayerTreeHost* host) { host_ = host; }

  // StubLayerTreeHostDelegate overrides.
  void RequestNewLayerTreeFrameSink() override;
  void DidFailToInitializeLayerTreeFrameSink() override;

  void SetUseSoftwareCompositing(bool sw) { software_comp_ = sw; }

 private:
  bool software_comp_ = true;
  raw_ptr<LayerTreeHost> host_ = nullptr;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_LAYER_TREE_HOST_DELEGATE_H_
