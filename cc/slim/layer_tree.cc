// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/slim/layer_tree.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "cc/slim/constants.h"
#include "cc/slim/features.h"
#include "cc/slim/layer_tree_cc_wrapper.h"
#include "cc/slim/layer_tree_impl.h"

namespace cc::slim {

// static
std::unique_ptr<LayerTree> LayerTree::Create(InitParams params) {
  if (!features::IsSlimCompositorEnabled()) {
    return base::WrapUnique<LayerTree>(
        new LayerTreeCcWrapper(std::move(params)));
  }
  return base::WrapUnique<LayerTree>(
      new LayerTreeImpl(params.client, kNumUnneededBeginFrameBeforeStop,
                        kMinimumOcclusionTrackingDimension));
}

LayerTree::InitParams::InitParams() = default;
LayerTree::InitParams::~InitParams() = default;
LayerTree::InitParams::InitParams(InitParams&&) = default;
LayerTree::InitParams& LayerTree::InitParams::operator=(InitParams&&) = default;

}  // namespace cc::slim
