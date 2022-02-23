// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/system_shadow.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/views/widget/widget.h"

namespace ash {

// static
std::unique_ptr<SystemShadow> SystemShadow::CreateShadowForWidget(
    views::Widget* widget,
    Type shadow_type) {
  DCHECK(widget);
  return CreateShadowForWindow(widget->GetNativeWindow(), shadow_type);
}

// static
std::unique_ptr<SystemShadow> SystemShadow::CreateShadowForWindow(
    aura::Window* window,
    Type shadow_type) {
  DCHECK(window);
  auto shadow = std::make_unique<SystemShadow>(shadow_type);
  auto* shadow_layer = shadow->layer();
  auto* window_layer = window->layer();

  // Add shadow layer to window layer and stack at the bottom.
  window_layer->Add(shadow_layer);
  window_layer->StackAtBottom(shadow_layer);
  return shadow;
}

SystemShadow::SystemShadow(Type type) : type_(type) {
  // Note: Init function should be called before SetShadowStyle.
  Init(GetElevationFromType(type_));
  // System shadow always use `kChromeOSSystemUI` as shadow style.
  SetShadowStyle(gfx::ShadowStyle::kChromeOSSystemUI);
}

SystemShadow::~SystemShadow() = default;

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

void SystemShadow::SetType(Type type) {
  if (type_ == type)
    return;

  type_ = type;
  SetElevation(GetElevationFromType(type_));
}

}  // namespace ash
