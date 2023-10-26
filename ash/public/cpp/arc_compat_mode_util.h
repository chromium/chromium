// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ARC_COMPAT_MODE_UTIL_H_
#define ASH_PUBLIC_CPP_ARC_COMPAT_MODE_UTIL_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ui/gfx/vector_icon_types.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class Widget;
}  // namespace views

namespace ash {

enum class ResizeCompatMode {
  kPhone,
  kTablet,
  kResizable,
};

namespace compat_mode_util {

ASH_PUBLIC_EXPORT ResizeCompatMode
PredictCurrentMode(const views::Widget* widget);

ASH_PUBLIC_EXPORT ResizeCompatMode
PredictCurrentMode(const aura::Window* window);

// Determines which icon should be associated with the given `ResizeCompatMode`
// state.
ASH_PUBLIC_EXPORT const gfx::VectorIcon& GetIcon(ResizeCompatMode mode);

// Determines what text should be displayed for the given `ResizeCompatMode`
// state.
ASH_PUBLIC_EXPORT std::u16string GetText(ResizeCompatMode mode);

}  // namespace compat_mode_util

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ARC_COMPAT_MODE_UTIL_H_
