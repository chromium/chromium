// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/shared_bitmap_id_registrar.h"

#include <utility>

#include "cc/layers/texture_layer.h"

namespace cc {

SharedBitmapIdRegistration::SharedBitmapIdRegistration() = default;

SharedBitmapIdRegistration::SharedBitmapIdRegistration(
    base::WeakPtr<TextureLayer> layer_ptr,
    const viz::SharedBitmapId& id)
    : layer_ptr_(std::move(layer_ptr)), id_(id) {}

SharedBitmapIdRegistration::~SharedBitmapIdRegistration() {
  if (layer_ptr_)
    layer_ptr_->UnregisterSharedBitmapId(id_);
}

SharedBitmapIdRegistration::SharedBitmapIdRegistration(
    SharedBitmapIdRegistration&&) noexcept = default;

SharedBitmapIdRegistration& SharedBitmapIdRegistration::operator=(
    SharedBitmapIdRegistration&& other) noexcept {
  if (layer_ptr_)
    layer_ptr_->UnregisterSharedBitmapId(id_);
  layer_ptr_ = std::move(other.layer_ptr_);
  id_ = std::move(other.id_);
  return *this;
}

}  // namespace cc
