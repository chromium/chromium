// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/tile_display_layer_impl.h"

#include "base/check_deref.h"
#include "cc/layers/append_quads_context.h"
#include "cc/layers/append_quads_data.h"
#include "cc/test/test_layer_tree_host_base.h"

namespace cc {

class TileDisplayLayerImplTest : public TestLayerTreeHostBase {};

TEST_F(TileDisplayLayerImplTest, NoQuadAppendedByDefault) {
  TileDisplayLayerImpl layer(CHECK_DEREF(host_impl()->active_tree()),
                             /*id=*/42);

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  layer.AppendQuads(
      AppendQuadsContext{DRAW_MODE_RESOURCELESS_SOFTWARE, {}, false},
      render_pass.get(), &data);

  EXPECT_EQ(render_pass->quad_list.size(), 0u);
}

}  // namespace cc
