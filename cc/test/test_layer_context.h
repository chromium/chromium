// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_LAYER_CONTEXT_H_
#define CC_TEST_TEST_LAYER_CONTEXT_H_

#include "cc/trees/layer_context.h"

namespace cc {

class TestLayerContext : public LayerContext {
 public:
  TestLayerContext() = default;
  ~TestLayerContext() override = default;

  void SetVisible(bool visible) override;

  void UpdateDisplayTreeFrom(LayerTreeImpl& tree,
                             viz::ClientResourceProvider& resource_provider,
                             viz::RasterContextProvider& context_provider,
                             const gfx::Rect& viewport_damage_rect) override;

  void UpdateDisplayTile(PictureLayerImpl& layer,
                         const Tile& tile,
                         viz::ClientResourceProvider& resource_provider,
                         viz::RasterContextProvider& context_provider,
                         bool update_damage) override;
};

}  // namespace cc

#endif  // CC_TEST_TEST_LAYER_CONTEXT_H_
