// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_UTILITY_LAYER_UTIL_H_
#define ASH_UTILITY_LAYER_UTIL_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/functional/callback.h"

namespace ui {
class Layer;
}

namespace gfx {
class Size;
}

namespace viz {
class CopyOutputResult;
}

namespace ash {

// Creates the new layer with |layer_size| using the image in |copy_result|.
ASH_EXPORT std::unique_ptr<ui::Layer> CreateLayerFromCopyOutputResult(
    std::unique_ptr<viz::CopyOutputResult> copy_result,
    const gfx::Size& layer_size);

using LayerCopyCallback =
    base::OnceCallback<void(std::unique_ptr<ui::Layer> new_layer)>;

// Creates a new layer that has a copy of the |layer|'s content. This is an
// async API and a new layer will be passed to the |callback| when copy is done.
ASH_EXPORT void CopyLayerContentToNewLayer(ui::Layer* layer,
                                           LayerCopyCallback callback);

using GetTargetLayerCallback = base::OnceCallback<void(ui::Layer**)>;

// Copy the content of |original_layer| to a new layer given via |callback|.
// This is an async API and |callback| is called when the copy result is ready.
ASH_EXPORT void CopyLayerContentToLayer(ui::Layer* original_layer,
                                        GetTargetLayerCallback callback);

}  // namespace ash

#endif  // ASH_UTILITY_LAYER_UTIL_H_
