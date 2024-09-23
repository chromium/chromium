// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/system_shadow.h"

#include "ash/style/system_shadow_on_nine_patch_layer.h"
#include "ash/style/system_shadow_on_texture_layer.h"
#include "base/memory/ptr_util.h"
#include "ui/color/color_provider.h"

namespace ash {

SystemShadow::~SystemShadow() = default;

// static
std::unique_ptr<SystemShadow> SystemShadow::CreateShadowOnNinePatchLayer(
    Type shadow_type,
    const LayerRecreatedCallback& layer_recreated_callback) {
  return base::WrapUnique(new SystemShadowOnNinePatchLayerImpl(
      shadow_type, layer_recreated_callback));
}

// static
std::unique_ptr<SystemShadow> SystemShadow::CreateShadowOnNinePatchLayerForView(
    views::View* view,
    Type shadow_type) {
  DCHECK(view);
  return base::WrapUnique(
      new SystemViewShadowOnNinePatchLayer(view, shadow_type));
}

// static
std::unique_ptr<SystemShadow>
SystemShadow::CreateShadowOnNinePatchLayerForWindow(aura::Window* window,
                                                    Type shadow_type) {
  DCHECK(window);
  return base::WrapUnique(
      new SystemWindowShadowOnNinePatchLayer(window, shadow_type));
}

// static
std::unique_ptr<SystemShadow> SystemShadow::CreateShadowOnTextureLayer(
    Type shadow_type) {
  return base::WrapUnique(new SystemShadowOnTextureLayer(shadow_type));
}

// static
int SystemShadow::GetElevationFromType(Type type) {
  switch (type) {
    case Type::kElevation4:
      return 4;
    case Type::kElevation12:
      return 12;
    case Type::kElevation24:
      return 24;
  }
}

void SystemShadow::ObserveColorProviderSource(
    ui::ColorProviderSource* color_provider_source) {
  Observe(color_provider_source);
}

void SystemShadow::OnColorProviderChanged() {
  if (auto* color_provider_source = GetColorProviderSource()) {
    UpdateShadowColors(color_provider_source->GetColorProvider());
  }
}

}  // namespace ash
