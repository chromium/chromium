// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_COLLECTIONS_H_
#define CC_LAYERS_LAYER_COLLECTIONS_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/memory/ref_counted.h"
#include "cc/cc_export.h"

namespace cc {
class Layer;
class LayerImpl;
class RenderSurfaceImpl;

using LayerList = std::vector<scoped_refptr<Layer>>;
using OwnedLayerImplList = std::vector<std::unique_ptr<LayerImpl>>;
using LayerImplList = std::vector<LayerImpl*>;
using RenderSurfaceList = std::vector<RenderSurfaceImpl*>;
using OwnedLayerImplMap = std::unordered_map<int, std::unique_ptr<LayerImpl>>;
using LayerImplMap = std::unordered_map<int, LayerImpl*>;

}  // namespace cc

#endif  // CC_LAYERS_LAYER_COLLECTIONS_H_
