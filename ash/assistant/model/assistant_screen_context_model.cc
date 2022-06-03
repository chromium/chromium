// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_screen_context_model.h"

namespace ash {

AssistantStructureFuture::AssistantStructureFuture() = default;

AssistantStructureFuture::~AssistantStructureFuture() = default;

void AssistantStructureFuture::SetValue(
    ax::mojom::AssistantStructurePtr structure) {
  DCHECK(!HasValue());
  structure_ = std::move(structure);
  Notify();
}

void AssistantStructureFuture::GetValueAsync(Callback callback) {
  if (HasValue()) {
    RunCallback(std::move(callback));
    return;
  }
  callbacks_.push_back(std::move(callback));
}

bool AssistantStructureFuture::HasValue() const {
  return !structure_.is_null();
}

void AssistantStructureFuture::Clear() {
  structure_.reset();
}

void AssistantStructureFuture::Notify() {
  for (auto& callback : callbacks_)
    RunCallback(std::move(callback));

  callbacks_.clear();
}

void AssistantStructureFuture::RunCallback(Callback callback) {
  std::move(callback).Run(*structure_);
}

AssistantScreenContextModel::AssistantScreenContextModel() = default;

AssistantScreenContextModel::~AssistantScreenContextModel() = default;

void AssistantScreenContextModel::Clear() {
  assistant_structure_.Clear();
}

}  // namespace ash
