// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/tile_based_layer_impl.h"

namespace cc {

TileBasedLayerImpl::TileBasedLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id) {}

TileBasedLayerImpl::~TileBasedLayerImpl() = default;

}  // namespace cc
