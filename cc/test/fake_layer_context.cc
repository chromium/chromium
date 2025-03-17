// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_layer_context.h"

namespace cc {

void FakeLayerContext::SetVisible(bool visible) {}

void FakeLayerContext::UpdateDisplayTreeFrom(
    LayerTreeImpl& tree,
    viz::ClientResourceProvider& resource_provider,
    viz::RasterContextProvider& context_provider) {}

void FakeLayerContext::UpdateDisplayTile(
    PictureLayerImpl& layer,
    const Tile& tile,
    viz::ClientResourceProvider& resource_provider,
    viz::RasterContextProvider& context_provider) {}

}  // namespace cc
