// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ROUNDED_CORNER_UTILS_H_
#define ASH_PUBLIC_CPP_ROUNDED_CORNER_UTILS_H_

#include "ash/public/cpp/ash_public_export.h"

namespace aura {
class Window;
}

namespace ui {
class Layer;
}

namespace ash {

// Puts rounded corners with |radius| on |layer|, and on |shadow_window|'s
// shadow if it has one. Enables fast rounded corners on |layer|.
ASH_PUBLIC_EXPORT void SetCornerRadius(aura::Window* shadow_window,
                                       ui::Layer* layer,
                                       int radius);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ROUNDED_CORNER_UTILS_H_
