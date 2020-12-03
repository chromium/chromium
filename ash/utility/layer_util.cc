// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/layer_util.h"

#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/size.h"

namespace ash {
namespace {

void CopyCopyOutputResultToLayer(
    std::unique_ptr<viz::CopyOutputResult> copy_result,
    ui::Layer* target_layer) {
  DCHECK(!copy_result->IsEmpty());
  DCHECK_EQ(copy_result->format(), viz::CopyOutputResult::Format::RGBA_TEXTURE);

  const gfx::Size layer_size = copy_result->size();
  viz::TransferableResource transferable_resource =
      viz::TransferableResource::MakeGL(
          copy_result->GetTextureResult()->mailbox, GL_LINEAR, GL_TEXTURE_2D,
          copy_result->GetTextureResult()->sync_token, layer_size,
          /*is_overlay_candidate=*/false);
  std::unique_ptr<viz::SingleReleaseCallback> release_callback =
      copy_result->TakeTextureOwnership();
  target_layer->SetTransferableResource(
      transferable_resource, std::move(release_callback), layer_size);
}

void CopyToNewLayerOnCopyRequestFinished(
    LayerCopyCallback layer_copy_callback,
    std::unique_ptr<viz::CopyOutputResult> copy_result) {
  if (!copy_result || copy_result->IsEmpty()) {
    std::move(layer_copy_callback).Run(nullptr);
    return;
  }

  auto copy_layer = CreateLayerFromCopyOutputResult(std::move(copy_result));
  std::move(layer_copy_callback).Run(std::move(copy_layer));
}

void CopyToLayerOnCopyRequestFinished(
    GetTargetLayerCallback get_target_layer_callback,
    std::unique_ptr<viz::CopyOutputResult> copy_result) {
  if (get_target_layer_callback.IsCancelled())
    return;

  if (!copy_result || copy_result->IsEmpty())
    return;

  ui::Layer* layer = nullptr;
  std::move(get_target_layer_callback).Run(&layer);
  if (!layer)
    return;

  CopyCopyOutputResultToLayer(std::move(copy_result), layer);
}

}  // namespace

std::unique_ptr<ui::Layer> CreateLayerFromCopyOutputResult(
    std::unique_ptr<viz::CopyOutputResult> copy_result) {
  auto copy_layer = std::make_unique<ui::Layer>();
  CopyCopyOutputResultToLayer(std::move(copy_result), copy_layer.get());
  return copy_layer;
}

void CopyLayerContentToNewLayer(ui::Layer* layer, LayerCopyCallback callback) {
  auto new_callback =
      base::BindOnce(&CopyToNewLayerOnCopyRequestFinished, std::move(callback));
  auto copy_request = std::make_unique<viz::CopyOutputRequest>(
      viz::CopyOutputRequest::ResultFormat::RGBA_TEXTURE,
      std::move(new_callback));
  gfx::Rect bounds(layer->size());
  copy_request->set_area(bounds);
  copy_request->set_result_selection(bounds);
  layer->RequestCopyOfOutput(std::move(copy_request));
}

void CopyLayerContentToLayer(ui::Layer* layer,
                             GetTargetLayerCallback callback) {
  auto new_callback =
      base::BindOnce(&CopyToLayerOnCopyRequestFinished, std::move(callback));
  auto copy_request = std::make_unique<viz::CopyOutputRequest>(
      viz::CopyOutputRequest::ResultFormat::RGBA_TEXTURE,
      std::move(new_callback));
  gfx::Rect bounds(layer->size());
  copy_request->set_area(bounds);
  copy_request->set_result_selection(bounds);
  layer->RequestCopyOfOutput(std::move(copy_request));
}

}  // namespace ash
