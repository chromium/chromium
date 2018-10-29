// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_IMMERSIVE_IMMERSIVE_HANDLER_FACTORY_H_
#define ASH_PUBLIC_CPP_IMMERSIVE_IMMERSIVE_HANDLER_FACTORY_H_

#include <memory>

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

class ImmersiveFullscreenController;
class ImmersiveGestureHandler;

// Used by ImmersiveFullscreenController to create event handlers/watchers.
class ASH_PUBLIC_EXPORT ImmersiveHandlerFactory {
 public:
  static ImmersiveHandlerFactory* Get() { return instance_; }

  virtual std::unique_ptr<ImmersiveGestureHandler> CreateGestureHandler(
      ImmersiveFullscreenController* controller) = 0;

 protected:
  ImmersiveHandlerFactory();
  virtual ~ImmersiveHandlerFactory();

 private:
  static ImmersiveHandlerFactory* instance_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_IMMERSIVE_IMMERSIVE_HANDLER_FACTORY_H_
