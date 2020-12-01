// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_UTILITY_LAYER_UTIL_H_
#define ASH_UTILITY_LAYER_UTIL_H_

#include <memory>

#include "base/callback.h"

namespace ui {
class Layer;
}

namespace viz {
class CopyOutputResult;
}

namespace ash {
using LayerCopyCallback =
    base::OnceCallback<void(std::unique_ptr<ui::Layer> new_layer)>;

// Creates the new layer using the image in |copy_result|.
std::unique_ptr<ui::Layer> CreateLayerFromCopyOutputResult(
    std::unique_ptr<viz::CopyOutputResult> copy_result);

// Creates a new layer that has a copy of the |layer|'s content. This is an
// async API and a new layer will be passed to the |callback| when copy is done.
void CopyLayerContentToNewLayer(ui::Layer* layer, LayerCopyCallback callback);

}  // namespace ash

#endif  // ASH_UTILITY_LAYER_UTIL_H_
