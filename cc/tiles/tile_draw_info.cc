// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/tile_draw_info.h"

#include "base/metrics/histogram_macros.h"
#include "cc/base/math_util.h"

namespace cc {

TileDrawInfo::TileDrawInfo() = default;
TileDrawInfo::~TileDrawInfo() {
  DCHECK(!resource_);
}

void TileDrawInfo::AsValueInto(base::trace_event::TracedValue* state) const {
  state->SetBoolean("is_solid_color", mode_ == SOLID_COLOR_MODE);
  state->SetBoolean("is_transparent",
                    mode_ == SOLID_COLOR_MODE && !SkColorGetA(solid_color_));
}

void TileDrawInfo::SetResource(ResourcePool::InUsePoolResource resource,
                               bool resource_is_checker_imaged,
                               bool is_premultiplied) {
  DCHECK(!resource_);
  DCHECK(resource);

  mode_ = RESOURCE_MODE;
  is_resource_ready_to_draw_ = false;
  resource_is_checker_imaged_ = resource_is_checker_imaged;
  is_premultiplied_ = is_premultiplied;
  resource_ = std::move(resource);
}

const ResourcePool::InUsePoolResource& TileDrawInfo::GetResource() {
  DCHECK_EQ(mode_, RESOURCE_MODE);
  DCHECK(resource_);
  return resource_;
}

ResourcePool::InUsePoolResource TileDrawInfo::TakeResource() {
  DCHECK_EQ(mode_, RESOURCE_MODE);
  DCHECK(resource_);
  is_resource_ready_to_draw_ = false;
  resource_is_checker_imaged_ = false;
  is_premultiplied_ = false;
  return std::move(resource_);
}

}  // namespace cc
