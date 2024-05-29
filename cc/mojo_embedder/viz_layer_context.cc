// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/mojo_embedder/viz_layer_context.h"

#include <utility>

#include "cc/trees/layer_tree_impl.h"
#include "services/viz/public/mojom/compositing/layer_context.mojom.h"

namespace cc::mojo_embedder {

VizLayerContext::VizLayerContext(viz::mojom::CompositorFrameSink& frame_sink) {
  auto context = viz::mojom::PendingLayerContext::New();
  context->receiver = service_.BindNewEndpointAndPassReceiver();
  context->client = client_receiver_.BindNewEndpointAndPassRemote();
  frame_sink.BindLayerContext(std::move(context));
}

VizLayerContext::~VizLayerContext() = default;

void VizLayerContext::SetVisible(bool visible) {
  service_->SetVisible(visible);
}

void VizLayerContext::UpdateDisplayTreeFrom(LayerTreeImpl& tree) {
  // TODO(crbug.com/40902503): Push actual tree updates.
  auto update = viz::mojom::LayerTreeUpdate::New();
  update->local_surface_id_from_parent = tree.local_surface_id_from_parent();
  service_->Commit(std::move(update));
}

void VizLayerContext::OnRequestCommitForFrame(const viz::BeginFrameArgs& args) {
}

}  // namespace cc::mojo_embedder
