// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_layer_context.h"

namespace cc {

void TestLayerContext::SetVisible(bool visible) {}

void TestLayerContext::UpdateDisplayTreeFrom(
    LayerTreeImpl& tree,
    viz::ClientResourceProvider& resource_provider,
    viz::RasterContextProvider& context_provider) {}

void TestLayerContext::UpdateDisplayTile(
    PictureLayerImpl& layer,
    const Tile& tile,
    viz::ClientResourceProvider& resource_provider,
    viz::RasterContextProvider& context_provider) {}

}  // namespace cc
