// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/mojo_embedder/viz_layer_context.h"

#include <utility>

#include "base/check.h"
#include "cc/trees/layer_context_client.h"
#include "services/viz/public/mojom/compositing/layer_context.mojom.h"

namespace cc::mojo_embedder {

VizLayerContext::VizLayerContext(viz::mojom::CompositorFrameSink& frame_sink,
                                 cc::LayerContextClient* client)
    : client_(client) {
  CHECK(client_);
  auto context = viz::mojom::PendingLayerContext::New();
  context->receiver = service_.BindNewEndpointAndPassReceiver();
  context->client = client_receiver_.BindNewEndpointAndPassRemote();
  frame_sink.BindLayerContext(std::move(context));
}

VizLayerContext::~VizLayerContext() = default;

void VizLayerContext::SetTargetLocalSurfaceId(const viz::LocalSurfaceId& id) {
  service_->SetTargetLocalSurfaceId(id);
}

void VizLayerContext::SetVisible(bool visible) {
  service_->SetVisible(visible);
}

void VizLayerContext::Commit(const CommitState& state) {
  // TODO(https://crbug.com/1431762): Push actual commit data. For now we only
  // update basic parameters required for any LayerTreeHost drawing.
  auto update = viz::mojom::LayerTreeUpdate::New();
  update->device_viewport = state.device_viewport_rect;
  update->device_scale_factor = state.device_scale_factor;
  update->local_surface_id_from_parent = state.local_surface_id_from_parent;
  service_->Commit(std::move(update));
}

void VizLayerContext::OnRequestCommitForFrame(const viz::BeginFrameArgs& args) {
  client_->OnRequestCommitForFrame(args);
}

}  // namespace cc::mojo_embedder
