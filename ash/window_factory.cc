// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/window_factory.h"

#include "ui/aura/window.h"

namespace ash {
namespace window_factory {

std::unique_ptr<aura::Window> NewWindow(aura::WindowDelegate* delegate,
                                        aura::client::WindowType type) {
  return std::make_unique<aura::Window>(delegate, type);
}

}  // namespace window_factory
}  // namespace ash
