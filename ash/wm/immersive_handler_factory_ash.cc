// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/immersive_handler_factory_ash.h"

#include <memory>

#include "ash/wm/immersive_gesture_handler_classic.h"

namespace ash {

ImmersiveHandlerFactoryAsh::ImmersiveHandlerFactoryAsh() = default;

ImmersiveHandlerFactoryAsh::~ImmersiveHandlerFactoryAsh() = default;

std::unique_ptr<ImmersiveGestureHandler>
ImmersiveHandlerFactoryAsh::CreateGestureHandler(
    ImmersiveFullscreenController* controller) {
  return std::make_unique<ImmersiveGestureHandlerClassic>(controller);
}

}  // namespace ash
