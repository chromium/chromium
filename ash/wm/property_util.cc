// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/property_util.h"

#include "services/ws/public/cpp/property_type_converters.h"
#include "services/ws/public/mojom/window_manager.mojom.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/window.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

int64_t GetInitialDisplayId(const InitProperties& properties) {
  auto iter =
      properties.find(ws::mojom::WindowManager::kDisplayId_InitProperty);
  return iter == properties.end() ? display::kInvalidDisplayId
                                  : mojo::ConvertTo<int64_t>(iter->second);
}

bool GetInitialContainerId(const InitProperties& properties,
                           int* container_id) {
  auto iter =
      properties.find(ws::mojom::WindowManager::kContainerId_InitProperty);
  if (iter == properties.end())
    return false;

  *container_id = mojo::ConvertTo<int32_t>(iter->second);
  return true;
}

bool GetInitialBounds(const InitProperties& properties, gfx::Rect* bounds) {
  auto iter = properties.find(ws::mojom::WindowManager::kBounds_InitProperty);
  if (iter == properties.end())
    return false;

  *bounds = mojo::ConvertTo<gfx::Rect>(iter->second);
  return true;
}

bool GetWindowPreferredSize(const InitProperties& properties, gfx::Size* size) {
  auto iter =
      properties.find(ws::mojom::WindowManager::kPreferredSize_Property);
  if (iter == properties.end())
    return false;

  *size = mojo::ConvertTo<gfx::Size>(iter->second);
  return true;
}

bool ShouldRemoveStandardFrame(const InitProperties& properties) {
  auto iter = properties.find(
      ws::mojom::WindowManager::kRemoveStandardFrame_InitProperty);
  return iter != properties.end() && mojo::ConvertTo<bool>(iter->second);
}

void ApplyProperties(
    aura::Window* window,
    aura::PropertyConverter* property_converter,
    const std::map<std::string, std::vector<uint8_t>>& properties) {
  for (auto& property_pair : properties) {
    property_converter->SetPropertyFromTransportValue(
        window, property_pair.first, &property_pair.second);
  }
}

}  // namespace ash
