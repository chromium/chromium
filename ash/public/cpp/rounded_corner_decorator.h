// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ROUNDED_CORNER_DECORATOR_H_
#define ASH_PUBLIC_CPP_ROUNDED_CORNER_DECORATOR_H_

#include <memory>

#include "ash/public/cpp/ash_public_export.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_observer.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

constexpr int kPipRoundedCornerRadius = 8;

// Applies rounded corners to the given layer, and modifies the shadow of
// the given window to be rounded.
class ASH_PUBLIC_EXPORT RoundedCornerDecorator : public ui::LayerObserver,
                                                 public aura::WindowObserver {
 public:
  RoundedCornerDecorator(aura::Window* shadow_window,
                         aura::Window* layer_window,
                         ui::Layer* layer,
                         int radius);
  ~RoundedCornerDecorator() override;

  // Returns true if the rounded corner decorator is still applied to a valid
  // layer.
  bool IsValid();

  // ui::LayerObserver:
  void LayerDestroyed(ui::Layer* layer) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

 private:
  void Shutdown();

  aura::Window* layer_window_;
  ui::Layer* layer_;
  int radius_;

  DISALLOW_COPY_AND_ASSIGN(RoundedCornerDecorator);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ROUNDED_CORNER_DECORATOR_H_
