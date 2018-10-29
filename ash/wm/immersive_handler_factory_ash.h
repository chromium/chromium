// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_IMMERSIVE_HANDLER_FACTORY_ASH_H_
#define ASH_WM_IMMERSIVE_HANDLER_FACTORY_ASH_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/immersive/immersive_handler_factory.h"
#include "base/macros.h"

namespace ash {

class ASH_EXPORT ImmersiveHandlerFactoryAsh : public ImmersiveHandlerFactory {
 public:
  ImmersiveHandlerFactoryAsh();
  ~ImmersiveHandlerFactoryAsh() override;

  // ImmersiveHandlerFactory:
  std::unique_ptr<ImmersiveGestureHandler> CreateGestureHandler(
      ImmersiveFullscreenController* controller) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImmersiveHandlerFactoryAsh);
};

}  // namespace ash

#endif  // ASH_WM_IMMERSIVE_HANDLER_FACTORY_ASH_H_
