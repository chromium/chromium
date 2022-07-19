// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/system_shadow.h"

#include "ash/style/system_shadow_on_nine_patch_layer.h"
#include "base/memory/ptr_util.h"

namespace ash {

SystemShadow::~SystemShadow() = default;

// static
std::unique_ptr<SystemShadow> SystemShadow::CreateShadowOnNinePatchLayer(
    Type shadow_type) {
  int elevation = GetElevationFromType(shadow_type);
  return base::WrapUnique(new SystemShadowOnNinePatchLayerImpl(elevation));
}

// static
std::unique_ptr<SystemShadow> SystemShadow::CreateShadowOnNinePatchLayerForView(
    views::View* view,
    Type shadow_type) {
  DCHECK(view);
  int elevation = GetElevationFromType(shadow_type);
  return base::WrapUnique(
      new SystemViewShadowOnNinePatchLayer(view, elevation));
}

// static
std::unique_ptr<SystemShadow>
SystemShadow::CreateShadowOnNinePatchLayerForWindow(aura::Window* window,
                                                    Type shadow_type) {
  DCHECK(window);
  int elevation = GetElevationFromType(shadow_type);
  return base::WrapUnique(
      new SystemWindowShadowOnNinePatchLayer(window, elevation));
}

// static
int SystemShadow::GetElevationFromType(Type type) {
  switch (type) {
    case Type::kElevation4:
      return 4;
    case Type::kElevation8:
      return 8;
    case Type::kElevation12:
      return 12;
    case Type::kElevation16:
      return 16;
    case Type::kElevation24:
      return 24;
  }
}

}  // namespace ash
